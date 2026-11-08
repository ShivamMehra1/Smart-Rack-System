#include "RFIDManager.h"
#include <Wire.h>
#if defined(RFID_READER_MFRC522) || defined(RFID_READER_PN532_SPI)
#include <SPI.h>
#endif

RFIDManager::RFIDManager()
#if defined(RFID_READER_PN532_I2C)
    : reader(PIN_PN532_IRQ, PIN_PN532_RESET, &Wire),
      cardLatched(false),
#elif defined(RFID_READER_PN532_SPI)
    : reader(PIN_PN532_SPI_SS, &SPI),
      cardLatched(false),
#else
    : reader(PIN_RFID_SS, PIN_RFID_RST),
#endif
      currentMode(MODE_NORMAL_ALLOCATION),
      onCardScanned(nullptr),
      lastScanTime(0),
      lastScannedUID(""),
      buzzerOffAt(0),
      ready(false) {}

const char* RFIDManager::readerName() const {
#if defined(RFID_READER_PN532_I2C)
    return "PN532 · I2C";
#elif defined(RFID_READER_PN532_SPI)
    return "PN532 · SPI";
#else
    return "RC522 · SPI";
#endif
}

void RFIDManager::begin(RFIDCallback callback) {
    onCardScanned = callback;
#if defined(RFID_READER_PN532_I2C)
    reader.begin();
    const uint32_t version = reader.getFirmwareVersion();
    ready = version != 0;
    if (isReady()) {
        reader.SAMConfig();
        reader.setPassiveActivationRetries(0x01);
        Serial.printf("[RFID] PN532 ready, firmware 0x%08lX.\n", (unsigned long)version);
    } else {
        Serial.println("[RFID] PN532 was not found on the shared I2C bus.");
    }
#elif defined(RFID_READER_PN532_SPI)
    SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_PN532_SPI_SS);
    reader.begin();
    const uint32_t version = reader.getFirmwareVersion();
    ready = version != 0;
    if (isReady()) {
        reader.SAMConfig();
        reader.setPassiveActivationRetries(0x01);
        Serial.printf("[RFID] PN532 ready over SPI, firmware 0x%08lX.\n", (unsigned long)version);
    } else {
        Serial.println("[RFID] PN532 was not found over SPI.");
    }
#else
    SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
    reader.PCD_Init();
    delay(4);
    const byte version = reader.PCD_ReadRegister(0x37);
    ready = version != 0x00 && version != 0xFF;
    Serial.printf("[RFID] MFRC522 %s (version 0x%02X).\n", isReady() ? "ready" : "not responding", version);
#endif
}

void RFIDManager::dispatchScan(const String &uid) {
    const unsigned long now = millis();
    if (uid == lastScannedUID && now - lastScanTime < SCAN_COOLDOWN_MS) return;

    lastScannedUID = uid;
    lastScanTime = now;
    if (PIN_BUZZER >= 0) {
        digitalWrite(PIN_BUZZER, HIGH);
        buzzerOffAt = now + 45;
    }
    if (onCardScanned) onCardScanned(uid, getMode());
}

void RFIDManager::loop() {
    const unsigned long now = millis();
    if (buzzerOffAt && (long)(now - buzzerOffAt) >= 0) {
        digitalWrite(PIN_BUZZER, LOW);
        buzzerOffAt = 0;
    }
    if (!isReady()) return;

#if defined(RFID_READER_PN532_I2C) || defined(RFID_READER_PN532_SPI)
    uint8_t uidBytes[7] = {0};
    uint8_t uidLength = 0;
    // Give the PN532 enough time to acknowledge and complete a poll. This can
    // wait up to one second, but it runs on the dedicated Core 0 reader task.
    if (!reader.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidBytes, &uidLength, 1000)) {
        cardLatched = false;
        return;
    }
    if (cardLatched) return;

    String uid;
    for (uint8_t i = 0; i < uidLength; ++i) {
        if (uidBytes[i] < 0x10) uid += '0';
        uid += String(uidBytes[i], HEX);
    }
    uid.toUpperCase();
    cardLatched = true;
    dispatchScan(uid);
#else
    if (!reader.PICC_IsNewCardPresent() || !reader.PICC_ReadCardSerial()) return;
    String uid;
    for (byte i = 0; i < reader.uid.size; ++i) {
        if (reader.uid.uidByte[i] < 0x10) uid += '0';
        uid += String(reader.uid.uidByte[i], HEX);
    }
    uid.toUpperCase();
    dispatchScan(uid);
    reader.PICC_HaltA();
    reader.PCD_StopCrypto1();
#endif
}
