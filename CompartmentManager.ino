#include "CompartmentManager.h"
#include "StorageManager.h"

void CompartmentManager::init(const SystemSettings &sysSettings) {
    settings = sysSettings;
    if (!StorageManager::loadCompartments(compartments) || compartments.empty()) {
        generateLayout(settings.totalRows, settings.totalCols, settings.namingFormat, 
                       settings.numberingDirection, settings.prefix, settings.startingNumber);
    }
}

void CompartmentManager::generateLayout(int rows, int cols, const String &format, const String &direction, const String &prefix, int startNum) {
    std::vector<Compartment> oldList = compartments;
    compartments.clear();
    int idCounter = 1;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            Compartment box;
            box.id = idCounter;
            box.row = r;
            box.col = c;
            box.status = AVAILABLE;
            box.assignedStudentId = "";
            box.maintenanceNotes = "";

            if (format == "ALPHA_NUM") {
                char rowLetter = 'A' + r;
                box.displayName = String(rowLetter) + String(c + 1);
            } else if (format == "THREE_DIGIT") {
                box.displayName = String((r + 1) * 100 + (c + 1));
            } else if (format == "PREFIX") {
                box.displayName = prefix + String(idCounter < 10 ? "0" : "") + String(idCounter);
            } else {
                box.displayName = String(startNum + idCounter - 1);
            }

            for (const auto &oldBox : oldList) {
                if (oldBox.id == box.id && oldBox.status == OCCUPIED) {
                    box.status = OCCUPIED;
                    box.assignedStudentId = oldBox.assignedStudentId;
                    break;
                }
            }

            compartments.push_back(box);
            idCounter++;
        }
    }
    StorageManager::saveCompartments(compartments);
}

Compartment* CompartmentManager::getCompartmentById(int id) {
    for (auto &box : compartments) {
        if (box.id == id) return &box;
    }
    return nullptr;
}

Compartment* CompartmentManager::getFirstAvailable() {
    for (auto &box : compartments) {
        if (box.status == AVAILABLE) return &box;
    }
    return nullptr;
}

Compartment* CompartmentManager::getByStudentId(const String &studentId) {
    if (studentId.isEmpty()) return nullptr;
    for (auto &box : compartments) {
        if (box.status == OCCUPIED && box.assignedStudentId == studentId) return &box;
    }
    return nullptr;
}

bool CompartmentManager::allocateCompartment(int compartmentId, const String &studentId) {
    Compartment* box = getCompartmentById(compartmentId);
    if (!box || box->status != AVAILABLE) return false;
    box->status = OCCUPIED;
    box->assignedStudentId = studentId;
    StorageManager::saveCompartments(compartments);
    return true;
}

bool CompartmentManager::releaseCompartment(int compartmentId) {
    Compartment* box = getCompartmentById(compartmentId);
    if (!box) return false;
    box->status = AVAILABLE;
    box->assignedStudentId = "";
    StorageManager::saveCompartments(compartments);
    return true;
}

bool CompartmentManager::setMaintenance(int compartmentId, bool inMaintenance, const String &notes) {
    Compartment* box = getCompartmentById(compartmentId);
    if (!box) return false;
    if (box->status == OCCUPIED && inMaintenance) return false;
    box->status = inMaintenance ? MAINTENANCE : AVAILABLE;
    box->maintenanceNotes = notes;
    StorageManager::saveCompartments(compartments);
    return true;
}

bool CompartmentManager::updateDisplayName(int compartmentId, const String &newName) {
    Compartment* box = getCompartmentById(compartmentId);
    if (!box || newName.isEmpty()) return false;
    box->displayName = newName;
    StorageManager::saveCompartments(compartments);
    return true;
}

int CompartmentManager::getAvailableCount() {
    int cnt = 0;
    for (const auto &b : compartments) if (b.status == AVAILABLE) cnt++;
    return cnt;
}

int CompartmentManager::getOccupiedCount() {
    int cnt = 0;
    for (const auto &b : compartments) if (b.status == OCCUPIED) cnt++;
    return cnt;
}

int CompartmentManager::getMaintenanceCount() {
    int cnt = 0;
    for (const auto &b : compartments) if (b.status == MAINTENANCE) cnt++;
    return cnt;
}

