#pragma once

extern "C" {
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_picmg.h>
#include <ipmitool/ipmi_sdr.h>
#include <ipmitool/ipmi_sensor.h>
}

#include <vector>
#include <map>
#include <string>
#include <list>
#include <array>

#include <pthread.h>

typedef enum {
    JobType_listSDR = 1,
} eJobType;

struct Job {
    eJobType mType;
};

struct SensorReading {
    double value;
    uint64_t ts;
    unsigned status;
};

struct SensorInfo {
    const char *host;
    int recordType;
    int sensorType;
    int entityId;
    int entityInstance;
    char name[20];
    char units[20];
    double unr;     // unr = upper non-recoverable
    double ucr;     // ucr = upper critical
    double unc;     // unc = upper non-critical
    double lnc;     // lnc = lower non-critical
    double lcr;     // lcr = lower critical
    double lnr;     // lnr = lower non-recoverable
    bool threshAvailable;
};

struct Sensor {
    Sensor(const char *host, const int record_type, const int sensor_type, const char *name, const int entity_id, const int entity_instance);
    void SetUnits(const char *units);
    void AddReading(const double value, const uint64_t ts, const unsigned status);
    void SetThresholds(const bool available, const double unr, const double ucr, const double unc, const double lnr, const double lcr, const double lnc);
    static unsigned MakeUID(const int type, const uint8_t *rec);

    SensorInfo info;
    std::vector<SensorReading> values; 
};

struct Context;
struct Host {
    Host(const char *host, Context *ctx);
    ~Host();
    bool Connect();
    void Disconnect();
    void KeepAlive();
    bool CheckIPMIVersion();
    bool ListSDR();
    void RequestWork(Job job);
    void Dump();
    static void *Run(void *instance);
    void *RunThread();
    bool JoinThread();
    bool TerminateThread();
    Sensor *CreateSensor(const int record_type, const int sensor_type, const char *name, const int entity_id, const int entity_instance);
    Sensor *GetSensor(const unsigned uid);

    char hostname[32];
    Context *ctx;
    ipmi_intf *ipmiIntf;
    pthread_t threadId;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool terminate;
    std::list<Job> jobs;
    std::map<unsigned,Sensor *> sensors;
};

struct Context {
    ~Context();
    Host *AddHost(const char *hostname);
    void RemoveHost(const char *hostname);
    void ShowHosts();
    
    std::map<std::string,Host *> hosts;
};

Context *InitContext();
void DestroyContext(Context *ctx);
