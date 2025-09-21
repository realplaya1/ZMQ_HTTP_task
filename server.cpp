#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include "transport_impl.h"

using namespace std;

int main() {
    // Создаем экземпляры обоих серверов
    auto zmq_server = make_unique<ZmqServerTransport>();
    auto http_server = make_unique<HttpServerTransport>();

    cout << "[Main Thread] Starting all servers in separate threads..." << endl;

    // Запускаем каждый сервер в своем потоке
    thread zmq_thread([&zmq_server]() {
        try {
            zmq_server->run();
        } catch (const exception& e) {
            cerr << "ZMQ Server crashed: " << e.what() << endl;
        }
    });

    thread http_thread([&http_server]() {
        try {
            http_server->run();
        } catch (const exception& e) {
            cerr << "HTTP Server crashed: " << e.what() << endl;
        }
    });

    cout << "[Main Thread] Servers are running. Press Ctrl+C to stop." << endl;

    // Дожидаемся завершения работы потоков
    if (zmq_thread.joinable()) {
        zmq_thread.join();
    }
    if (http_thread.joinable()) {
        http_thread.join();
    }

    cout << "[Main Thread] All servers have shut down." << endl;

    return 0;
}