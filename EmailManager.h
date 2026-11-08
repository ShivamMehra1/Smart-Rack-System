#pragma once
#include <vector>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "Models.h"

struct QueuedEmail {
    String logId;
    String recipientEmail;
    String studentName;
    String subject;
    String body;
    String eventType;
    String timestamp;
    int retries;
};

class EmailManager {
private:
    EmailConfig config;
    std::vector<EmailLog> logs;
    std::vector<QueuedEmail> queue;
    unsigned long lastProcessTime;
    const unsigned long PROCESS_INTERVAL_MS = 2000;

    String interpolateTemplate(const String &tpl, const Student *s, const String &boxName, const String &timestamp);
    bool executeSend(const EmailConfig &cfg, const String &to, const String &subject, const String &body, String &errorMsg);

public:
    EmailManager();
    void init();
    void loop();

    EmailConfig& getConfig() { return config; }
    void updateConfig(const EmailConfig &newCfg);

    bool queueSubmissionEmail(const Student &student, const String &boxName, const String &timestamp);
    bool queueCollectionEmail(const Student &student, const String &boxName, const String &timestamp);
    bool sendTestEmail(const String &testRecipient, String &outMessage);
    bool retryFailedEmail(const String &logId);

    const std::vector<EmailLog>& getLogs() const { return logs; }
    void addLog(const String &logId, const String &to, const String &student, const String &subject, const String &type, const String &timestamp, bool success, const String &error);
};

