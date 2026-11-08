#include "LCDManager.h"
#include <Wire.h>

LCDManager::LCDManager(uint8_t address) 
    : lcd(address, LCD_COLS, LCD_ROWS),
      currentState(SCREEN_STARTUP),
      stateStartTime(0),
      displayDurationMs(0),
      isTemporaryScreen(false),
      ready(false) {}

void LCDManager::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.beginTransmission(LCD_DEFAULT_ADDR);
    ready = Wire.endTransmission() == 0;
    if (!ready) {
        Serial.printf("[LCD] No display found at I2C address 0x%02X.\n", LCD_DEFAULT_ADDR);
        return;
    }
    lcd.init();
    lcd.backlight();
    showStartup();
}

String LCDManager::formatLine(String str) {
    if (str.length() > LCD_COLS) {
        return str.substring(0, LCD_COLS);
    }
    while (str.length() < LCD_COLS) {
        str += " ";
    }
    return str;
}

void LCDManager::updateScreenText(const String &line1, const String &line2) {
    if (!ready) return;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(formatLine(line1));
    lcd.setCursor(0, 1);
    lcd.print(formatLine(line2));
}

void LCDManager::showStartup() {
    currentState = SCREEN_STARTUP;
    updateScreenText("ECE SMART RACK", "System Ready...");
    stateStartTime = millis();
    displayDurationMs = 2500;
    isTemporaryScreen = true;
}

void LCDManager::showWaitingForRFID() {
    currentState = SCREEN_IDLE_WAITING;
    updateScreenText("Welcome Student", "Tap Your RFID");
    isTemporaryScreen = false;
}

void LCDManager::showSubmissionSuccess(const String &studentName, const String &compartmentName) {
    currentState = SCREEN_SUBMISSION_SUCCESS;
    updateScreenText("Welcome " + studentName.substring(0, 8), "Box: " + compartmentName);
    stateStartTime = millis();
    displayDurationMs = 4000;
    isTemporaryScreen = true;
}

void LCDManager::showAlreadyAssigned(const String &studentName, const String &compartmentName) {
    currentState = SCREEN_ALREADY_ASSIGNED;
    updateScreenText("Already Assigned", "Box: " + compartmentName);
    stateStartTime = millis();
    displayDurationMs = 3500;
    isTemporaryScreen = true;
}

void LCDManager::showBagCollection(const String &studentName, const String &compartmentName) {
    currentState = SCREEN_BAG_COLLECTION_READY;
    updateScreenText(studentName.substring(0, 16), "Collect: " + compartmentName);
    stateStartTime = millis();
    displayDurationMs = 3500;
    isTemporaryScreen = true;
}

void LCDManager::showCollectionConfirmationWaiting(const String &compartmentName) {
    currentState = SCREEN_CONFIRMATION_WAITING;
    updateScreenText("Collect Your Bag", "Press ConfirmBtn");
    stateStartTime = millis();
    displayDurationMs = 6000;
    isTemporaryScreen = true;
}

void LCDManager::showCollectionDone(const String &compartmentName) {
    currentState = SCREEN_COLLECTION_DONE;
    updateScreenText("Collection Done", "Box " + compartmentName + " Free");
    stateStartTime = millis();
    displayDurationMs = 3000;
    isTemporaryScreen = true;
}

void LCDManager::showUnregisteredCard() {
    currentState = SCREEN_UNREGISTERED_CARD;
    updateScreenText("Access Denied", "Card Unregistered");
    stateStartTime = millis();
    displayDurationMs = 3000;
    isTemporaryScreen = true;
}

void LCDManager::showStorageFull() {
    currentState = SCREEN_STORAGE_FULL;
    updateScreenText("Storage Full!", "No Box Available");
    stateStartTime = millis();
    displayDurationMs = 3500;
    isTemporaryScreen = true;
}

void LCDManager::showReadError() {
    currentState = SCREEN_READ_ERROR;
    updateScreenText("RFID Read Error", "Please Try Again");
    stateStartTime = millis();
    displayDurationMs = 2500;
    isTemporaryScreen = true;
}

void LCDManager::loop() {
    if (isTemporaryScreen && (millis() - stateStartTime >= displayDurationMs)) {
        showWaitingForRFID();
    }
}

