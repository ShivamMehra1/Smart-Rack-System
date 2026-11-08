#include "EmailManager.h"
#include "StorageManager.h"
#include <base64.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t stateMutex;

namespace {
class EmailStateLock {
public:
    EmailStateLock() {
        if (stateMutex) xSemaphoreTakeRecursive(stateMutex, portMAX_DELAY);
    }
    ~EmailStateLock() {
        if (stateMutex) xSemaphoreGiveRecursive(stateMutex);
    }
    EmailStateLock(const EmailStateLock &) = delete;
    EmailStateLock &operator=(const EmailStateLock &) = delete;
};
}

EmailManager::EmailManager() : lastProcessTime(0) {}

void EmailManager::init() {
    StorageManager::loadEmailConfig(config);
    StorageManager::loadEmailLogs(logs);
    Serial.printf("[EMAIL] Email Manager Initialized. Enabled: %s, Provider: %s\n", 
                  config.enabled ? "YES" : "NO", config.smtpProvider.c_str());
}

void EmailManager::updateConfig(const EmailConfig &newCfg) {
    config = newCfg;
    StorageManager::saveEmailConfig(config);
    Serial.println("[EMAIL] Configuration updated and saved to LittleFS.");
}

String EmailManager::interpolateTemplate(const String &tpl, const Student *s, const String &boxName, const String &timestamp) {
    String out = tpl;
    out.replace("{{institution_name}}", config.institutionName);
    out.replace("{{compartment_name}}", boxName);
    out.replace("{{timestamp}}", timestamp);
    if (s != nullptr) {
        out.replace("{{student_name}}", s->name);
        out.replace("{{reg_number}}", s->regNumber);
        out.replace("{{section}}", s->section);
        out.replace("{{mobile}}", s->mobile);
        out.replace("{{email}}", s->email);
    }
    return out;
}

void EmailManager::addLog(const String &logId, const String &to, const String &student, const String &subject, const String &type, const String &timestamp, bool success, const String &error) {
    EmailLog l;
    l.logId = logId;
    l.recipientEmail = to;
    l.studentName = student;
    l.subject = subject;
    l.eventType = type;
    l.timestamp = timestamp;
    l.success = success;
    l.errorMessage = error;
    l.retryCount = 0;

    logs.push_back(l);
    if (logs.size() > 30) logs.erase(logs.begin(), logs.begin() + (logs.size() - 30));
    StorageManager::saveEmailLogs(logs);
}

bool EmailManager::executeSend(const EmailConfig &cfg, const String &to, const String &subject, const String &body, String &errorMsg) {
    if (!cfg.enabled) {
        errorMsg = "Email notifications disabled in settings.";
        return false;
    }

    if (cfg.smtpHost.isEmpty() || cfg.smtpUsername.isEmpty() || cfg.senderEmail.isEmpty()) {
        errorMsg = "SMTP credentials incomplete in Email Settings.";
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        errorMsg = "ESP32 Station Wi-Fi is offline. Connect to Internet Wi-Fi.";
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Allows connecting to customer's SSL/TLS SMTP server without hardcoding CA root
    client.setTimeout(8000);

    Serial.printf("[EMAIL] Connecting to SMTP %s:%d...\n", cfg.smtpHost.c_str(), cfg.smtpPort);
    if (!client.connect(cfg.smtpHost.c_str(), cfg.smtpPort)) {
        errorMsg = "Failed to connect to SMTP server: " + cfg.smtpHost;
        return false;
    }

    // SMTP replies can span several lines (Gmail's EHLO reply does). Read
    // every continuation line before sending the next command, stopping only
    // at the final line whose fourth character is a space instead of '-'.
    auto readResponse = [&client](String &resp) -> bool {
        resp = "";
        const unsigned long startedAt = millis();
        bool haveLine = false;

        while (millis() - startedAt < 8000) {
            if (!client.available()) {
                if (!client.connected()) return false;
                delay(10);
                continue;
            }

            String line = client.readStringUntil('\n');
            line.trim();
            if (line.length() < 3) continue;

            if (haveLine) resp += '\n';
            resp += line;
            haveLine = true;

            if (line.length() < 4 || line[3] != '-') return true;
        }
        return false;
    };

    String resp;
    if (!readResponse(resp) || (!resp.startsWith("220") && !resp.startsWith("250"))) {
        errorMsg = "SMTP Greeting Error: " + resp;
        client.stop();
        return false;
    }

    // Send EHLO
    client.println("EHLO esp32.local");
    if (!readResponse(resp) || !resp.startsWith("250")) {
        errorMsg = "EHLO failed: " + resp;
        client.stop();
        return false;
    }

    // Authenticate (AUTH LOGIN)
    client.println("AUTH LOGIN");
    if (!readResponse(resp) || !resp.startsWith("334")) {
        errorMsg = "AUTH LOGIN rejected: " + resp;
        client.stop();
        return false;
    }

    // Username base64
    client.println(base64::encode(cfg.smtpUsername));
    if (!readResponse(resp) || !resp.startsWith("334")) {
        errorMsg = "SMTP Username rejected: " + resp;
        client.stop();
        return false;
    }

    // Password base64
    client.println(base64::encode(cfg.smtpPassword));
    if (!readResponse(resp) || !resp.startsWith("235")) {
        errorMsg = "Authentication failed (Invalid App Password / Password): " + resp;
        client.stop();
        return false;
    }

    // MAIL FROM
    client.println("MAIL FROM:<" + cfg.senderEmail + ">");
    if (!readResponse(resp) || !resp.startsWith("250")) {
        errorMsg = "MAIL FROM error: " + resp;
        client.stop();
        return false;
    }

    // RCPT TO
    client.println("RCPT TO:<" + to + ">");
    if (!readResponse(resp) || !resp.startsWith("250")) {
        errorMsg = "RCPT TO error: " + resp;
        client.stop();
        return false;
    }

    // DATA
    client.println("DATA");
    if (!readResponse(resp) || !resp.startsWith("354")) {
        errorMsg = "DATA command error: " + resp;
        client.stop();
        return false;
    }

    // Headers & Message Body
    client.println("From: \"" + cfg.senderName + "\" <" + cfg.senderEmail + ">");
    client.println("To: <" + to + ">");
    client.println("Subject: " + subject);
    client.println("Content-Type: text/plain; charset=UTF-8");
    client.println();
    client.println(body);
    client.println(".");

    if (!readResponse(resp) || !resp.startsWith("250")) {
        errorMsg = "Failed to deliver message: " + resp;
        client.stop();
        return false;
    }

    client.println("QUIT");
    client.stop();
    return true;
}

bool EmailManager::queueSubmissionEmail(const Student &student, const String &boxName, const String &timestamp) {
    if (!config.enabled || !config.notifyOnSubmission) return false;
    if (student.email.isEmpty() || student.email.indexOf('@') < 1) return false;
    if (queue.size() >= 8) return false;

    String subj = interpolateTemplate(config.submissionSubject, &student, boxName, timestamp);
    String body = interpolateTemplate(config.submissionTemplate, &student, boxName, timestamp);

    QueuedEmail q;
    q.logId = "EML-" + String(millis());
    q.recipientEmail = student.email;
    q.studentName = student.name;
    q.subject = subj;
    q.body = body;
    q.eventType = "SUBMISSION";
    q.timestamp = timestamp;
    q.retries = 0;

    queue.push_back(q);
    Serial.printf("[EMAIL] Queued bag submission email for %s (%s)\n", student.name.c_str(), student.email.c_str());
    return true;
}

bool EmailManager::queueCollectionEmail(const Student &student, const String &boxName, const String &timestamp) {
    if (!config.enabled) {
        Serial.println("[EMAIL] Collection email skipped: email notifications are disabled.");
        return false;
    }
    if (!config.notifyOnCollection) {
        Serial.println("[EMAIL] Collection email skipped: Bag Collection receipt is disabled in Email Settings.");
        return false;
    }
    if (student.email.isEmpty() || student.email.indexOf('@') < 1) {
        Serial.printf("[EMAIL] Collection email skipped: no valid email saved for %s.\n", student.name.c_str());
        return false;
    }
    if (queue.size() >= 8) {
        Serial.println("[EMAIL] Collection email skipped: email queue is full.");
        return false;
    }

    String subj = interpolateTemplate(config.collectionSubject, &student, boxName, timestamp);
    String body = interpolateTemplate(config.collectionTemplate, &student, boxName, timestamp);

    QueuedEmail q;
    q.logId = "EML-" + String(millis());
    q.recipientEmail = student.email;
    q.studentName = student.name;
    q.subject = subj;
    q.body = body;
    q.eventType = "COLLECTION";
    q.timestamp = timestamp;
    q.retries = 0;

    queue.push_back(q);
    Serial.printf("[EMAIL] Queued bag collection email for %s (%s)\n", student.name.c_str(), student.email.c_str());
    return true;
}

bool EmailManager::sendTestEmail(const String &testRecipient, String &outMessage) {
    if (testRecipient.isEmpty()) {
        outMessage = "Recipient address cannot be blank.";
        return false;
    }
    if (!config.enabled) {
        outMessage = "Email notifications are disabled in Email Settings.";
        return false;
    }
    if (queue.size() >= 8) {
        outMessage = "Email queue is full. Wait for pending messages to finish.";
        return false;
    }

    const String subj = "Test Email: " + config.institutionName + " Smart Storage System";
    const String body = "Hello,\n\nThis is a verification email from your ECE Smart Storage Rack.\n"
                        "If you received this message, your customer SMTP settings (" + config.smtpProvider + ") are working properly!\n\n"
                        "Institution: " + config.institutionName + "\n"
                        "Sender Name: " + config.senderName + "\n"
                        "SMTP Server: " + config.smtpHost + ":" + String(config.smtpPort) + "\n\n"
                        "System ready for automatic student notifications.";
    QueuedEmail q;
    q.logId = "TEST-" + String(millis());
    q.recipientEmail = testRecipient;
    q.studentName = "Admin Test";
    q.subject = subj;
    q.body = body;
    q.eventType = "TEST";
    q.timestamp = "Queued";
    q.retries = 0;
    queue.push_back(q);
    Serial.printf("[EMAIL] Queued test email for %s\n", testRecipient.c_str());
    outMessage = "Test email queued. Check Email Logs for delivery status.";
    return true;
}

bool EmailManager::retryFailedEmail(const String &logId) {
    for (auto &l : logs) {
        if (l.logId == logId && !l.success) {
            if (queue.size() >= 8) return false;
            QueuedEmail q;
            q.logId = "RETRY-" + String(millis());
            q.recipientEmail = l.recipientEmail;
            q.studentName = l.studentName;
            q.subject = l.subject;
            q.body = "Notification re-transmitted:\n\nSubject: " + l.subject;
            q.eventType = "RETRY";
            q.timestamp = "Queued";
            q.retries = 0;
            queue.push_back(q);
            Serial.printf("[EMAIL] Queued retry for %s (%s)\n", l.studentName.c_str(), l.recipientEmail.c_str());
            return true;
        }
    }
    return false;
}

void EmailManager::loop() {
    QueuedEmail item;
    EmailConfig configSnapshot;
    {
        EmailStateLock lock;
        if (queue.empty()) return;
        if (millis() - lastProcessTime < PROCESS_INTERVAL_MS) return;
        lastProcessTime = millis();

        item = queue.front();
        queue.erase(queue.begin());
        configSnapshot = config;
    }

    String err;
    bool ok = executeSend(configSnapshot, item.recipientEmail, item.subject, item.body, err);

    {
        EmailStateLock lock;
        addLog(item.logId, item.recipientEmail, item.studentName, item.subject, item.eventType, item.timestamp, ok, ok ? "Sent" : err);

        if (!ok && item.retries < 2) {
            item.retries++;
            queue.push_back(item); // Re-queue for another attempt
            Serial.printf("[EMAIL] Delivery failed, re-queued (%d/2): %s\n", item.retries, err.c_str());
        }
    }
}

