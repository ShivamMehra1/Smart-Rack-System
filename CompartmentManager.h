#pragma once
#include <vector>
#include "Models.h"

class CompartmentManager {
private:
    std::vector<Compartment> compartments;
    SystemSettings settings;

public:
    void init(const SystemSettings &sysSettings);
    void generateLayout(int rows, int cols, const String &format, const String &direction, const String &prefix, int startNum);
    bool configureLayout(int rows, int cols, const String &format, const String &prefix, int startNum, String &error);
    
    std::vector<Compartment>& getCompartments() { return compartments; }
    Compartment* getCompartmentById(int id);
    Compartment* getFirstAvailable();
    Compartment* getByStudentId(const String &studentId);

    bool allocateCompartment(int compartmentId, const String &studentId);
    bool releaseCompartment(int compartmentId);
    bool setMaintenance(int compartmentId, bool inMaintenance, const String &notes = "");
    bool updateDisplayName(int compartmentId, const String &newName);
    
    int getAvailableCount();
    int getOccupiedCount();
    int getMaintenanceCount();
    int getTotalCount() { return compartments.size(); }
};

