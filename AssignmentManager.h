#pragma once
#include <vector>
#include <ArduinoJson.h>
#include "Models.h"
#include "StudentManager.h"
#include "CompartmentManager.h"
#include "LCDManager.h"
#include "EmailManager.h"

class AssignmentManager {
private:
    std::vector<AssignmentRecord> history;
    StudentManager &studentMgr;
    CompartmentManager &compartmentMgr;
    LCDManager &lcdMgr;
    EmailManager &emailMgr;
    SystemSettings &settings;

    int pendingCollectionBoxId;
    String pendingCollectionStudentId;
    unsigned long pendingStartTime;
    int feedbackPin;
    unsigned long feedbackOffAt;

    String getCurrentDateTime();
    void pulseOutput(int pin, unsigned long durationMs);

public:
    AssignmentManager(StudentManager &sm, CompartmentManager &cm, LCDManager &lm, EmailManager &em, SystemSettings &st)
        : studentMgr(sm), compartmentMgr(cm), lcdMgr(lm), emailMgr(em), settings(st),
          pendingCollectionBoxId(0), pendingStartTime(0), feedbackPin(-1), feedbackOffAt(0) {}

    void init();
    void loop();

    bool handleCardTap(const String &uid, String &outMessage, JsonObject outEventData);
    bool confirmCollection();
    void cancelPendingCollection();
    bool removeStudentAndHistory(const String &uid, String &outError, int &httpStatus);
    int clearCompletedHistory();

    std::vector<AssignmentRecord>& getHistory() { return history; }
    int getSubmissionsTodayCount();
    int getCollectionsTodayCount();
    int getPendingCollectionBox() const { return pendingCollectionBoxId; }
};
