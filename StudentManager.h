#pragma once
#include <vector>
#include "Models.h"

class StudentManager {
private:
    std::vector<Student> students;

public:
    void init();
    std::vector<Student>& getStudents() { return students; }
    
    Student* getByUid(const String &uid);
    Student* getById(const String &studentId);
    Student* getByRegNumber(const String &regNo);
    
    bool registerStudent(const Student &s);
    bool updateStudent(const Student &s);
    bool replaceCard(const String &studentId, const String &newUid);
    bool setStatus(const String &studentId, bool active);
    int getTotalStudents() { return students.size(); }
};

