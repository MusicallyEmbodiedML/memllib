#ifndef DMA_MIDI_H
#define DMA_MIDI_H

#include <Arduino.h>
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "pico/time.h"

// MIDI Configuration
#define MIDI_BAUD_RATE 31250
#define RX_BUFFER_SIZE 512  // Must be power of 2 for ring buffer
#define TX_BUFFER_SIZE 512  // Must be power of 2
#define MIDI_POLL_INTERVAL_US 500  // Check DMA buffer every 500us

// MIDI Message Types
#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON 0x90
#define MIDI_POLY_AFTERTOUCH 0xA0
#define MIDI_CONTROL_CHANGE 0xB0
#define MIDI_PROGRAM_CHANGE 0xC0
#define MIDI_CHANNEL_AFTERTOUCH 0xD0
#define MIDI_PITCH_BEND 0xE0
#define MIDI_SYSTEM 0xF0

// MIDI Message Structure
struct MidiMessage {
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    uint8_t channel;
    uint8_t type;
    bool valid;
};

// Callback function types
typedef void (*NoteOnCallback)(uint8_t channel, uint8_t note, uint8_t velocity);
typedef void (*NoteOffCallback)(uint8_t channel, uint8_t note, uint8_t velocity);
typedef void (*ControlChangeCallback)(uint8_t channel, uint8_t control, uint8_t value);

class DMA_MIDI {
public:
    DMA_MIDI(uart_inst_t* uart_instance = uart0, uint tx_pin = 0, uint rx_pin = 1);
    ~DMA_MIDI();
    
    // Initialize MIDI with DMA
    bool begin();
    
    // Register callbacks
    void setHandleNoteOn(NoteOnCallback cb) { noteOnCallback = cb; }
    void setHandleNoteOff(NoteOffCallback cb) { noteOffCallback = cb; }
    void setHandleControlChange(ControlChangeCallback cb) { controlChangeCallback = cb; }
    
    // Send MIDI messages
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);
    
    // Process incoming MIDI (call frequently)
    void process();
    
    // Status
    uint32_t getRxBytesProcessed() { return rx_bytes_processed; }
    uint32_t getTxBytesSent() { return tx_bytes_sent; }
    bool isTransmitting() { return dma_channel_is_busy(tx_dma_channel); }
    
private:
    // UART configuration
    uart_inst_t* uart;
    uint tx_pin;
    uint rx_pin;
    
    // DMA channels
    int rx_dma_channel;
    int tx_dma_channel;
    int tx_control_channel;
    
    // Buffers
    uint8_t rx_buffer[RX_BUFFER_SIZE] __attribute__((aligned(RX_BUFFER_SIZE)));
    uint8_t tx_buffer[TX_BUFFER_SIZE] __attribute__((aligned(TX_BUFFER_SIZE)));
    volatile uint32_t tx_write_pos;
    volatile uint32_t tx_read_pos;
    
    // RX tracking
    uint32_t rx_read_pos;
    uint32_t rx_bytes_processed;
    uint32_t tx_bytes_sent;
    
    // MIDI parser state
    uint8_t parser_state;
    uint8_t parser_status;
    uint8_t parser_data[2];
    uint8_t parser_index;
    uint8_t running_status;
    
    // Callbacks
    NoteOnCallback noteOnCallback;
    NoteOffCallback noteOffCallback;
    ControlChangeCallback controlChangeCallback;
    
    // Timer
    repeating_timer_t poll_timer;
    
    // Internal methods
    bool setupRxDMA();
    bool setupTxDMA();
    void processRxBuffer();
    void processMidiByte(uint8_t byte);
    void handleMidiMessage(const MidiMessage& msg);
    void queueTxByte(uint8_t byte);
    void startTxDMA();
    uint32_t getRxWritePos();
    
    // Static timer callback
    static bool timerCallback(repeating_timer_t *rt);
};

#endif // DMA_MIDI_H
