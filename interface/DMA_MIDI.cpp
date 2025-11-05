#include "DMA_MIDI.h"

DMA_MIDI::DMA_MIDI(uart_inst_t* uart_instance, uint tx_pin, uint rx_pin)
    : uart(uart_instance), tx_pin(tx_pin), rx_pin(rx_pin),
      rx_dma_channel(-1), tx_dma_channel(-1), tx_control_channel(-1),
      tx_write_pos(0), tx_read_pos(0), rx_read_pos(0),
      rx_bytes_processed(0), tx_bytes_sent(0),
      parser_state(0), parser_status(0), parser_index(0), running_status(0),
      noteOnCallback(nullptr), noteOffCallback(nullptr), controlChangeCallback(nullptr) {
    memset(parser_data, 0, sizeof(parser_data));
}

DMA_MIDI::~DMA_MIDI() {
    cancel_repeating_timer(&poll_timer);
    if (rx_dma_channel >= 0) {
        dma_channel_abort(rx_dma_channel);
        dma_channel_unclaim(rx_dma_channel);
    }
    if (tx_dma_channel >= 0) {
        dma_channel_abort(tx_dma_channel);
        dma_channel_unclaim(tx_dma_channel);
    }
    if (tx_control_channel >= 0) {
        dma_channel_unclaim(tx_control_channel);
    }
}

bool DMA_MIDI::begin() {
    while(!Serial) {}
    // Initialize UART
    uint actual_baud = uart_init(uart, MIDI_BAUD_RATE);
    if (actual_baud == 0) {
        return false;  // UART init failed
    }

    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);

    // Set UART format: 8 data bits, 1 stop bit, no parity
    uart_set_format(uart, 8, 1, UART_PARITY_NONE);

    // Enable UART FIFO (DMA will handle transfers)
    uart_set_fifo_enabled(uart, true);

    // Setup DMA channels (with error checking)
    if (!setupRxDMA()) {
        return false;
    }
    if (!setupTxDMA()) {
        // Clean up RX DMA on failure
        if (rx_dma_channel >= 0) {
            dma_channel_abort(rx_dma_channel);
            dma_channel_unclaim(rx_dma_channel);
            rx_dma_channel = -1;
        }
        return false;
    }

    // Start periodic polling timer
    if (!add_repeating_timer_us(-MIDI_POLL_INTERVAL_US, timerCallback, this, &poll_timer)) {
        // Clean up on timer failure
        if (rx_dma_channel >= 0) {
            dma_channel_abort(rx_dma_channel);
            dma_channel_unclaim(rx_dma_channel);
            rx_dma_channel = -1;
        }
        if (tx_dma_channel >= 0) {
            dma_channel_abort(tx_dma_channel);
            dma_channel_unclaim(tx_dma_channel);
            tx_dma_channel = -1;
        }
        if (tx_control_channel >= 0) {
            dma_channel_unclaim(tx_control_channel);
            tx_control_channel = -1;
        }
        return false;
    }

    return true;
}

bool DMA_MIDI::setupRxDMA() {
    // Claim DMA channel for RX (don't panic on failure)
    rx_dma_channel = dma_claim_unused_channel(false);
    if (rx_dma_channel < 0) {
        return false;  // No DMA channel available
    }

    // Configure RX DMA with ring buffer
    dma_channel_config c = dma_channel_get_default_config(rx_dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, false);  // Always read from UART DR
    channel_config_set_write_increment(&c, true);  // Increment write address
    channel_config_set_dreq(&c, uart_get_dreq(uart, false));  // UART RX DREQ

    // Enable ring buffer on write (wraps at RX_BUFFER_SIZE)
    uint32_t ring_size = 0;
    uint32_t size = RX_BUFFER_SIZE;
    while (size >>= 1) ring_size++;
    channel_config_set_ring(&c, true, ring_size);  // true = wrap on write

    // Start the DMA channel
    dma_channel_configure(
        rx_dma_channel,
        &c,
        rx_buffer,                      // Write to rx_buffer
        &uart_get_hw(uart)->dr,        // Read from UART data register
        0xFFFFFFFF,                     // Transfer count (infinite with ring)
        true                            // Start immediately
    );

    return true;
}

bool DMA_MIDI::setupTxDMA() {
    // Claim DMA channel for TX data (don't panic on failure)
    tx_dma_channel = dma_claim_unused_channel(false);
    if (tx_dma_channel < 0) {
        return false;  // No DMA channel available
    }

    // Claim DMA channel for TX control (don't panic on failure)
    tx_control_channel = dma_claim_unused_channel(false);
    if (tx_control_channel < 0) {
        // Clean up tx_dma_channel on failure
        dma_channel_unclaim(tx_dma_channel);
        tx_dma_channel = -1;
        return false;
    }

    // Configure TX data channel
    dma_channel_config c = dma_channel_get_default_config(tx_dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);   // Increment read address
    channel_config_set_write_increment(&c, false); // Always write to UART DR
    channel_config_set_dreq(&c, uart_get_dreq(uart, true));  // UART TX DREQ

    dma_channel_configure(
        tx_dma_channel,
        &c,
        &uart_get_hw(uart)->dr,        // Write to UART data register
        NULL,                           // Read address set later
        0,                              // Transfer count set later
        false                           // Don't start yet
    );

    return true;
}

void DMA_MIDI::process() {
    processRxBuffer();
}

uint32_t DMA_MIDI::getRxWritePos() {
    // Calculate current write position from DMA transfer count
    uint32_t remaining = dma_channel_hw_addr(rx_dma_channel)->transfer_count;
    return (0xFFFFFFFF - remaining) & (RX_BUFFER_SIZE - 1);
}

void DMA_MIDI::processRxBuffer() {
    uint32_t write_pos = getRxWritePos();
    
    // Process all available bytes
    while (rx_read_pos != write_pos) {
        uint8_t byte = rx_buffer[rx_read_pos];
        rx_read_pos = (rx_read_pos + 1) & (RX_BUFFER_SIZE - 1);
        rx_bytes_processed++;
        
        processMidiByte(byte);
    }
}

void DMA_MIDI::processMidiByte(uint8_t byte) {
    // Check if this is a status byte
    if (byte & 0x80) {
        // System Real-Time messages (single byte, can interrupt other messages)
        if (byte >= 0xF8) {
            // Ignore system real-time for now (Clock, Start, Stop, etc.)
            return;
        }
        
        // System Common messages or Channel Voice messages
        parser_status = byte;
        parser_index = 0;
        
        // Determine expected data bytes
        uint8_t msg_type = byte & 0xF0;
        
        if (msg_type == 0xF0) {
            // System messages - ignore SysEx for this implementation
            if (byte == 0xF0) {
                parser_state = 0xFF;  // Ignore until 0xF7
            }
            return;
        }
        
        // Channel voice message
        running_status = parser_status;
        
    } else if (parser_status == 0 && running_status != 0) {
        // Use running status
        parser_status = running_status;
        parser_index = 0;
        // Process this data byte
    }
    
    // Ignore if in SysEx ignore mode
    if (parser_state == 0xFF) {
        if (byte == 0xF7) parser_state = 0;
        return;
    }
    
    // Process data bytes
    if (!(byte & 0x80) && parser_status != 0) {
        uint8_t msg_type = parser_status & 0xF0;
        
        // Determine how many data bytes we need
        uint8_t expected_bytes = 2;
        if (msg_type == MIDI_PROGRAM_CHANGE || msg_type == MIDI_CHANNEL_AFTERTOUCH) {
            expected_bytes = 1;
        }
        
        parser_data[parser_index++] = byte;
        
        if (parser_index >= expected_bytes) {
            // Complete message received
            MidiMessage msg;
            msg.status = parser_status;
            msg.channel = (parser_status & 0x0F) + 1;  // 1-16
            msg.type = msg_type;
            msg.data1 = parser_data[0];
            msg.data2 = (expected_bytes > 1) ? parser_data[1] : 0;
            msg.valid = true;
            
            handleMidiMessage(msg);
            
            // Reset for next message (but keep running status)
            parser_index = 0;
        }
    }
}

void DMA_MIDI::handleMidiMessage(const MidiMessage& msg) {
    switch (msg.type) {
        case MIDI_NOTE_ON:
            if (msg.data2 == 0) {
                // Velocity 0 = Note Off
                if (noteOffCallback) {
                    noteOffCallback(msg.channel, msg.data1, 0);
                }
            } else {
                if (noteOnCallback) {
                    noteOnCallback(msg.channel, msg.data1, msg.data2);
                }
            }
            break;
            
        case MIDI_NOTE_OFF:
            if (noteOffCallback) {
                noteOffCallback(msg.channel, msg.data1, msg.data2);
            }
            break;
            
        case MIDI_CONTROL_CHANGE:
            if (controlChangeCallback) {
                controlChangeCallback(msg.channel, msg.data1, msg.data2);
            }
            break;
            
        // Add more message types as needed
    }
}

void DMA_MIDI::sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    if (note > 127 || velocity > 127) return;
    
    queueTxByte(MIDI_NOTE_ON | ((channel - 1) & 0x0F));
    queueTxByte(note);
    queueTxByte(velocity);
}

void DMA_MIDI::sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    if (note > 127 || velocity > 127) return;
    
    queueTxByte(MIDI_NOTE_OFF | ((channel - 1) & 0x0F));
    queueTxByte(note);
    queueTxByte(velocity);
}

void DMA_MIDI::sendControlChange(uint8_t control, uint8_t value, uint8_t channel) {
    if (channel < 1 || channel > 16) return;
    if (control > 127 || value > 127) return;
    
    queueTxByte(MIDI_CONTROL_CHANGE | ((channel - 1) & 0x0F));
    queueTxByte(control);
    queueTxByte(value);
}

void DMA_MIDI::queueTxByte(uint8_t byte) {
    // Add byte to circular buffer
    uint32_t next_pos = (tx_write_pos + 1) & (TX_BUFFER_SIZE - 1);
    
    // Wait if buffer is full
    while (next_pos == tx_read_pos) {
        tight_loop_contents();
    }
    
    tx_buffer[tx_write_pos] = byte;
    tx_write_pos = next_pos;
    tx_bytes_sent++;
    
    // Start DMA if not already transmitting
    if (!dma_channel_is_busy(tx_dma_channel)) {
        startTxDMA();
    }
}

void DMA_MIDI::startTxDMA() {
    if (tx_read_pos == tx_write_pos) return;  // Nothing to send
    
    // Calculate bytes to send
    uint32_t bytes_to_send;
    if (tx_write_pos > tx_read_pos) {
        bytes_to_send = tx_write_pos - tx_read_pos;
    } else {
        // Wrapped: send to end of buffer first
        bytes_to_send = TX_BUFFER_SIZE - tx_read_pos;
    }
    
    // Configure and start DMA transfer
    dma_channel_set_read_addr(tx_dma_channel, &tx_buffer[tx_read_pos], false);
    dma_channel_set_trans_count(tx_dma_channel, bytes_to_send, true);
    
    // Update read position
    tx_read_pos = (tx_read_pos + bytes_to_send) & (TX_BUFFER_SIZE - 1);
}

bool DMA_MIDI::timerCallback(repeating_timer_t *rt) {
    DMA_MIDI* midi = (DMA_MIDI*)rt->user_data;
    
    // Process RX buffer
    midi->processRxBuffer();
    
    // Check if TX DMA finished and more data to send
    if (!dma_channel_is_busy(midi->tx_dma_channel)) {
        if (midi->tx_read_pos != midi->tx_write_pos) {
            midi->startTxDMA();
        }
    }
    
    return true;  // Keep repeating
}
