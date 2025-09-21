#ifndef TRANSPORT_IMPL_H
#define TRANSPORT_IMPL_H

#include "transport.h"
#include "student.h"
#include "data_processor.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <thread>
#include <chrono>
#include <sstream>

#include <zmq.hpp>
#include "httplib.h"

using namespace std;

// --- Общая вспомогательная функция ---

inline bool isValidDate(const string& date_str) {
    stringstream ss(date_str);
    unsigned int d, m;
    int y;
    char dot1, dot2;

    // Парсит строку в формате DD.MM.YYYY
    // `ss >> ...` вернет `false`, если тип не совпадает (например, "abc" вместо числа)
    if (ss >> d >> dot1 >> m >> dot2 >> y && dot1 == '.' && dot2 == '.') {
        // Убедимся, что после даты нет лишних символов (например, "01.01.1988a")
        char remaining;
        if (!(ss >> remaining)) {
            auto year = chrono::year{y};
            auto month = chrono::month{m};
            auto day = chrono::day{d};

            chrono::year_month_day ymd{year, month, day};
             
            // Вернет true только если дата существует в григорианском календаре
            return ymd.ok();
        }
    }
    return false;
}


inline set<Student> getUniqueStudents() {
    set<Student> students;
    auto readFromFile = [&](const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
             cerr << "Warning: Could not open file " << filename << endl;
             return;
        }

        string line;
        int line_num = 0;
        while (getline(file, line)) {
            line_num++;
            if (line.empty()) continue;

            stringstream ss(line);
            Student s;
            
            // Читаем ID
            if (!(ss >> s.id)) {
                cerr << "Warning: Malformed line " << line_num << " in " << filename 
                          << " (cannot read ID). Skipping." << endl;
                continue;
            }

            // Читает все оставшиеся слова в вектор
            vector<string> parts;
            string part;
            while(ss >> part) {
                parts.push_back(part);
            }

            // Проверяет наличие хотя бы имени и даты (минимум 2 части)
            if (parts.size() < 2) {
                cerr << "Warning: Malformed line " << line_num << " in " << filename << " (not enough parts for name and date). Skipping." << endl;
                continue;
            }

            s.birthDate = parts.back();
            parts.pop_back();

            if (!isValidDate(s.birthDate)) {
                cerr << "Warning: Invalid date '" << s.birthDate << "' on line " << line_num << " in " << filename << ". Skipping." << endl;
                continue;
            }

            if (!parts.empty()) {
                s.lastName = parts.back();
                parts.pop_back();
            }

            string firstName;
            for (size_t i = 0; i < parts.size(); ++i) {
                firstName += parts[i] + (i == parts.size() - 1 ? "" : " ");
            }
            s.firstName = firstName;
            
            students.insert(s);
            
        }
    };
    readFromFile("student_file_1.txt");
    readFromFile("student_file_2.txt");
    return students;
}

// --- Реализации СЕРВЕРНЫХ транспортов ---

class ZmqServerTransport : public IServerTransport {
public:
    void run() override {
        zmq::context_t context(1);
        
        zmq::socket_t publisher(context, ZMQ_PUB);
        publisher.bind("tcp://*:5555");

        zmq::socket_t sync_service(context, ZMQ_REP);
        sync_service.bind("tcp://*:5556");

        cout << "[ZMQ Server Thread] Server started. Waiting for one client..." << endl;

        zmq::message_t request;
        sync_service.recv(request, zmq::recv_flags::none);
        sync_service.send(zmq::buffer(""), zmq::send_flags::none);
        
        cout << "[ZMQ Server Thread] Client synchronized. Publishing data..." << endl;
        
        this_thread::sleep_for(chrono::milliseconds(50));

        
        const string TOPIC = "STUDENTS|";
        auto students = getUniqueStudents();
        for (const auto& student : students) {
            // Добавляет топик к каждому сообщению
            string message = TOPIC + student.toZmqString();
            publisher.send(zmq::buffer(message), zmq::send_flags::none);
        }
        // Добавляет топик к сообщению END
        publisher.send(zmq::buffer(TOPIC + "END"), zmq::send_flags::none);

        cout << "[ZMQ Server Thread] Data sent. Shutting down." << endl;
    }
};

class HttpServerTransport : public IServerTransport {
public:
    void run() override {
        httplib::Server svr;
        svr.Get("/students", [](const httplib::Request&, httplib::Response& res) {
            cout << "[HTTP Server Thread] GET /students request received." << endl;
            auto students = getUniqueStudents();
            json j = students;
            res.set_content(j.dump(4), "application/json");
        });
        cout << "[HTTP Server Thread] Server started on http://localhost:8080" << endl;
        svr.listen("0.0.0.0", 8080);
        cout << "[HTTP Server Thread] Server has shut down." << endl;
    }
};


// --- Реализации КЛИЕНТСКИХ транспортов ---

class ZmqClientTransport : public IClientTransport {
public:
    ZmqClientTransport(DataProcessor& processor) : processor_(processor) {}

    void run() override {
        zmq::context_t context(1);

        zmq::socket_t subscriber(context, ZMQ_SUB);
        subscriber.connect("tcp://localhost:5555");
        
        const string TOPIC = "STUDENTS|";

        subscriber.set(zmq::sockopt::subscribe, TOPIC);

        zmq::socket_t sync_client(context, ZMQ_REQ);
        sync_client.connect("tcp://localhost:5556");

        this_thread::sleep_for(chrono::milliseconds(50));

        cout << "[ZMQ Receiver Thread] Synchronizing with server..." << endl;
        sync_client.send(zmq::buffer(""), zmq::send_flags::none);
        zmq::message_t reply;
        sync_client.recv(reply, zmq::recv_flags::none);
        cout << "[ZMQ Receiver Thread] Synchronization complete. Receiving data..." << endl;

        vector<Student> receivedStudents;
        while (true) {
            zmq::message_t msg;
            auto result = subscriber.recv(msg, zmq::recv_flags::none);
            if (!result.has_value()) {
                cerr << "[ZMQ Receiver Thread] recv failed. Terminating." << endl;
                break;
            }

            string data = msg.to_string();
            
            // Проверяет что сообщение - END с топиком
            if (data == (TOPIC + "END")) {
                break;
            }
            
            // Отделяет полезные данные от топика
            string payload = data.substr(TOPIC.length());
            auto student_opt = Student::fromZmqString(payload); 
            if (student_opt) {
                receivedStudents.push_back(*student_opt); 
            }            
            else {
                // Если optional пуст, значит, данные некорректны
                cerr << "[ZMQ Receiver Thread] Warning: Received malformed student data. Payload: \""  << payload << "\". Skipping." << endl;
            }
        }
        
        if (!receivedStudents.empty()) {
            cout << "[ZMQ Receiver Thread] Received all data (" << receivedStudents.size() << " students). Submitting to processor." << endl;
            processor_.submit_data(move(receivedStudents));
        }
    }
private:
    DataProcessor& processor_;
};

class HttpClientTransport : public IClientTransport {
public:
    HttpClientTransport(DataProcessor& processor) : processor_(processor) {}

    void run() override {
        httplib::Client cli("http://localhost:8080");
        cout << "[HTTP Receiver Thread] Client started. Requesting data from /students..." << endl;
        auto res = cli.Get("/students");
        if (res && res->status == 200) {
try {
    json j = json::parse(res->body);
    std::vector<Student> receivedStudents = j.get<std::vector<Student>>();
    
    if (!receivedStudents.empty()) {
        cout << "[HTTP Receiver Thread] Received all data (" << receivedStudents.size() << " students). Submitting to processor." << endl;
                processor_.submit_data(move(receivedStudents));
    }
} 
catch (const json::exception& e) {
    cerr << "[HTTP Receiver Thread] Error: Failed to parse JSON response. Reason: " << e.what() << std::endl;
        }   
    }   
}
private:
    DataProcessor& processor_;
};

#endif // TRANSPORT_IMPL_H