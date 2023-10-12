#ifndef IEC104_CLIENT_CONNECTION_H
#define IEC104_CLIENT_CONNECTION_H

#include "iec61850_client_config.hpp"
#include <libiec61850/iec61850_client.h>
#include <mutex>
#include <thread>

class IEC61850Client;

class IEC61850ClientConnection
{
public:
    IEC61850ClientConnection(IEC61850Client* client, IEC61850ClientConfig* config, const std::string& ip, int tcpPort);
    ~IEC61850ClientConnection();

    void Start();
    void Stop();
    void Activate();

    void Disconnect();
    void Connect();

    bool Disconnected() {return ((m_connecting == false) && (m_connected == false));};
    bool Connecting() {return m_connecting;};
    bool Connected() {return m_connected;};
    bool Active() {return m_active;};

private:
    bool prepareConnection();
    IedConnection m_connection = nullptr; 
    void executePeriodicTasks();

    IEC61850Client* m_client;
    IEC61850ClientConfig* m_config;

    typedef enum {
        CON_STATE_IDLE,
        CON_STATE_CONNECTING,
        CON_STATE_CONNECTED,
        CON_STATE_CLOSED,
        CON_STATE_WAIT_FOR_RECONNECT,
        CON_STATE_FATAL_ERROR
    } ConState;
    
    ConState m_connectionState = CON_STATE_IDLE;

    int  m_tcpPort;
    std::string m_serverIp;  
    bool m_connected = false;
    bool m_active = false; 
    bool m_connecting = false;
    bool m_started = false;

    std::mutex m_conLock;
    
    uint64_t m_delayExpirationTime;

    std::thread* m_conThread = nullptr;
    void _conThread();

    bool m_connect = false; 
    bool m_disconnect = false;
};

#endif


