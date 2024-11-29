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
    ImIpmiJobType_showAllSdr = 1,
    ImIpmiJobType_updateTargetSensors = 2
} eImIpmiJobType;

struct ImIpmiJob {
    eImIpmiJobType mType;
    int mTarget;
};

struct ImIpmiSensor {
    std::string mName;
    std::string mUnits;
    // 0 LOWER_NON_RECOV
    // 1 LOWER_CRIT
    // 2 LOWER_NON_CRIT
    double mLowerThresholds[3];
    // 0 UPPER_NON_RECOV
    // 1 UPPER_CRIT
    // 2 UPPER_NON_CRIT
    double mUpperThresholds[3];
    std::vector<double> mValues;
};

struct ImIpmiTarget {
    int mAddress;
    std::map<std::string,ImIpmiSensor> mSensors;
};

struct ImIpmiThread;
struct ImIpmiHost {
    ImIpmiHost(const char *hostname);
    ~ImIpmiHost();
    bool connect();
    void disconnect();
    void keepalive();
    bool check();
    bool bridge(uint8_t target_addr);
    bool list_sdr();
    bool update_target_sdr(uint8_t target_addr);
    void request_work(ImIpmiJob job);
    void dump(uint8_t target_addr);

    std::string mHostname;
    ipmi_intf *mIntf;
    struct ImIpmiThread *mThread;
    std::map<int,ImIpmiTarget> mTargets;
    std::vector<int> mAllTargetAddresses;
};

struct ImIpmiThread {
    ImIpmiThread(ImIpmiHost *host);
    ~ImIpmiThread();
    static void *run(void *instance);
    void *do_run();
    void start();
    bool join();
    bool terminate();
    void request_work(ImIpmiJob job);

    ImIpmiHost *mHost;
    pthread_t mThreadId;
    pthread_mutex_t mMutex;
    pthread_cond_t mCond;
    bool mTerminate;
    std::list<ImIpmiJob> mJobs;
};


struct ImIpmi {
    ~ImIpmi();
    ImIpmiHost *addHost(const char *hostname);
    void removeHost(const char *hostname);
    void showHosts();

    // std::vector<ImIpmiHost *> mHosts;
    std::map<std::string,ImIpmiHost *> mHosts;
};

ImIpmi *initContext();
void destroyContext(ImIpmi *ctx);

// ImIpmiHost *createHost(const char *hostname);
// void destroyHost(ImIpmiHost *host);
