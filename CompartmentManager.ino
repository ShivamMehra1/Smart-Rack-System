#include "CompartmentManager.h"
#include "StorageManager.h"
#include "Config.h"
#include <algorithm>
#include <utility>

void CompartmentManager::init(const SystemSettings &sysSettings) {
    settings = sysSettings;
    if (!StorageManager::loadCompartments(compartments) || compartments.empty()) {
        generateLayout(settings.totalRows, settings.totalCols, settings.namingFormat, 
                       settings.numberingDirection, settings.prefix, settings.startingNumber);
    }
}

void CompartmentManager::generateLayout(int rows, int cols, const String &format, const String &direction, const String &prefix, int startNum) {
    String error;
    if (!configureLayout(rows, cols, format, prefix, startNum, error)) {
        Serial.printf("[RACK] Could not generate layout: %s\n", error.c_str());
    }
}

bool CompartmentManager::configureLayout(int rows, int cols, const String &format, const String &prefix, int startNum, String &error) {
    if (rows < 1 || cols < 1 || rows > MAX_COMPARTMENTS || cols > MAX_COMPARTMENTS || rows * cols > MAX_COMPARTMENTS) {
        error = "Rows and columns must be positive, with no more than 100 compartments total.";
        return false;
    }
    if (format != "ALPHA_NUM" && format != "THREE_DIGIT" && format != "SEQUENTIAL" && format != "PREFIX") {
        error = "Choose a supported compartment label format.";
        return false;
    }
    if (format == "ALPHA_NUM" && rows > 26) {
        error = "Letter-number labels support at most 26 rows (A-Z).";
        return false;
    }

    const size_t newCount = static_cast<size_t>(rows * cols);
    for (const auto &oldBox : compartments) {
        if (oldBox.id > static_cast<int>(newCount) && oldBox.status != AVAILABLE) {
            error = "The smaller layout would remove an occupied or maintenance compartment. Clear those compartments first.";
            return false;
        }
    }

    std::vector<Compartment> next;
    next.reserve(newCount);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int id = r * cols + c + 1;
            Compartment box{};
            const auto previous = std::find_if(compartments.begin(), compartments.end(), [id](const Compartment &item) {
                return item.id == id;
            });
            if (previous != compartments.end()) {
                box = *previous;
            } else {
                box.id = id;
                box.status = AVAILABLE;
                box.assignedStudentId = "";
                box.maintenanceNotes = "";
            }

            box.id = id;
            box.row = r;
            box.col = c;
            if (format == "ALPHA_NUM") {
                box.displayName = String(static_cast<char>('A' + r)) + String(c + 1);
            } else if (format == "THREE_DIGIT") {
                box.displayName = String((r + 1) * 100 + (c + 1));
            } else if (format == "PREFIX") {
                box.displayName = prefix + String(id < 10 ? "0" : "") + String(id);
            } else {
                box.displayName = String(startNum + id - 1);
            }
            next.push_back(box);
        }
    }

    if (!StorageManager::saveCompartments(next)) {
        error = "Could not save the new compartment layout to LittleFS.";
        return false;
    }

    compartments = std::move(next);
    settings.totalRows = rows;
    settings.totalCols = cols;
    settings.namingFormat = format;
    settings.prefix = prefix;
    settings.startingNumber = startNum;
    error = "";
    return true;
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

