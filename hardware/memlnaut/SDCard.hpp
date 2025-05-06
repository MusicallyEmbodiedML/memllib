#ifndef __SD_CARD_HPP__
#define __SD_CARD_HPP__

#include <cstddef>
#include <vector>
#include <functional>

class SDCard {
public:
    using CardEventCallback = std::function<void(bool)>;  // Callback type for card events (inserted/removed)
    
    enum class CardType {
        UNKNOWN = -1,
        SD1 = 0,
        SD2 = 1, 
        SDHC_SDXC = 3
    };

    struct CardInfo {
        CardType type;
        uint32_t clusterSize;
        uint32_t blocksPerCluster;
        uint32_t blockSize;
        uint64_t totalBytes;
        uint64_t usedBytes;
        uint8_t fatType;
    };

    /**
     * @brief Constructor initialises the hardware SPI interface
     * @param miso MISO pin number
     * @param mosi MOSI pin number
     * @param cs CS pin number  
     * @param sck SCK pin number
     */
    SDCard(int miso = 12, int mosi = 15, int cs = 13, int sck = 14);

    /**
     * @brief Set callback for card insertion/removal events
     * @param cb Callback function receiving bool (true=inserted, false=removed)
     */
    void setCardEventCallback(CardEventCallback cb) { cardEventCb_ = cb; }

    /**
     * @brief Tries to read an (presumably) inserted SD card
     * @return true Read successfully
     * @return false Failed to read
     */
    bool init();

    /**
     * @brief Get current card information
     * @return CardInfo struct with card details
     */  
    CardInfo getCardInfo() const;

    /**
     * @brief Check if card is currently available
     * @return true if card is inserted and initialized
     */
    bool isCardPresent() const { return cardPresent_; }

    /**
     * @brief Creates directory structure if it doesn't exist
     * @param path Directory path to create
     * @return true if successful or already exists
     */
    bool ensureDirectory(const char* path);

    // Your existing file operations
    bool write(const char* filename, const std::vector<char> &data);
    bool read(const char* filename, std::vector<char> &data, size_t size = 0);
    bool touchFile(const char* filename);

    /** 
     * @brief Check card status and trigger callbacks if changed
     * Should be called periodically from main loop
     */
    void update();

private:
    const int miso_;
    const int mosi_;
    const int cs_;
    const int sck_;
    bool cardPresent_;
    CardEventCallback cardEventCb_;

    void initSPI();
    bool detectCardChange();
};

#endif  // __SD_CARD_HPP__