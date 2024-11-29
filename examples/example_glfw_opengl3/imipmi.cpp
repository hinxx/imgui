#include <cassert>

#include "imipmi.h"

#define DEBUG(fmt, ...) printf("DEBUG: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...)  printf("INFO: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...)  printf("WARN: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ERROR(fmt, ...) printf("ERROR: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

ImIpmi *initContext() {
    ImIpmi *ctx = new ImIpmi();
    return ctx;
}

void destroyContext(ImIpmi *ctx) {
    delete ctx;
}

ImIpmi::~ImIpmi() {
    DEBUG(">>>");
    for (auto it = std::begin(mHosts); it != std::end(mHosts); ++it) {
        delete it->second;
        it->second = nullptr;
    }
    mHosts.clear();
}

ImIpmiHost *ImIpmi::addHost(const char *hostname) {
    DEBUG(">>>");
    showHosts();
    if (mHosts.count(hostname)) {
        ERROR("host object for hostname %s already exist", hostname);
        return nullptr;
    }
    ImIpmiHost *host = new ImIpmiHost(hostname);
    mHosts[hostname] = host;
    return host;
}

void ImIpmi::removeHost(const char *hostname) {
    DEBUG(">>>");
    showHosts();
    auto it = mHosts.find(hostname);
    if (it != mHosts.end()) {
        DEBUG("removing %s from the context", hostname);
        delete it->second;
        mHosts.erase(it);
    }
    showHosts();
    assert(mHosts.count(hostname) == 0);
}

void ImIpmi::showHosts() {
    for (auto it = std::begin(mHosts); it != std::end(mHosts); ++it) {
        DEBUG("host %s, %p", it->first.c_str(), it->second);
    }
}

ImIpmiHost::ImIpmiHost(const char *hostname)
    : mHostname(hostname),
    mIntf(nullptr),
    mThread(nullptr) {

    DEBUG(">>>");

    // AMC4
    mAllTargetAddresses.push_back(0x78);
    // CU1
    mAllTargetAddresses.push_back(0xa8);
    connect();
}

ImIpmiHost::~ImIpmiHost() {
    DEBUG(">>>");
    disconnect();
}

bool ImIpmiHost::connect() {
    INFO("connecting to %s", mHostname.c_str());
    ipmi_intf *intf = ipmi_intf_load((char *)"lan");
    if (! intf) {
        ERROR("cannot load lan interface");
        return false;
    }

    ipmi_intf_session_set_hostname(intf, const_cast<char *>(mHostname.c_str()));
    // empty username and password
    ipmi_intf_session_set_username(intf, (char *)"");
    ipmi_intf_session_set_password(intf, (char *)"");
    ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
	uint8_t kgkey[IPMI_KG_BUFFER_SIZE] = {0};
    ipmi_intf_session_set_kgkey(intf, kgkey);
    ipmi_intf_session_set_lookupbit(intf, 0x10);

    if (! intf->open || (intf->open(intf) == -1)) {
        ERROR("failed to connect to %s", mHostname.c_str());
        ipmi_cleanup(intf);
        return false;
    }

    intf->my_addr = IPMI_BMC_SLAVE_ADDR;
    intf->target_addr = intf->my_addr;
    mIntf = intf;

    INFO("connected to %s", mHostname.c_str());

    if (! check()) {
        disconnect();
        return false;
    }

    mThread = new ImIpmiThread(this);
    mThread->start();

    return true;
}

void ImIpmiHost::disconnect() {
    DEBUG(">>>");
	if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return;
    }

    if (mThread) {
        mThread->terminate();
        mThread = nullptr;
    }

    ipmi_cleanup(mIntf);
	if (mIntf->opened && mIntf->close) {
        mIntf->close(mIntf);
        INFO("disconnected from host %s", mHostname.c_str());
    }
    mIntf = nullptr;
}

void ImIpmiHost::keepalive() {
    DEBUG(">>>");
    if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return;
    }
    mIntf->keepalive(mIntf);
}

bool ImIpmiHost::check() {
    // from ipmitool, ipmi_main.c
    ipmi_rq req;
    bool version_accepted = false;

    DEBUG(">>>");
    if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return false;
    }

    memset(&req, 0, sizeof(req));
    req.msg.netfn = IPMI_NETFN_PICMG;
    req.msg.cmd = PICMG_GET_PICMG_PROPERTIES_CMD;
    uint8_t msg_data = 0x00;
    req.msg.data = &msg_data;
    req.msg.data_len = 1;
    msg_data = 0;

    const ipmi_rs *const rsp = mIntf->sendrecv(mIntf, &req);
    if (rsp && !rsp->ccode) {
        if (rsp->data[0] == 0) {
            if (((rsp->data[1] & 0x0F) == PICMG_ATCA_MAJOR_VERSION ||
                 (rsp->data[1] & 0x0F) == PICMG_AMC_MAJOR_VERSION ||
                 (rsp->data[1] & 0x0F) == PICMG_UTCA_MAJOR_VERSION)) {
                version_accepted = true;
            }
            DEBUG("PICMG %d.%d detected", (int)(rsp->data[1] & 0x0F), (int)((rsp->data[1] & 0xF0) >> 4));
        }
    }

    DEBUG("PICMG %s accepted", version_accepted ? "" : "not ");
    return version_accepted;
}

bool ImIpmiHost::bridge(uint8_t target_addr) {
    uint8_t target_channel = 7;
    uint8_t transit_addr = 0x82;
    uint8_t transit_channel = 0;

    DEBUG(">>>");
    if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return false;
    }

    // // if bridging addresses are specified, handle them
    // if (transit_addr > 0 || target_addr > 0) {
    //     if ((transit_addr != 0 || transit_channel != 0) && target_addr == 0) {
    //         ERROR("transit address/channel 0x%x/0x%x ignored, target address must be specified!",
    //                 transit_addr, transit_channel);
    //         return false;
    //     }
    if (target_addr > 0) {
        mIntf->target_addr = target_addr;
        mIntf->target_channel = target_channel;

        mIntf->transit_addr = transit_addr;
        mIntf->transit_channel = transit_channel;

        /* must be admin level to do this over lan */
        // ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
        /* Get the ipmb address of the targeted entity */
        // intf_->target_ipmb_addr = ipmi_acquire_ipmb_address(intf_);
        mIntf->target_ipmb_addr = ipmi_picmg_ipmb_address(mIntf);
        DEBUG("specified addressing: target 0x%x:0x%x transit 0x%x:0x%x",
                mIntf->target_addr, mIntf->target_channel, mIntf->transit_addr, mIntf->transit_channel);
        if (mIntf->target_ipmb_addr) {
            INFO("discovered target IPMB-0 address 0x%x", mIntf->target_ipmb_addr);
        }
    } else {
        mIntf->target_addr = mIntf->my_addr;
        mIntf->target_channel = 0;

        mIntf->transit_addr = 0;
        mIntf->transit_channel = 0;
        mIntf->target_ipmb_addr = 0;
    }

    DEBUG("interface address: my_addr 0x%x transit 0x%x:0x%x target 0x%x:0x%x ipmb_target 0x%x",
            mIntf->my_addr, mIntf->transit_addr, mIntf->transit_channel, mIntf->target_addr,
            mIntf->target_channel, mIntf->target_ipmb_addr);

    return true;
}

bool ImIpmiHost::list_sdr() {
    struct sdr_get_rs *header;
    struct ipmi_sdr_iterator *itr;

    DEBUG(">>>");
    if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return false;
    }

    DEBUG("querying SDR for sensor list");

    itr = ipmi_sdr_start(mIntf, 0);
    if (! itr) {
        ERROR("unable to open SDR for reading");
        return false;
    }

    size_t count = 0;
    while ((header = ipmi_sdr_get_next_header(mIntf, itr))) {
        uint8_t *rec;

        rec = ipmi_sdr_get_record(mIntf, header, itr);
        if (!rec) {
            ERROR("rec == NULL");
            continue;
        }

        switch (header->type) {
        case SDR_RECORD_TYPE_FULL_SENSOR:
        case SDR_RECORD_TYPE_COMPACT_SENSOR:
            count++;
            ipmi_sensor_print_fc(mIntf, (struct sdr_record_common_sensor *)rec, header->type);
            break;
        }
        free(rec);
        rec = NULL;

        /* fix for CR6604909: */
        /* mask failure of individual reads in sensor list command */
        /* rc = (r == 0) ? rc : r; */
    }
    ipmi_sdr_end(itr);

    DEBUG("SDR has %ld sensors", count);
    return true;
}

bool ImIpmiHost::update_target_sdr(uint8_t target_addr) {
    struct sdr_get_rs *header;
    struct ipmi_sdr_iterator *itr;

    DEBUG(">>>");
    if (! mIntf) {
        ERROR("ipmi interface not connected!");
        return false;
    }

    DEBUG("querying SDR for sensor list for target 0x%x", target_addr);

    if (! bridge(target_addr)) {
        return false;
    }

    if (mTargets.find(target_addr) == mTargets.end()) {
        ImIpmiTarget target = {
            .mAddress = target_addr
        };
        DEBUG("adding target 0x%x", target_addr);
        mTargets[target_addr] = target;
    } else {
        DEBUG("target 0x%x exists", target_addr);
    }
    // ImIpmiTarget &target = mTargets[target_addr];
    std::map<std::string,ImIpmiSensor> &sensors  = mTargets[target_addr].mSensors;

    itr = ipmi_sdr_start(mIntf, 0);
    if (! itr) {
        ERROR("unable to open SDR for reading");
        bridge(0);
        return false;
    }

    size_t count = 0;
    while ((header = ipmi_sdr_get_next_header(mIntf, itr))) {
        uint8_t *rec;

        rec = ipmi_sdr_get_record(mIntf, header, itr);
        if (!rec) {
            ERROR("rec == NULL");
            continue;
        }

        switch (header->type) {
        case SDR_RECORD_TYPE_FULL_SENSOR:
        case SDR_RECORD_TYPE_COMPACT_SENSOR:
            count++;
            // ipmi_sensor_print_fc(mIntf, (struct sdr_record_common_sensor *)rec, header->type);
            {
                struct sdr_record_common_sensor *sensor = (struct sdr_record_common_sensor *)rec;
                if (IS_THRESHOLD_SENSOR(sensor)) {
            		// return ipmi_sensor_print_fc_threshold(intf, sensor, sdr_record_type);
                    int thresh_available = 1;
                    struct ipmi_rs *rsp;
                    struct sensor_reading *sr;

                    sr = ipmi_sdr_read_sensor_value(mIntf, sensor, header->type, 3);
                    if (! sr) {
                        ERROR("failed to read sensor value");
                        continue;
                    }

                    const char *thresh_status = ipmi_sdr_get_thresh_status(sr, "ns");
                    // get sensor thresholds
                    rsp = ipmi_sdr_get_sensor_thresholds(mIntf,
                            sensor->keys.sensor_num, sensor->keys.owner_id,
                            sensor->keys.lun, sensor->keys.channel);

                    if (! rsp || rsp->ccode || ! rsp->data_len) {
                        thresh_available = 0;
                    }
        			// dump_sensor_fc_thredshold(thresh_available, thresh_status, rsp, sr);
                    
                    if (sensors.find(sr->s_id) == sensors.end()) {
                        ImIpmiSensor s;
                        s.mName = std::string(sr->s_id);
                        s.mUnits = sr->s_a_units;
                        s.mValues.push_back(sr->s_a_val);
                        struct sdr_record_full_sensor *full = sr->full;
                        memset(s.mLowerThresholds, 0, sizeof(s.mLowerThresholds));
                        memset(s.mUpperThresholds, 0, sizeof(s.mUpperThresholds));
                        if (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED) {
                            s.mLowerThresholds[0] = sdr_convert_sensor_reading(full, rsp->data[3]);
                        }
                        if (rsp->data[0] & LOWER_CRIT_SPECIFIED) {
                            s.mLowerThresholds[1] = sdr_convert_sensor_reading(full, rsp->data[2]);
                        }
                        if (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED) {
                            s.mLowerThresholds[2] = sdr_convert_sensor_reading(full, rsp->data[1]);
                        }
                        if (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED) {
                            s.mUpperThresholds[0] = sdr_convert_sensor_reading(full, rsp->data[4]);
                        }
                        if (rsp->data[0] & UPPER_CRIT_SPECIFIED) {
                            s.mUpperThresholds[1] = sdr_convert_sensor_reading(full, rsp->data[5]);
                        }
                        if (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED) {
                            s.mUpperThresholds[2] = sdr_convert_sensor_reading(full, rsp->data[6]);
                        }
                	    DEBUG("adding sensor '%s'", s.mName.c_str());
                        sensors[sr->s_id] = s;
                    } else {
                	    DEBUG("sensor '%s' exists", sr->s_id);
                        ImIpmiSensor &s = sensors[sr->s_id];
                        s.mValues.push_back(sr->s_a_val);
                    }
                }
            }
            break;
        }
        free(rec);
        rec = NULL;

        /* fix for CR6604909: */
        /* mask failure of individual reads in sensor list command */
        /* rc = (r == 0) ? rc : r; */
    }
    ipmi_sdr_end(itr);

    DEBUG("SDR for target 0x%x has %ld sensors", target_addr, count);

    bridge(0);

    return true;
}

void ImIpmiHost::request_work(ImIpmiJob job) {
    mThread->request_work(job);
}

void ImIpmiHost::dump(uint8_t target_addr) {
    DEBUG(">>>");
    DEBUG("nr targets %ld", mTargets.size());

    for (auto it = std::begin(mTargets); it != mTargets.end(); ++it) {
        ImIpmiTarget &t = it->second;
        DEBUG("**** TARGET 0x%x (nr sensors %ld) *****", t.mAddress, t.mSensors.size());
        for (auto it2 = std::begin(t.mSensors); it2 != t.mSensors.end(); ++it2) {
            ImIpmiSensor &s = it2->second;
            DEBUG("  SENSOR (%ld readings) %-20s %4.2f %s (%4.2f %4.2f %4.2f | %4.2f %4.2f %4.2f)",
                s.mValues.size(), s.mName.c_str(), s.mValues.back(), s.mUnits.c_str(),
                s.mLowerThresholds[0], s.mLowerThresholds[1], s.mLowerThresholds[2],
                s.mUpperThresholds[0], s.mUpperThresholds[1], s.mUpperThresholds[2]
                );
        }
    }
}

ImIpmiThread::ImIpmiThread(ImIpmiHost *host)
    : mHost(host), mTerminate(false) {

    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mCond, nullptr);
}

ImIpmiThread::~ImIpmiThread() {
    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mCond);
}

void ImIpmiThread::start() {
    DEBUG(">>>");
    int ret = pthread_create(&mThreadId, nullptr, run, this);
    if (ret) {
        ERROR("failed to start thread for %s", mHost->mHostname.c_str());
    }
}

bool ImIpmiThread::terminate() {
    DEBUG(">>>");
    pthread_mutex_lock(&mMutex);
    mTerminate = true;
    pthread_cond_signal(&mCond);
    pthread_mutex_unlock(&mMutex);

    return join();
}

bool ImIpmiThread::join() {
    DEBUG(">>>");
    return pthread_join(mThreadId, nullptr) == 0;
}

void *ImIpmiThread::run(void *instance) {
    DEBUG(">>>");
    ImIpmiThread *us = static_cast<ImIpmiThread *>(instance);
    return us->do_run();
}

void ImIpmiThread::request_work(ImIpmiJob job) {
    DEBUG(">>>");
    pthread_mutex_lock(&mMutex);
    mJobs.push_back(job);
    pthread_cond_signal(&mCond);
    pthread_mutex_unlock(&mMutex);
}

void *ImIpmiThread::do_run() {
    DEBUG(">>>");
    while (true) {
        std::list<ImIpmiJob> jobs;
        pthread_mutex_lock(&mMutex);
        mJobs.swap(jobs);
        pthread_mutex_unlock(&mMutex);

        for (auto it = std::begin(jobs); it != std::end(jobs); ++it) {
            ImIpmiJob &job = *it;
            DEBUG("handling job type %d, target %d", job.mType, job.mTarget);
            if (job.mType == ImIpmiJobType_showAllSdr) {
                mHost->bridge(job.mTarget);
                mHost->list_sdr();
            } else if (job.mType == ImIpmiJobType_updateTargetSensors) {
                mHost->update_target_sdr(job.mTarget);
            }
        }
        jobs.clear();

        if (mTerminate) {
            pthread_exit(nullptr);
        }

        pthread_mutex_lock(&mMutex);
        DEBUG("going for a wait");
        int rc = ETIMEDOUT;
        while (rc == ETIMEDOUT) {
            mHost->keepalive();

            for (auto it = std::begin(mHost->mAllTargetAddresses); it != std::end(mHost->mAllTargetAddresses); ++it) {
                mHost->update_target_sdr(*it);
            }

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;
            rc = pthread_cond_timedwait(&mCond, &mMutex, &ts);
            DEBUG("woken rc %d", rc);
        }
        DEBUG("woken up");
        pthread_mutex_unlock(&mMutex);
    }
    
    return nullptr;
}