#include "AssignmentManager.h"
#include "StorageManager.h"
#include <time.h>
#include <algorithm>

void AssignmentManager::pulseOutput(int pin, unsigned long durationMs) {
    if (pin < 0) return;
    if (feedbackPin >= 0 && feedbackPin != pin) digitalWrite(feedbackPin, LOW);
    feedbackPin = pin;
    digitalWrite(feedbackPin, HIGH);
    feedbackOffAt = millis() + durationMs;
}

void AssignmentManager::init() {
    StorageManager::loadAssignments(history);
}

String AssignmentManager::getCurrentDateTime() {
    const time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm localTime;
        localtime_r(&now, &localTime);
        char dateTime[24];
        strftime(dateTime, sizeof(dateTime), "%Y-%m-%d %H:%M:%S", &localTime);
        return String(dateTime);
    }

    unsigned long sec = millis() / 1000;
    int h = (sec / 3600) % 24;
    int m = (sec / 60) % 60;
    int s = sec % 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "Session %02d:%02d:%02d", h, m, s);
    return String(buf);
}

bool AssignmentManager::handleCardTap(const String &uid, String &outMessage, JsonObject outEventData) {
    outEventData["uid"] = uid;
    
    Student* student = studentMgr.getByUid(uid);
    if (!student || !student->active) {
        lcdMgr.showUnregisteredCard();
        pulseOutput(PIN_LED_ERROR, 300);
        outMessage = "Unregistered or Inactive Card!";
        outEventData["event"] = "UNREGISTERED_CARD";
        return false;
    }

    outEventData["studentId"] = student->studentId;
    outEventData["studentName"] = student->name;
    outEventData["regNumber"] = student->regNumber;

    Compartment* assignedBox = compartmentMgr.getByStudentId(student->studentId);

    if (assignedBox != nullptr) {
        lcdMgr.showBagCollection(student->name, assignedBox->displayName);
        pendingCollectionBoxId = assignedBox->id;
        pendingCollectionStudentId = student->studentId;
        pendingStartTime = millis();

        outMessage = "Assigned to Compartment " + assignedBox->displayName;
        outEventData["event"] = "COLLECTION_INITIATED";
        outEventData["compartmentId"] = assignedBox->id;
        outEventData["compartmentName"] = assignedBox->displayName;

        pulseOutput(PIN_LED_SUCCESS, 150);
        return true;
    }

    Compartment* freeBox = compartmentMgr.getFirstAvailable();
    if (freeBox == nullptr) {
        lcdMgr.showStorageFull();
        pulseOutput(PIN_LED_ERROR, 400);
        outMessage = "Storage Full! No compartments available.";
        outEventData["event"] = "STORAGE_FULL";
        return false;
    }

    if (!compartmentMgr.allocateCompartment(freeBox->id, student->studentId)) {
        lcdMgr.showReadError();
        outMessage = "Allocation failed.";
        return false;
    }

    AssignmentRecord record;
    record.assignmentId = "ASN-" + String(millis());
    record.studentId = student->studentId;
    record.studentName = student->name;
    record.regNumber = student->regNumber;
    record.compartmentId = freeBox->id;
    record.compartmentName = freeBox->displayName;
    record.rfidUid = uid;
    record.submissionTimestamp = getCurrentDateTime();
    record.collectionTimestamp = "";
    record.isCompleted = false;

    history.push_back(record);
    if (history.size() > MAX_HISTORY) {
        const auto completed = std::find_if(history.begin(), history.end(), [](const AssignmentRecord &item) { return item.isCompleted; });
        history.erase(completed != history.end() ? completed : history.begin());
    }
    StorageManager::saveAssignments(history);

    // Asynchronously queue student submission email
    emailMgr.queueSubmissionEmail(*student, freeBox->displayName, record.submissionTimestamp);

    lcdMgr.showSubmissionSuccess(student->name, freeBox->displayName);
    pulseOutput(PIN_LED_SUCCESS, 250);

    outMessage = "Bag Submitted: " + freeBox->displayName;
    outEventData["event"] = "BAG_SUBMITTED";
    outEventData["compartmentId"] = freeBox->id;
    outEventData["compartmentName"] = freeBox->displayName;
    return true;
}

bool AssignmentManager::confirmCollection() {
    if (pendingCollectionBoxId <= 0) return false;

    Compartment* box = compartmentMgr.getCompartmentById(pendingCollectionBoxId);
    if (!box) return false;

    String boxName = box->displayName;
    Student* student = studentMgr.getById(pendingCollectionStudentId);
    String collectionTime = getCurrentDateTime();

    compartmentMgr.releaseCompartment(pendingCollectionBoxId);

    for (auto &rec : history) {
        if (!rec.isCompleted && rec.compartmentId == pendingCollectionBoxId && rec.studentId == pendingCollectionStudentId) {
            rec.isCompleted = true;
            rec.compartmentName = boxName;
            rec.collectionTimestamp = collectionTime;
            break;
        }
    }
    StorageManager::saveAssignments(history);

    Serial.printf("[COLLECTION] Confirmed for student ID '%s', compartment '%s'.\n",
                  pendingCollectionStudentId.c_str(), boxName.c_str());

    // Queue the receipt immediately; SMTP is handled asynchronously by the email worker.
    if (student != nullptr) {
        if (!emailMgr.queueCollectionEmail(*student, boxName, collectionTime)) {
            Serial.println("[COLLECTION] Receipt was not queued; see the [EMAIL] skip reason above.");
        }
    } else {
        Serial.printf("[COLLECTION] Receipt not queued: student ID '%s' is missing from the student database.\n",
                      pendingCollectionStudentId.c_str());
    }

    lcdMgr.showCollectionDone(boxName);
    pulseOutput(PIN_BUZZER, 80);

    pendingCollectionBoxId = 0;
    pendingCollectionStudentId = "";
    return true;
}

void AssignmentManager::cancelPendingCollection() {
    pendingCollectionBoxId = 0;
    pendingCollectionStudentId = "";
}

bool AssignmentManager::removeStudentAndHistory(const String &uid, String &outError, int &httpStatus) {
    Student *student = studentMgr.getByUid(uid);
    if (!student) {
        outError = "Student was not found.";
        httpStatus = 404;
        return false;
    }

    const String studentId = student->studentId;
    if (compartmentMgr.getByStudentId(studentId) != nullptr) {
        outError = "Collect the student's bag before deleting this profile.";
        httpStatus = 409;
        return false;
    }

    const std::vector<AssignmentRecord> previousHistory = history;
    history.erase(std::remove_if(history.begin(), history.end(), [&studentId](const AssignmentRecord &record) {
        return record.studentId == studentId;
    }), history.end());

    if (!StorageManager::saveAssignments(history)) {
        history = previousHistory;
        outError = "Could not save the updated activity history.";
        httpStatus = 500;
        return false;
    }

    if (!studentMgr.deleteByUid(uid)) {
        history = previousHistory;
        StorageManager::saveAssignments(history);
        outError = "Could not save the updated student list.";
        httpStatus = 500;
        return false;
    }

    httpStatus = 200;
    return true;
}

int AssignmentManager::clearCompletedHistory() {
    const std::vector<AssignmentRecord> previousHistory = history;
    const auto firstRemoved = std::remove_if(history.begin(), history.end(), [](const AssignmentRecord &record) {
        return record.isCompleted;
    });
    const int removedCount = static_cast<int>(history.end() - firstRemoved);
    if (removedCount == 0) return 0;

    history.erase(firstRemoved, history.end());
    if (!StorageManager::saveAssignments(history)) {
        history = previousHistory;
        return -1;
    }
    return removedCount;
}

void AssignmentManager::loop() {
    if (feedbackPin >= 0 && (long)(millis() - feedbackOffAt) >= 0) {
        digitalWrite(feedbackPin, LOW);
        feedbackPin = -1;
        feedbackOffAt = 0;
    }
    if (pendingCollectionBoxId > 0 && (millis() - pendingStartTime > 20000)) {
        cancelPendingCollection();
    }
}

int AssignmentManager::getSubmissionsTodayCount() {
    return history.size();
}

int AssignmentManager::getCollectionsTodayCount() {
    int cnt = 0;
    for (const auto &r : history) if (r.isCompleted) cnt++;
    return cnt;
}
