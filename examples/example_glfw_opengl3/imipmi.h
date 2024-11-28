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

#include <pthread.h>


struct ImIpmiHost {
    ImIpmiHost(const char *hostname);
    ~ImIpmiHost();
    bool connect();
    void disconnect();
    void keepalive();
    bool check();
    bool bridge(uint8_t target_addr);
    bool list_sdr();

    std::string mHostname;
    ipmi_intf *mIntf;
};

struct ImIpmiThread {
    ImIpmiThread(const char *hostname);
    ~ImIpmiThread();
    static void *run(void *instance);
    void *do_run();
    void start();
    bool join();
    bool terminate();
    void request_work();

    std::string mHostname;
    ImIpmiHost *mHost;
    pthread_t mThreadId;
    pthread_mutex_t mMutex;
    pthread_cond_t mCond;
    bool mTerminate;
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
