#ifndef INCLUDE_IEC61850_H_
#define INCLUDE_IEC61850_H_

#include <cstdint>
#include <logger.h>
#include <plugin_api.h>
#include <reading.h>

#include <mutex>
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "iec61850_client_config.hpp"
#include "iec61850_client_connection.hpp"

#define BACKUP_CONNECTION_TIMEOUT 5000

class IEC61850Client;


class PivotTimestamp
{
public:
    PivotTimestamp(Datapoint* timestampData);
    PivotTimestamp(uint64_t ms);
    ~PivotTimestamp();

    void setTimeInMs(uint64_t ms);

    int SecondSinceEpoch();
    int FractionOfSecond();
    uint64_t getTimeInMs();

    bool ClockFailure() {return m_clockFailure;};
    bool LeapSecondKnown() {return m_leapSecondKnown;};
    bool ClockNotSynchronized() {return m_clockNotSynchronized;};
    int TimeAccuracy() {return m_timeAccuracy;};

    static uint64_t GetCurrentTimeInMs();

private:

    void handleTimeQuality(Datapoint* timeQuality);

    uint8_t* m_valueArray;

    int m_secondSinceEpoch;
    int m_fractionOfSecond;

    int m_timeAccuracy;
    bool m_clockFailure = false;
    bool m_leapSecondKnown = false;
    bool m_clockNotSynchronized = false;
};


class IEC61850
{
public:
    typedef void (*INGEST_CB)(void*, Reading);

    IEC61850();
    ~IEC61850();

    void setAssetName(const std::string& asset) { m_asset = asset; }
    void setJsonConfig(const std::string& stack_configuration,
                              const std::string& msg_configuration,
                              const std::string& tls_configuration);

    void start();
    void stop();

    void ingest(std::string assetName, std::vector<Datapoint*>& points);
    void registerIngest(void* data, void (*cb)(void*, Reading));
    bool operation(const std::string& operation, int count,
                   PLUGIN_PARAMETER** params);


private:

    IEC61850ClientConfig* m_config;

    std::string m_asset;

protected:

private:
    INGEST_CB m_ingest = nullptr;  // Callback function used to send data to south service
    void* m_data;        // Ingest function data
    IEC61850Client* m_client = nullptr;
};


class IEC61850Client
{
public:

    explicit IEC61850Client(IEC61850* iec61850, IEC61850ClientConfig* config);

    ~IEC61850Client();

    void sendData(std::vector<Datapoint*> data,
                  const std::vector<std::string> labels);

    void start();

    void stop();
    
    void prepareConnections();
    
    void handleValue(std::string objRef, MmsValue* mmsValue);
    void handleAllValues();

    bool handleOperation(Datapoint* operation);

    void logIedClientError(IedClientError err, std::string info);

    void sendCommandAck(const std::string& label, ControlModel mode, bool terminated);


private:
    std::vector<IEC61850ClientConnection*>* m_connections;
    
    IEC61850ClientConnection* m_active_connection = nullptr;
    std::mutex m_activeConnectionMtx;
    
    enum class ConnectionStatus
    {
        STARTED,
        NOT_CONNECTED
    };

    ConnectionStatus m_connStatus = ConnectionStatus::NOT_CONNECTED;

    void updateConnectionStatus(ConnectionStatus newState);

    std::thread* m_monitoringThread = nullptr;
    void _monitoringThread();

    bool m_started = false;

    IEC61850ClientConfig* m_config;
    IEC61850* m_iec61850;

    template <class T> Datapoint* m_createDatapoint(const std::string& label, const std::string& objRef, T value, Quality quality, uint64_t timestampMs);
    static int getRootFromCDC(const CDCTYPE cdc);
   
    void addQualityDp(Datapoint* cdcDp, Quality quality);
    void addTimestampDp(Datapoint* cdcDp, uint64_t timestampMs);
    template <class T> void addValueDp(Datapoint* cdcDp, CDCTYPE type, T value);

    void m_handleMonitoringData(const std::string& objRef, std::vector<Datapoint*>& datapoints, const std::string& label, CDCTYPE type, MmsValue* mmsValue, const std::string& variable, FunctionalConstraint fc);
    std::unordered_map<std::string, Datapoint*>* m_outstandingCommands = nullptr;
};

#endif  // INCLUDE_IEC61850_H_
