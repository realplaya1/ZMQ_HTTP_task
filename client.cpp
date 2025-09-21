#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include "transport_impl.h"
#include "data_processor.h"

using namespace std;

void print_usage() {
    cout << "Usage: ./client <transport>" << endl;
    cout << "Available transports: zmq, http" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }

    // Создает и запускает единственный поток-обработчик данных
    DataProcessor processor;
    processor.start();
    cout << "[Main Thread] Data processing thread started." << endl;


    // Выбирает и создает транспорт для потока-приемника,
    // передает ему ссылку на обработчик
    string transport_type = argv[1];
    unique_ptr<IClientTransport> client_transport;

    if (transport_type == "zmq") {
        client_transport = make_unique<ZmqClientTransport>(processor);
    } else if (transport_type == "http") {
        client_transport = make_unique<HttpClientTransport>(processor);
    } else {
        cerr << "Error: Unknown transport type '" << transport_type << "'" << endl;
        processor.stop(); // Остановка обработчика перед выходом
        print_usage();
        return 1;
    }

    // Запускает поток-приемник
    cout << "[Main Thread] Starting receiver thread for transport: " << transport_type << endl;
    thread receiver_thread([&client_transport]() {
        try {
            client_transport->run();
        } catch (const exception& e) {
            cerr << "Receiver thread crashed: " << e.what() << endl;
        }
    });

    // Ожидание завершения потока-приемника
    if (receiver_thread.joinable()) {
        receiver_thread.join();
    }
    cout << "[Main Thread] Receiver thread finished." << endl;

    cout << "[Main Thread] Stopping data processor..." << endl;
    processor.stop();

    cout << "[Main Thread] Client has shut down." << endl;
    return 0;
}