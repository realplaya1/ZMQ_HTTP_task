#ifndef STUDENT_H
#define STUDENT_H

#include <string>
#include <vector>
#include <sstream>
#include "json.hpp" 

using json = nlohmann::json;
using namespace std;

struct Student {
    int id;
    string firstName;
    string lastName;
    string birthDate;

    // Перегрузка оператора "меньше"
    // Необходима для хранения уникальных студентов в set на сервере.
    // Уникальность определяется по ФИО и дате рождения.
    bool operator<(const Student& other) const {
        if (firstName != other.firstName) {
            return firstName < other.firstName;
        }
        if (lastName != other.lastName) {
            return lastName < other.lastName;
        }
        return birthDate < other.birthDate;
    }

    // Сериализация: из объекта в строку для ZeroMQ
    string toZmqString() const {
        stringstream ss;
        ss << id << " " << firstName << " " << lastName << " " << birthDate;
        return ss.str();
    }

    // Десериализация: из строки ZeroMQ в объект
     static optional<Student> fromZmqString(const string& str) {
        Student s;
        std::stringstream ss(str);
        // Пытаемся прочитать все поля
        if (ss >> s.id >> s.firstName >> s.lastName >> s.birthDate) {
            // Убедимся, что после чтения в строке не осталось "мусора"
            char remaining;
            if (!(ss >> remaining)) {
                return s; // Все хорошо, возвращаем студента
            }
        }
        return std::nullopt; // В случае любой ошибки возвращаем пустоту
    }
};

// Функция для преобразования объекта Student в json
// Вызывается автоматически при json j = student_object;
void to_json(json& j, const Student& s) {
    j = json{
        {"id", s.id},
        {"firstName", s.firstName},
        {"lastName", s.lastName},
        {"birthDate", s.birthDate}
    };
}

// Функция для преобразования json в объект Student
// Вызывается автоматически при Student s = json_object;
void from_json(const json& j, Student& s) {
    j.at("id").get_to(s.id);
    j.at("firstName").get_to(s.firstName);
    j.at("lastName").get_to(s.lastName);
    j.at("birthDate").get_to(s.birthDate);
}

#endif // STUDENT_H