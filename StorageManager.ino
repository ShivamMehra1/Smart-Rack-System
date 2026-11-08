#include "StorageManager.h"

bool StorageManager::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("[STORAGE] LittleFS Mount Failed!");
        return false;
    }
    Serial.println("[STORAGE] LittleFS Mounted Successfully.");
    return true;
}

bool StorageManager::loadSettings(SystemSettings &s) {
    if (!LittleFS.exists("/config.json")) {
        s.totalRows = 4;
        s.totalCols = 6;
        s.namingFormat = "ALPHA_NUM";
        s.numberingDirection = "ROW_WISE";
        s.prefix = "ECE-";
        s.suffix = "";
        s.startingNumber = 1;
        s.lcdAddress = 0x27;
        s.confirmMethod = PHYSICAL_BUTTON;
        s.adminPassword = "admin";
        s.wifiSsid = "";
        s.wifiPassword = "";
        saveSettings(s);
        return true;
    }

    File f = LittleFS.open("/config.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, f);
    f.close();

    s.totalRows = doc["totalRows"] | 4;
    s.totalCols = doc["totalCols"] | 6;
    s.namingFormat = doc["namingFormat"] | "ALPHA_NUM";
    s.numberingDirection = doc["numberingDirection"] | "ROW_WISE";
    s.prefix = doc["prefix"] | "ECE-";
    s.suffix = doc["suffix"] | "";
    s.startingNumber = doc["startingNumber"] | 1;
    s.lcdAddress = doc["lcdAddress"] | 0x27;
    s.confirmMethod = (ConfirmationMethod)(doc["confirmMethod"] | 0);
    s.adminPassword = doc["adminPassword"] | "admin";
    s.wifiSsid = doc["wifiSsid"] | "";
    s.wifiPassword = doc["wifiPassword"] | "";
    return true;
}

bool StorageManager::saveSettings(const SystemSettings &s) {
    DynamicJsonDocument doc(2048);
    doc["totalRows"] = s.totalRows;
    doc["totalCols"] = s.totalCols;
    doc["namingFormat"] = s.namingFormat;
    doc["numberingDirection"] = s.numberingDirection;
    doc["prefix"] = s.prefix;
    doc["suffix"] = s.suffix;
    doc["startingNumber"] = s.startingNumber;
    doc["lcdAddress"] = s.lcdAddress;
    doc["confirmMethod"] = (int)s.confirmMethod;
    doc["adminPassword"] = s.adminPassword;
    doc["wifiSsid"] = s.wifiSsid;
    doc["wifiPassword"] = s.wifiPassword;

    File f = LittleFS.open("/config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool StorageManager::loadStudents(std::vector<Student> &students) {
    students.clear();
    if (!LittleFS.exists("/students.json")) return true;

    File f = LittleFS.open("/students.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        Student st;
        st.studentId = obj["studentId"].as<String>();
        st.name = obj["name"].as<String>();
        st.regNumber = obj["regNumber"].as<String>();
        st.mobile = obj["mobile"].as<String>();
        st.email = obj["email"].as<String>();
        st.section = obj["section"].as<String>();
        st.rfidUid = obj["rfidUid"].as<String>();
        st.registrationDate = obj["registrationDate"].as<String>();
        st.active = obj["active"].as<bool>();
        students.push_back(st);
    }
    return true;
}

bool StorageManager::saveStudents(const std::vector<Student> &students) {
    DynamicJsonDocument doc(16384);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &st : students) {
        JsonObject obj = arr.createNestedObject();
        obj["studentId"] = st.studentId;
        obj["name"] = st.name;
        obj["regNumber"] = st.regNumber;
        obj["mobile"] = st.mobile;
        obj["email"] = st.email;
        obj["section"] = st.section;
        obj["rfidUid"] = st.rfidUid;
        obj["registrationDate"] = st.registrationDate;
        obj["active"] = st.active;
    }
    File f = LittleFS.open("/students.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool StorageManager::loadCompartments(std::vector<Compartment> &compartments) {
    compartments.clear();
    if (!LittleFS.exists("/compartments.json")) return true;

    File f = LittleFS.open("/compartments.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        Compartment c;
        c.id = obj["id"].as<int>();
        c.displayName = obj["displayName"].as<String>();
        c.row = obj["row"].as<int>();
        c.col = obj["col"].as<int>();
        c.status = (CompartmentStatus)obj["status"].as<int>();
        c.assignedStudentId = obj["assignedStudentId"].as<String>();
        c.maintenanceNotes = obj["maintenanceNotes"].as<String>();
        compartments.push_back(c);
    }
    return true;
}

bool StorageManager::saveCompartments(const std::vector<Compartment> &compartments) {
    DynamicJsonDocument doc(16384);
    JsonArray arr = doc.to<JsonArray>();
    for (const auto &c : compartments) {
        JsonObject obj = arr.createNestedObject();
        obj["id"] = c.id;
        obj["displayName"] = c.displayName;
        obj["row"] = c.row;
        obj["col"] = c.col;
        obj["status"] = (int)c.status;
        obj["assignedStudentId"] = c.assignedStudentId;
        obj["maintenanceNotes"] = c.maintenanceNotes;
    }
    File f = LittleFS.open("/compartments.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool StorageManager::loadAssignments(std::vector<AssignmentRecord> &history) {
    history.clear();
    if (!LittleFS.exists("/assignments.json")) return true;

    File f = LittleFS.open("/assignments.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(24576);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        AssignmentRecord r;
        r.assignmentId = obj["assignmentId"].as<String>();
        r.studentId = obj["studentId"].as<String>();
        r.studentName = obj["studentName"].as<String>();
        r.regNumber = obj["regNumber"].as<String>();
        r.compartmentId = obj["compartmentId"].as<int>();
        r.compartmentName = obj["compartmentName"].as<String>();
        r.rfidUid = obj["rfidUid"].as<String>();
        r.submissionTimestamp = obj["submissionTimestamp"].as<String>();
        r.collectionTimestamp = obj["collectionTimestamp"].as<String>();
        r.isCompleted = obj["isCompleted"].as<bool>();
        history.push_back(r);
    }
    return true;
}

bool StorageManager::saveAssignments(const std::vector<AssignmentRecord> &history) {
    DynamicJsonDocument doc(24576);
    JsonArray arr = doc.to<JsonArray>();
    int startIdx = max(0, (int)history.size() - MAX_HISTORY);
    for (size_t i = startIdx; i < history.size(); i++) {
        const auto &r = history[i];
        JsonObject obj = arr.createNestedObject();
        obj["assignmentId"] = r.assignmentId;
        obj["studentId"] = r.studentId;
        obj["studentName"] = r.studentName;
        obj["regNumber"] = r.regNumber;
        obj["compartmentId"] = r.compartmentId;
        obj["compartmentName"] = r.compartmentName;
        obj["rfidUid"] = r.rfidUid;
        obj["submissionTimestamp"] = r.submissionTimestamp;
        obj["collectionTimestamp"] = r.collectionTimestamp;
        obj["isCompleted"] = r.isCompleted;
    }
    File f = LittleFS.open("/assignments.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool StorageManager::loadEmailConfig(EmailConfig &cfg) {
    if (!LittleFS.exists("/email_config.json")) {
        cfg.enabled = false;
        cfg.notifyOnSubmission = true;
        cfg.notifyOnCollection = true;
        cfg.institutionName = "ECE Department Lab";
        cfg.senderName = "ECE Smart Storage Rack";
        cfg.senderEmail = "";
        cfg.smtpProvider = "GMAIL";
        cfg.smtpHost = "smtp.gmail.com";
        cfg.smtpPort = 465;
        cfg.securityProtocol = "SSL_TLS";
        cfg.smtpUsername = "";
        cfg.smtpPassword = "";
        cfg.submissionSubject = "Bag Deposited in Compartment {{compartment_name}} - {{institution_name}}";
        cfg.submissionTemplate = "Dear {{student_name}},\n\nYour bag has been placed into Compartment: {{compartment_name}} at {{institution_name}}.\n\nRegistration No: {{reg_number}}\nSection: {{section}}\nTimestamp: {{timestamp}}\n\nPlease keep your RFID card safe for retrieving your bag.\n\nRegards,\n{{institution_name}} Admin";
        cfg.collectionSubject = "Bag Retrieved from Compartment {{compartment_name}} - {{institution_name}}";
        cfg.collectionTemplate = "Dear {{student_name}},\n\nThis is to confirm that your bag was collected from Compartment: {{compartment_name}} at {{timestamp}}.\n\nCompartment {{compartment_name}} is now marked AVAILABLE.\n\nRegards,\n{{institution_name}} Admin";
        saveEmailConfig(cfg);
        return true;
    }

    File f = LittleFS.open("/email_config.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    cfg.enabled = doc["enabled"] | false;
    cfg.notifyOnSubmission = doc["notifyOnSubmission"] | true;
    cfg.notifyOnCollection = doc["notifyOnCollection"] | true;
    cfg.institutionName = doc["institutionName"] | "ECE Department Lab";
    cfg.senderName = doc["senderName"] | "ECE Smart Storage Rack";
    cfg.senderEmail = doc["senderEmail"] | "";
    cfg.smtpProvider = doc["smtpProvider"] | "GMAIL";
    cfg.smtpHost = doc["smtpHost"] | "smtp.gmail.com";
    cfg.smtpPort = doc["smtpPort"] | 465;
    cfg.securityProtocol = doc["securityProtocol"] | "SSL_TLS";
    cfg.smtpUsername = doc["smtpUsername"] | "";
    cfg.smtpPassword = doc["smtpPassword"] | "";
    cfg.submissionSubject = doc["submissionSubject"] | "Bag Deposited in Compartment {{compartment_name}}";
    cfg.submissionTemplate = doc["submissionTemplate"] | "";
    cfg.collectionSubject = doc["collectionSubject"] | "Bag Retrieved from Compartment {{compartment_name}}";
    cfg.collectionTemplate = doc["collectionTemplate"] | "";
    return true;
}

bool StorageManager::saveEmailConfig(const EmailConfig &cfg) {
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
    doc["smtpPassword"] = cfg.smtpPassword;
    doc["submissionSubject"] = cfg.submissionSubject;
    doc["submissionTemplate"] = cfg.submissionTemplate;
    doc["collectionSubject"] = cfg.collectionSubject;
    doc["collectionTemplate"] = cfg.collectionTemplate;

    File f = LittleFS.open("/email_config.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}

bool StorageManager::loadEmailLogs(std::vector<EmailLog> &logs) {
    logs.clear();
    if (!LittleFS.exists("/email_logs.json")) return true;

    File f = LittleFS.open("/email_logs.json", "r");
    if (!f) return false;

    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
        EmailLog l;
        l.logId = obj["logId"].as<String>();
        l.recipientEmail = obj["recipientEmail"].as<String>();
        l.studentName = obj["studentName"].as<String>();
        l.subject = obj["subject"].as<String>();
        l.eventType = obj["eventType"].as<String>();
        l.timestamp = obj["timestamp"].as<String>();
        l.success = obj["success"].as<bool>();
        l.errorMessage = obj["errorMessage"].as<String>();
        l.retryCount = obj["retryCount"].as<int>();
        logs.push_back(l);
    }
    return true;
}

bool StorageManager::saveEmailLogs(const std::vector<EmailLog> &logs) {
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();
    int startIdx = max(0, (int)logs.size() - 30); // Keep last 30 logs
    for (size_t i = startIdx; i < logs.size(); i++) {
        const auto &l = logs[i];
        JsonObject obj = arr.createNestedObject();
        obj["logId"] = l.logId;
        obj["recipientEmail"] = l.recipientEmail;
        obj["studentName"] = l.studentName;
        obj["subject"] = l.subject;
        obj["eventType"] = l.eventType;
        obj["timestamp"] = l.timestamp;
        obj["success"] = l.success;
        obj["errorMessage"] = l.errorMessage;
        obj["retryCount"] = l.retryCount;
    }
    File f = LittleFS.open("/email_logs.json", "w");
    if (!f) return false;
    serializeJson(doc, f);
    f.close();
    return true;
}
