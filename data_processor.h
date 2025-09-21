#ifndef DATA_PROCESSOR_H
#define DATA_PROCESSOR_H

#include <vector>
#include <thread>
#include <iostream>
#include <algorithm>
#include "student.h"
#include "thread_queue.h"

using namespace std;

class DataProcessor {
public:
    // Запускает внутренний поток
    void start() {
        worker_ = thread(&DataProcessor::process_loop, this);
    }

    // Останавливает внутренний поток и дожидается его завершения
    void stop() {
        queue_.stop();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    // Метод для потока-приемника, чтобы добавить данные в очередь
    void submit_data(vector<Student> data) {
        queue_.push(move(data));
    }

private:
    void process_loop() {
        vector<Student> student_batch;
        while (queue_.wait_and_pop(student_batch)) {
            cout << "[Processing Thread] Received a batch of " << student_batch.size() << " students. Processing..." << endl;
            sort_and_display(student_batch);
        }
        cout << "[Processing Thread] Finished." << endl;
    }

    void sort_and_display(vector<Student>& students) {
        sort(students.begin(), students.end(), [](const Student& a, const Student& b) {
            if (a.lastName != b.lastName) return a.lastName < b.lastName;
            return a.firstName < b.firstName;
        });

        cout << "\n--- Sorted Students ---" << endl;
        for (const auto& student : students) {
            cout << student.id << " " << student.firstName << " " << student.lastName << " " << student.birthDate << endl;
        }
    }

    ThreadQueue<vector<Student>> queue_;
    thread worker_;
};

#endif // DATA_PROCESSOR_H