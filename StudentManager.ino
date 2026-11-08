#include "StudentManager.h"
#include "StorageManager.h"

void StudentManager::init() {
    StorageManager::loadStudents(students);
}

Student* StudentManager::getByUid(const String &uid) {
    for (auto &st : students) {
        if (st.rfidUid.equalsIgnoreCase(uid)) return &st;
    }
    return nullptr;
}

Student* StudentManager::getById(const String &studentId) {
    for (auto &st : students) {
        if (st.studentId == studentId) return &st;
    }
    return nullptr;
}

Student* StudentManager::getByRegNumber(const String &regNo) {
    for (auto &st : students) {
        if (st.regNumber.equalsIgnoreCase(regNo)) return &st;
    }
    return nullptr;
}

bool StudentManager::registerStudent(const Student &s) {
    if (s.name.isEmpty() || s.regNumber.isEmpty() || s.rfidUid.isEmpty()) return false;
    if (students.size() >= MAX_STUDENTS) return false;
    if (getByUid(s.rfidUid) != nullptr) return false;
    if (getByRegNumber(s.regNumber) != nullptr) return false;

    students.push_back(s);
    StorageManager::saveStudents(students);
    return true;
}

bool StudentManager::updateStudent(const Student &s) {
    Student* st = getById(s.studentId);
    if (!st) return false;
    st->name = s.name;
    st->mobile = s.mobile;
    st->email = s.email;
    st->section = s.section;
    StorageManager::saveStudents(students);
    return true;
}

bool StudentManager::replaceCard(const String &studentId, const String &newUid) {
    Student* existing = getByUid(newUid);
    if (existing && existing->studentId != studentId) return false;

    Student* st = getById(studentId);
    if (!st) return false;
    st->rfidUid = newUid;
    StorageManager::saveStudents(students);
    return true;
}

bool StudentManager::setStatus(const String &studentId, bool active) {
    Student* st = getById(studentId);
    if (!st) return false;
    st->active = active;
    StorageManager::saveStudents(students);
    return true;
}

