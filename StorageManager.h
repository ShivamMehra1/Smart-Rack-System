#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>
#include "Models.h"

class StorageManager {
public:
    static bool init();
    static bool loadSettings(SystemSettings &settings);
    static bool saveSettings(const SystemSettings &settings);
    static bool loadStudents(std::vector<Student> &students);
    static bool saveStudents(const std::vector<Student> &students);
    static bool loadCompartments(std::vector<Compartment> &compartments);
    static bool saveCompartments(const std::vector<Compartment> &compartments);
    static bool loadAssignments(std::vector<AssignmentRecord> &history);
    static bool saveAssignments(const std::vector<AssignmentRecord> &history);
    
    // Customer-Configurable Email System Persistence
    static bool loadEmailConfig(EmailConfig &config);
    static bool saveEmailConfig(const EmailConfig &config);
    static bool loadEmailLogs(std::vector<EmailLog> &logs);
    static bool saveEmailLogs(const std::vector<EmailLog> &logs);
};
