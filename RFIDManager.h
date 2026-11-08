#pragma once
#include "Config.h"
#include <atomic>

#if defined(RFID_READER_PN532_I2C) || defined(RFID_READER_PN532_SPI)
#include <Adafruit_PN532.h>
#elif defined(RFID_READER_MFRC522)
#include <MFRC522.h>
#else
#error "Select RFID_READER_MFRC522, RFID_READER_PN532_I2C, or RFID_READER_PN532_SPI."
#endif

enum RFIDMode {
    MODE_NORMAL_ALLOCATION,
    MODE_ADMIN_REGISTRATION,
    MODE_CARD_REPLACEMENT
};

typedef void (*RFIDCallback)(const String &uid, RFIDMode mode);

class RFIDManager {
private:
#if defined(RFID_READER_PN532_I2C) || defined(RFID_READER_PN532_SPI)
    Adafruit_PN532 reader;
    bool cardLatched;
#else
    MFRC522 reader;
#endif
    std::atomic<RFIDMode> currentMode;
    RFIDCallback onCardScanned;
    unsigned long lastScanTime;
    String lastScannedUID;
    unsigned long buzzerOffAt;
    std::atomic<bool> ready;
    static const unsigned long SCAN_COOLDOWN_MS = 900;

    void dispatchScan(const String &uid);

public:
    RFIDManager();
    void begin(RFIDCallback callback);
    void loop();
    void setMode(RFIDMode mode) { currentMode.store(mode, std::memory_order_relaxed); }
    RFIDMode getMode() const { return currentMode.load(std::memory_order_relaxed); }
    bool isReady() const { return ready.load(std::memory_order_relaxed); }
    const char* readerName() const;
};
