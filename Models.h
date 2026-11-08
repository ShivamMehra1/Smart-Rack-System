#pragma once
#include <Arduino.h>

enum CompartmentStatus {
    AVAILABLE = 0,
    OCCUPIED = 1,
    MAINTENANCE = 2
};

enum ConfirmationMethod {
    PHYSICAL_BUTTON = 0,
    WEB_DASHBOARD = 1
};

struct Student {
    String studentId;
    String name;
    String regNumber;
    String mobile;
    String email;
    String section;
    String rfidUid;
    String registrationDate;
    bool active;
};

struct Compartment {
    int id;               // Permanent internal ID: 1, 2, 3...
    String displayName;   // e.g. "A1", "ECE-101"
    int row;
    int col;
    CompartmentStatus status;
    String assignedStudentId; // empty if available
    String maintenanceNotes;
};

struct AssignmentRecord {
    String assignmentId;
    String studentId;
    String studentName;
    String regNumber;
    int compartmentId;
    String compartmentName;
    String rfidUid;
    String submissionTimestamp;
    String collectionTimestamp;
    bool isCompleted;
};

struct SystemSettings {
    int totalRows;
    int totalCols;
    String namingFormat;       // "ALPHA_NUM", "SEQUENTIAL", "THREE_DIGIT", "PREFIX"
    String numberingDirection; // "L2R", "R2L", "T2B", "B2T", "ROW_WISE", "COL_WISE"
    String prefix;
    String suffix;
    int startingNumber;
    uint8_t lcdAddress;
    ConfirmationMethod confirmMethod;
    String adminPassword;
    String wifiSsid;
    String wifiPassword;
};

struct EmailConfig {
    bool enabled;
    bool notifyOnSubmission;
    bool notifyOnCollection;
    String institutionName;
    String senderName;
    String senderEmail;
    String smtpProvider;       // "GMAIL", "OUTLOOK", "CUSTOM"
    String smtpHost;
    int smtpPort;
    String securityProtocol;   // "STARTTLS", "SSL_TLS"
    String smtpUsername;
    String smtpPassword;       // App password for Gmail or SMTP password
    String submissionSubject;
    String submissionTemplate;
    String collectionSubject;
    String collectionTemplate;
};

struct EmailLog {
    String logId;
    String recipientEmail;
    String studentName;
    String subject;
    String eventType;          // "SUBMISSION", "COLLECTION", "TEST"
    String timestamp;
    bool success;
    String errorMessage;
    int retryCount;
};
