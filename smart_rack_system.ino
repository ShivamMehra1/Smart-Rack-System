#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <time.h>
#include "Config.h"
#include "Models.h"
#include "StorageManager.h"
#include "LCDManager.h"
#include "RFIDManager.h"
#include "StudentManager.h"
#include "CompartmentManager.h"
#include "AssignmentManager.h"
#include "EmailManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

SystemSettings sysSettings;
LCDManager lcdMgr(LCD_DEFAULT_ADDR);
RFIDManager rfidMgr;
StudentManager studentMgr;
CompartmentManager compartmentMgr;
EmailManager emailMgr;
AssignmentManager assignmentMgr(studentMgr, compartmentMgr, lcdMgr, emailMgr, sysSettings);

AsyncWebServer server(HTTP_PORT);
AsyncWebSocket ws("/ws");

unsigned long lastBtnCheck = 0;
bool lastBtnState = LOW;
SemaphoreHandle_t stateMutex = nullptr;
QueueHandle_t cardQueue = nullptr;
bool emailWorkerStarted = false;

struct CardScanMessage {
    char uid[21];
    RFIDMode mode;
};

class StateGuard {
public:
    StateGuard() {
        if (stateMutex) xSemaphoreTakeRecursive(stateMutex, portMAX_DELAY);
    }
    ~StateGuard() {
        if (stateMutex) xSemaphoreGiveRecursive(stateMutex);
    }
    StateGuard(const StateGuard &) = delete;
    StateGuard &operator=(const StateGuard &) = delete;
};

template <typename T>
void broadcastWs(const String &type, const T &data) {
    DynamicJsonDocument doc(4096);
    doc["type"] = type;
    doc["payload"] = data;
    doc["timestamp"] = millis();
    String out;
    serializeJson(doc, out);
    ws.textAll(out);
}

void enqueueCardScanned(const String &uid, RFIDMode mode) {
    if (!cardQueue) return;
    CardScanMessage message{};
    uid.toCharArray(message.uid, sizeof(message.uid));
    message.mode = mode;
    if (xQueueSend(cardQueue, &message, 0) != pdTRUE) {
        Serial.println("[RFID] Card event queue full; scan dropped.");
    }
}

void processCardScanned(const String &uid, RFIDMode mode) {
    StateGuard guard;
    Serial.printf("[MAIN] Card Scanned: %s (Mode: %d)\n", uid.c_str(), mode);

    if (mode == MODE_ADMIN_REGISTRATION || mode == MODE_CARD_REPLACEMENT) {
        DynamicJsonDocument doc(512);
        doc["uid"] = uid;
        doc["mode"] = (mode == MODE_ADMIN_REGISTRATION) ? "REGISTRATION" : "REPLACEMENT";
        broadcastWs("RFID_SCANNED", doc.as<JsonVariant>());
        rfidMgr.setMode(MODE_NORMAL_ALLOCATION);
        return;
    }

    String msg;
    DynamicJsonDocument eventData(1024);
    assignmentMgr.handleCardTap(uid, msg, eventData.to<JsonObject>());
    broadcastWs("ALLOCATION_EVENT", eventData.as<JsonVariant>());
}

// Core 0: poll the PN532 and enqueue compact card events only. The web/API and
// all rack state changes remain in loop() on Core 1.
void rfidReaderTask(void *) {
    unsigned long retryAt = millis() + 5000;
    for (;;) {
        if (rfidMgr.isReady()) {
            rfidMgr.loop();
        } else if (static_cast<int32_t>(millis() - retryAt) >= 0) {
            retryAt = millis() + 5000;
            Serial.println("[RFID] Retrying PN532 initialization...");
            rfidMgr.begin(enqueueCardScanned);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// SMTP/TLS can wait several seconds on network I/O. Run it separately so the
// Arduino loop keeps scanning the confirmation button and updating the LCD.
void emailWorkerTask(void *) {
    for (;;) {
        emailMgr.loop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- [ECE LAB SMART STORAGE SYSTEM STARTING] ---");

    stateMutex = xSemaphoreCreateRecursiveMutex();
    cardQueue = xQueueCreate(8, sizeof(CardScanMessage));
    if (!stateMutex || !cardQueue) {
        Serial.println("[FATAL] Could not create state lock or RFID event queue.");
        while (true) delay(1000);
    }

    pinMode(PIN_CONFIRM_BTN, INPUT_PULLDOWN);
    pinMode(PIN_LED_SUCCESS, OUTPUT);
    pinMode(PIN_LED_ERROR, OUTPUT);

    if (PIN_BUZZER >= 0) {
        pinMode(PIN_BUZZER, OUTPUT);
        digitalWrite(PIN_BUZZER, LOW);
    }
    digitalWrite(PIN_LED_SUCCESS, LOW);
    digitalWrite(PIN_LED_ERROR, LOW);

    StorageManager::init();
    StorageManager::loadSettings(sysSettings);

    lcdMgr.begin();
    compartmentMgr.init(sysSettings);
    studentMgr.init();
    emailMgr.init();
    assignmentMgr.init();
    rfidMgr.begin(enqueueCardScanned);

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(DEFAULT_AP_SSID, DEFAULT_AP_PASS);
    Serial.print("[WIFI] AP Started. IP Address: ");
    Serial.println(WiFi.softAPIP());

    Serial.println("[WEB] Registering dashboard and API routes...");
    Serial.printf("[WEB] LittleFS /index.html: %s\n", LittleFS.exists("/index.html") ? "found" : "MISSING");
    server.addHandler(&ws);
    // The asset filenames are content-hashed, so cache them for a long time.
    // Keep the HTML uncached so it picks up a newly uploaded dashboard bundle.
    server.serveStatic("/assets/", LittleFS, "/assets/")
        .setCacheControl("public, max-age=31536000, immutable");
    server.serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("no-cache");

    server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        DynamicJsonDocument doc(1024);
        doc["online"] = true;
        doc["uptimeMs"] = millis();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["apSsid"] = DEFAULT_AP_SSID;
        doc["apIp"] = WiFi.softAPIP().toString();
        doc["routerConnected"] = WiFi.status() == WL_CONNECTED;
        doc["routerSsid"] = sysSettings.wifiSsid;
        doc["routerIp"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
        doc["rfidReader"] = rfidMgr.readerName();
        doc["rfidReady"] = rfidMgr.isReady();
        doc["lcdReady"] = lcdMgr.isReady();
        doc["lcdAddress"] = LCD_DEFAULT_ADDR;
        doc["pendingCollectionBox"] = assignmentMgr.getPendingCollectionBox();
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    AsyncCallbackJsonWebHandler *networkHandler = new AsyncCallbackJsonWebHandler("/api/network", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        if (obj.containsKey("routerSsid")) sysSettings.wifiSsid = obj["routerSsid"].as<String>();
        if (obj.containsKey("clearRouterPassword") && obj["clearRouterPassword"].as<bool>()) {
            sysSettings.wifiPassword = "";
        } else if (obj.containsKey("routerPassword") && !obj["routerPassword"].as<String>().isEmpty()) {
            sysSettings.wifiPassword = obj["routerPassword"].as<String>();
        }
        StorageManager::saveSettings(sysSettings);
        if (sysSettings.wifiSsid.isEmpty()) {
            WiFi.disconnect(false, true);
        } else {
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(sysSettings.wifiSsid.c_str(), sysSettings.wifiPassword.c_str());
        }
        request->send(200, "application/json", "{\"status\":\"saved\",\"message\":\"Network settings saved; router connection may take a few seconds.\"}");
    });
    server.addHandler(networkHandler);

    // Overview Stats
    server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        DynamicJsonDocument doc(512);
        doc["totalCompartments"] = compartmentMgr.getTotalCount();
        doc["availableCompartments"] = compartmentMgr.getAvailableCount();
        doc["occupiedCompartments"] = compartmentMgr.getOccupiedCount();
        doc["maintenanceCompartments"] = compartmentMgr.getMaintenanceCount();
        doc["totalStudents"] = studentMgr.getTotalStudents();
        doc["activeAssignments"] = compartmentMgr.getOccupiedCount();
        doc["submissionsToday"] = assignmentMgr.getSubmissionsTodayCount();
        doc["collectionsToday"] = assignmentMgr.getCollectionsTodayCount();
        doc["emailEnabled"] = emailMgr.getConfig().enabled;
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    // Compartments API
    server.on("/api/compartments", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        DynamicJsonDocument doc(16384);
        JsonArray arr = doc.to<JsonArray>();
        for (const auto &c : compartmentMgr.getCompartments()) {
            JsonObject o = arr.createNestedObject();
            o["id"] = c.id;
            o["name"] = c.displayName;
            o["row"] = c.row;
            o["col"] = c.col;
            o["status"] = (int)c.status;
            o["studentId"] = c.assignedStudentId;
            o["notes"] = c.maintenanceNotes;
        }
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    AsyncCallbackJsonWebHandler *compartmentUpdateHandler = new AsyncCallbackJsonWebHandler("/api/compartments/update", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        const int id = obj["id"] | 0;
        Compartment *box = compartmentMgr.getCompartmentById(id);
        if (!box) {
            request->send(404, "application/json", "{\"error\":\"Compartment was not found.\"}");
            return;
        }

        bool ok = true;
        if (obj.containsKey("name")) ok = compartmentMgr.updateDisplayName(id, obj["name"].as<String>());
        if (ok && obj.containsKey("maintenance")) {
            const bool maintenance = obj["maintenance"].as<bool>();
            if (maintenance != (box->status == MAINTENANCE)) {
                ok = compartmentMgr.setMaintenance(id, maintenance, obj["notes"] | "");
            }
        }
        if (!ok) {
            request->send(409, "application/json", "{\"error\":\"Occupied compartments cannot be placed into maintenance.\"}");
            return;
        }
        broadcastWs("COMPARTMENTS_UPDATED", id);
        request->send(200, "application/json", "{\"status\":\"updated\"}");
    });
    server.addHandler(compartmentUpdateHandler);

    // Students API
    server.on("/api/students", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        DynamicJsonDocument doc(16384);
        JsonArray arr = doc.to<JsonArray>();
        for (const auto &s : studentMgr.getStudents()) {
            JsonObject o = arr.createNestedObject();
            o["studentId"] = s.studentId;
            o["name"] = s.name;
            o["regNumber"] = s.regNumber;
            o["mobile"] = s.mobile;
            o["email"] = s.email;
            o["section"] = s.section;
            o["rfidUid"] = s.rfidUid;
            o["registrationDate"] = s.registrationDate;
            o["active"] = s.active;
            Compartment* b = compartmentMgr.getByStudentId(s.studentId);
            o["currentCompartment"] = b ? b->displayName : "";
        }
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    AsyncCallbackJsonWebHandler *studentCreateHandler = new AsyncCallbackJsonWebHandler("/api/students", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        Student student;
        student.studentId = "ST-" + String(millis());
        student.name = obj["name"] | "";
        student.regNumber = obj["regNumber"] | "";
        student.mobile = obj["mobile"] | "";
        student.email = obj["email"] | "";
        student.section = obj["section"] | "";
        student.rfidUid = obj["rfidUid"] | "";
        student.registrationDate = "Session " + String(millis() / 1000);
        student.active = true;

        if (student.name.isEmpty() || student.regNumber.isEmpty() || student.rfidUid.isEmpty()) {
            request->send(400, "application/json", "{\"error\":\"Name, registration number, and scanned RFID UID are required.\"}");
            return;
        }
        if (!studentMgr.registerStudent(student)) {
            request->send(409, "application/json", "{\"error\":\"That RFID card or registration number is already registered.\"}");
            return;
        }
        broadcastWs("STUDENT_UPDATED", student.studentId);
        request->send(201, "application/json", "{\"status\":\"created\"}");
    });
    server.addHandler(studentCreateHandler);

    AsyncCallbackJsonWebHandler *studentStatusHandler = new AsyncCallbackJsonWebHandler("/api/students/status", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        const String studentId = obj["studentId"] | "";
        const bool active = obj["active"] | false;
        if (!studentMgr.setStatus(studentId, active)) {
            request->send(404, "application/json", "{\"error\":\"Student was not found.\"}");
            return;
        }
        broadcastWs("STUDENT_UPDATED", studentId);
        request->send(200, "application/json", "{\"status\":\"updated\"}");
    });
    server.addHandler(studentStatusHandler);

    AsyncCallbackJsonWebHandler *studentDeleteHandler = new AsyncCallbackJsonWebHandler("/api/students/delete", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        const String uid = obj["rfidUid"] | "";
        if (uid.isEmpty()) {
            request->send(400, "application/json", "{\"error\":\"RFID UID is required.\"}");
            return;
        }

        String error;
        int status = 500;
        if (!assignmentMgr.removeStudentAndHistory(uid, error, status)) {
            DynamicJsonDocument doc(256);
            doc["error"] = error;
            String response;
            serializeJson(doc, response);
            request->send(status, "application/json", response);
            return;
        }

        broadcastWs("STUDENT_UPDATED", "Student removed");
        request->send(200, "application/json", "{\"status\":\"deleted\"}");
    });
    server.addHandler(studentDeleteHandler);

    // History API
    server.on("/api/history", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        DynamicJsonDocument doc(24576);
        JsonArray arr = doc.to<JsonArray>();
        for (const auto &r : assignmentMgr.getHistory()) {
            JsonObject o = arr.createNestedObject();
            o["assignmentId"] = r.assignmentId;
            o["studentName"] = r.studentName;
            o["regNumber"] = r.regNumber;
            o["compartmentId"] = r.compartmentId;
            o["compartmentName"] = r.compartmentName;
            o["rfidUid"] = r.rfidUid;
            o["submissionTime"] = r.submissionTimestamp;
            o["collectionTime"] = r.collectionTimestamp;
            o["isCompleted"] = r.isCompleted;
        }
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    AsyncCallbackJsonWebHandler *clearHistoryHandler = new AsyncCallbackJsonWebHandler("/api/history/clear-completed", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        const int removed = assignmentMgr.clearCompletedHistory();
        if (removed < 0) {
            request->send(500, "application/json", "{\"error\":\"Could not save the cleared activity history.\"}");
            return;
        }

        broadcastWs("COLLECTION_CONFIRMED", "Completed history cleared");
        DynamicJsonDocument doc(256);
        doc["status"] = "cleared";
        doc["removed"] = removed;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    server.addHandler(clearHistoryHandler);

    // Confirm Collection API
    server.on("/api/confirm-collection", HTTP_POST, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        if (assignmentMgr.confirmCollection()) {
            broadcastWs("COLLECTION_CONFIRMED", "Success");
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"No pending collection\"}");
        }
    });

    // Arm RFID Registration
    server.on("/api/rfid/arm-registration", HTTP_POST, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        rfidMgr.setMode(MODE_ADMIN_REGISTRATION);
        request->send(200, "application/json", "{\"status\":\"armed\"}");
    });

    // --- Customer-Configurable Email System APIs ---

    // 1. Get Email Configuration (Never expose raw password to untrusted sources, mask with stars if set)
    server.on("/api/email/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        const EmailConfig &cfg = emailMgr.getConfig();
        DynamicJsonDocument doc(4096);
        doc["enabled"] = cfg.enabled;
        doc["notifyOnSubmission"] = cfg.notifyOnSubmission;
        doc["notifyOnCollection"] = cfg.notifyOnCollection;
        doc["institutionName"] = cfg.institutionName;
        doc["senderName"] = cfg.senderName;
        doc["senderEmail"] = cfg.senderEmail;
        doc["smtpProvider"] = cfg.smtpProvider;
        doc["smtpHost"] = cfg.smtpHost;
        doc["smtpPort"] = cfg.smtpPort;
        doc["securityProtocol"] = cfg.securityProtocol;
        doc["smtpUsername"] = cfg.smtpUsername;
        doc["hasPassword"] = !cfg.smtpPassword.isEmpty();
        doc["submissionSubject"] = cfg.submissionSubject;
        doc["submissionTemplate"] = cfg.submissionTemplate;
        doc["collectionSubject"] = cfg.collectionSubject;
        doc["collectionTemplate"] = cfg.collectionTemplate;

        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    // 2. Save Email Configuration (Customer's own SMTP settings)
    AsyncCallbackJsonWebHandler *emailConfigHandler = new AsyncCallbackJsonWebHandler("/api/email/config", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        EmailConfig cfg = emailMgr.getConfig();

        if (obj.containsKey("enabled")) cfg.enabled = obj["enabled"].as<bool>();
        if (obj.containsKey("notifyOnSubmission")) cfg.notifyOnSubmission = obj["notifyOnSubmission"].as<bool>();
        if (obj.containsKey("notifyOnCollection")) cfg.notifyOnCollection = obj["notifyOnCollection"].as<bool>();
        if (obj.containsKey("institutionName")) cfg.institutionName = obj["institutionName"].as<String>();
        if (obj.containsKey("senderName")) cfg.senderName = obj["senderName"].as<String>();
        if (obj.containsKey("senderEmail")) cfg.senderEmail = obj["senderEmail"].as<String>();
        if (obj.containsKey("smtpProvider")) cfg.smtpProvider = obj["smtpProvider"].as<String>();
        if (obj.containsKey("smtpHost")) cfg.smtpHost = obj["smtpHost"].as<String>();
        if (obj.containsKey("smtpPort")) cfg.smtpPort = obj["smtpPort"].as<int>();
        if (obj.containsKey("securityProtocol")) cfg.securityProtocol = obj["securityProtocol"].as<String>();
        if (obj.containsKey("smtpUsername")) cfg.smtpUsername = obj["smtpUsername"].as<String>();
        if (obj.containsKey("smtpPassword") && !obj["smtpPassword"].as<String>().isEmpty()) {
            cfg.smtpPassword = obj["smtpPassword"].as<String>();
        }
        if (obj.containsKey("submissionSubject")) cfg.submissionSubject = obj["submissionSubject"].as<String>();
        if (obj.containsKey("submissionTemplate")) cfg.submissionTemplate = obj["submissionTemplate"].as<String>();
        if (obj.containsKey("collectionSubject")) cfg.collectionSubject = obj["collectionSubject"].as<String>();
        if (obj.containsKey("collectionTemplate")) cfg.collectionTemplate = obj["collectionTemplate"].as<String>();

        emailMgr.updateConfig(cfg);
        request->send(200, "application/json", "{\"status\":\"saved\"}");
    });
    server.addHandler(emailConfigHandler);

    // 3. Send Test Email
    AsyncCallbackJsonWebHandler *emailTestHandler = new AsyncCallbackJsonWebHandler("/api/email/test", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        String recipient = obj["recipient"].as<String>();
        String msg;
        bool ok = emailMgr.sendTestEmail(recipient, msg);

        DynamicJsonDocument doc(512);
        doc["success"] = ok;
        doc["message"] = msg;
        String res;
        serializeJson(doc, res);
        request->send(ok ? 200 : 400, "application/json", res);
    });
    server.addHandler(emailTestHandler);

    // 4. Get Email Logs & Delivery History
    server.on("/api/email/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
        StateGuard guard;
        const auto &logs = emailMgr.getLogs();
        DynamicJsonDocument doc(16384);
        JsonArray arr = doc.to<JsonArray>();
        for (int i = logs.size() - 1; i >= 0; i--) {
            const auto &l = logs[i];
            JsonObject o = arr.createNestedObject();
            o["logId"] = l.logId;
            o["recipient"] = l.recipientEmail;
            o["student"] = l.studentName;
            o["subject"] = l.subject;
            o["type"] = l.eventType;
            o["timestamp"] = l.timestamp;
            o["success"] = l.success;
            o["error"] = l.errorMessage;
            o["retries"] = l.retryCount;
        }
        String res;
        serializeJson(doc, res);
        request->send(200, "application/json", res);
    });

    // 5. Retry Failed Email
    AsyncCallbackJsonWebHandler *emailRetryHandler = new AsyncCallbackJsonWebHandler("/api/email/retry", [](AsyncWebServerRequest *request, JsonVariant &json) {
        StateGuard guard;
        JsonObject obj = json.as<JsonObject>();
        String logId = obj["logId"].as<String>();
        bool ok = emailMgr.retryFailedEmail(logId);
        request->send(ok ? 200 : 400, "application/json", ok ? "{\"status\":\"resent\"}" : "{\"error\":\"Retry failed\"}");
    });
    server.addHandler(emailRetryHandler);

    Serial.println("[WEB] Starting HTTP server...");
    server.begin();
    Serial.println("[SYSTEM] Server Started with Full Customer Email Subsystem.");
    const BaseType_t taskOk = xTaskCreatePinnedToCore(
        rfidReaderTask, "rfid-scan", 6144, nullptr, 2, nullptr, 0);
    if (taskOk != pdPASS) Serial.println("[FATAL] Could not start the RFID scan task.");
    const BaseType_t emailTaskOk = xTaskCreatePinnedToCore(
        emailWorkerTask, "email-worker", 8192, nullptr, 1, nullptr, 1);
    emailWorkerStarted = emailTaskOk == pdPASS;
    if (!emailWorkerStarted) Serial.println("[WARN] Email worker task unavailable; SMTP will run in the main loop.");

    // Start optional internet time/router services only after the local AP web
    // server is running. The dashboard must remain available without a router.
    if (!sysSettings.wifiSsid.isEmpty()) {
        Serial.printf("[WIFI] Connecting to saved router network: %s\n", sysSettings.wifiSsid.c_str());
        WiFi.begin(sysSettings.wifiSsid.c_str(), sysSettings.wifiPassword.c_str());
    }
    configTime(UTC_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com");
}

void loop() {
    ws.cleanupClients();
    CardScanMessage scan{};
    while (xQueueReceive(cardQueue, &scan, 0) == pdTRUE) {
        processCardScanned(String(scan.uid), scan.mode);
    }

    {
        StateGuard guard;
        lcdMgr.loop();
        assignmentMgr.loop();

        if (millis() - lastBtnCheck > 50) {
            lastBtnCheck = millis();
            bool currentBtn = digitalRead(PIN_CONFIRM_BTN);
            if (currentBtn == HIGH && lastBtnState == LOW) {
                if (assignmentMgr.getPendingCollectionBox() > 0) {
                    assignmentMgr.confirmCollection();
                    broadcastWs("COLLECTION_CONFIRMED", "Success");
                }
            }
            lastBtnState = currentBtn;
        }
    }

    // Fallback only if FreeRTOS could not create the dedicated email task.
    if (!emailWorkerStarted) emailMgr.loop();
    delay(2);
}
