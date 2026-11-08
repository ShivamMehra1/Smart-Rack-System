#pragma once
#include <LiquidCrystal_I2C.h>
#include "Config.h"

enum LCDScreenState {
    SCREEN_STARTUP,
    SCREEN_IDLE_WAITING,
    SCREEN_SUBMISSION_SUCCESS,
    SCREEN_ALREADY_ASSIGNED,
    SCREEN_BAG_COLLECTION_READY,
    SCREEN_CONFIRMATION_WAITING,
    SCREEN_COLLECTION_DONE,
    SCREEN_UNREGISTERED_CARD,
    SCREEN_STORAGE_FULL,
    SCREEN_READ_ERROR
};

class LCDManager {
private:
    LiquidCrystal_I2C lcd;
    LCDScreenState currentState;
    unsigned long stateStartTime;
    unsigned long displayDurationMs;
    bool isTemporaryScreen;
    bool ready;

    void updateScreenText(const String &line1, const String &line2);
    String formatLine(String str);

public:
    LCDManager(uint8_t address = LCD_DEFAULT_ADDR);
    void begin();
    void loop();
    bool isReady() const { return ready; }

    void showStartup();
    void showWaitingForRFID();
    void showSubmissionSuccess(const String &studentName, const String &compartmentName);
    void showAlreadyAssigned(const String &studentName, const String &compartmentName);
    void showBagCollection(const String &studentName, const String &compartmentName);
    void showCollectionConfirmationWaiting(const String &compartmentName);
    void showCollectionDone(const String &compartmentName);
    void showUnregisteredCard();
    void showStorageFull();
    void showReadError();
};

