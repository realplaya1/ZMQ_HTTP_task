#ifndef TRANSPORT_H
#define TRANSPORT_H

// Абстрактный базовый класс для любого сервера
class IServerTransport {
public:
    virtual ~IServerTransport() = default;
    virtual void run() = 0; 
};

// Абстрактный базовый класс для любого клиента
class IClientTransport {
public:
    virtual ~IClientTransport() = default;
    virtual void run() = 0;
};

#endif // TRANSPORT_H