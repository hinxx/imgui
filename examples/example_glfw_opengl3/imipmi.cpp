#include <cassert>

#include "imipmi.h"

#define DEBUG(fmt, ...) printf("DEBUG: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...)  printf("INFO: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...)  printf("WARN: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define ERROR(fmt, ...) printf("ERROR: %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)


///////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////     CONTEXT    /////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

Context *InitContext() {
    Context *ctx = new Context();
    return ctx;
}

void DestroyContext(Context *ctx) {
    delete ctx;
}

Context::~Context() {
    DEBUG(">>>");
    for (auto it = std::begin(hosts); it != std::end(hosts); ++it) {
        delete it->second;
        it->second = nullptr;
    }
    hosts.clear();
}

Host *Context::AddHost(const char *hostname) {
    DEBUG(">>>");
    ShowHosts();
    if (hosts.count(hostname)) {
        ERROR("host object for hostname %s already exist", hostname);
        return nullptr;
    }
    Host *host = new Host(hostname, this);
    hosts[hostname] = host;
    return host;
}

void Context::RemoveHost(const char *hostname) {
    DEBUG(">>>");
    ShowHosts();
    auto it = hosts.find(hostname);
    if (it != hosts.end()) {
        DEBUG("removing %s from the context", hostname);
        delete it->second;
        hosts.erase(it);
    }
    ShowHosts();
    assert(hosts.count(hostname) == 0);
}

void Context::ShowHosts() {
    for (auto it = std::begin(hosts); it != std::end(hosts); ++it) {
        DEBUG("host %s, %p", it->first.c_str(), it->second);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////      HOST     //////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

Host::Host(const char *host, Context *ctx)
    : ctx(ctx),
    ipmiIntf(nullptr) {

    DEBUG(">>>");
    strncpy(hostname, host, 32);
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&cond, nullptr);

    Connect();
}

Host::~Host() {
    DEBUG(">>>");
    Disconnect();
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
}

bool Host::Connect() {
    INFO("connecting to %s", hostname);
    ipmi_intf *intf = ipmi_intf_load((char *)"lan");
    if (! intf) {
        ERROR("cannot load lan interface");
        return false;
    }

    ipmi_intf_session_set_hostname(intf, const_cast<char *>(hostname));
    // empty username and password
    ipmi_intf_session_set_username(intf, (char *)"");
    ipmi_intf_session_set_password(intf, (char *)"");
    ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);
	uint8_t kgkey[IPMI_KG_BUFFER_SIZE] = {0};
    ipmi_intf_session_set_kgkey(intf, kgkey);
    ipmi_intf_session_set_lookupbit(intf, 0x10);

    if (! intf->open || (intf->open(intf) == -1)) {
        ERROR("failed to connect to %s", hostname);
        ipmi_cleanup(intf);
        return false;
    }

    intf->my_addr = IPMI_BMC_SLAVE_ADDR;
    intf->target_addr = intf->my_addr;
    ipmiIntf = intf;

    INFO("connected to %s", hostname);

    if (! CheckIPMIVersion()) {
        Disconnect();
        return false;
    }

    terminate = false;
    int ret = pthread_create(&threadId, nullptr, Run, this);
    if (ret) {
        ERROR("failed to start thread for %s", hostname);
    }

    return true;
}

bool Host::TerminateThread() {
    DEBUG(">>>");
    if (threadId) {
        pthread_mutex_lock(&mutex);
        terminate = true;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        return JoinThread();
    }

    return false;
}

bool Host::JoinThread() {
    DEBUG(">>>");
    return pthread_join(threadId, nullptr) == 0;
}

void *Host::Run(void *instance) {
    DEBUG(">>>");
    Host *us = static_cast<Host *>(instance);
    return us->RunThread();
}

void *Host::RunThread() {
    DEBUG(">>>");
    while (true) {

        std::list<Job> local_jobs;
        pthread_mutex_lock(&mutex);
        local_jobs.swap(jobs);
        pthread_mutex_unlock(&mutex);

        for (auto it = std::begin(local_jobs); it != std::end(local_jobs); ++it) {
            Job &job = *it;
            DEBUG("handling job type %d", job.mType);
            if (job.mType == JobType_listSDR) {
                ReadSDR();
            }
        }
        local_jobs.clear();

        if (terminate) {
            pthread_exit(nullptr);
        }

        pthread_mutex_lock(&mutex);
        DEBUG("going for a wait");
        int rc = ETIMEDOUT;
        while (rc == ETIMEDOUT) {
            KeepAlive();

            ReadSDR();

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;
            rc = pthread_cond_timedwait(&cond, &mutex, &ts);
            DEBUG("woken rc %d", rc);
        }
        DEBUG("woken up");
        pthread_mutex_unlock(&mutex);
    }
    
    return nullptr;
}

void Host::RequestWork(Job job) {
    DEBUG(">>>");
    pthread_mutex_lock(&mutex);
    jobs.push_back(job);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

void Host::Disconnect() {
    DEBUG(">>>");
	if (! ipmiIntf) {
        ERROR("ipmi interface not connected!");
        return;
    }

    TerminateThread();

    ipmi_cleanup(ipmiIntf);
	if (ipmiIntf->opened && ipmiIntf->close) {
        ipmiIntf->close(ipmiIntf);
        INFO("disconnected from host %s", hostname);
    }
    ipmiIntf = nullptr;
}

void Host::KeepAlive() {
    DEBUG(">>>");
    if (! ipmiIntf) {
        ERROR("ipmi interface not connected!");
        return;
    }
    ipmiIntf->keepalive(ipmiIntf);
}

bool Host::CheckIPMIVersion() {
    // from ipmitool, ipmi_main.c
    ipmi_rq req;
    bool version_accepted = false;

    DEBUG(">>>");
    if (! ipmiIntf) {
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

    const ipmi_rs *const rsp = ipmiIntf->sendrecv(ipmiIntf, &req);
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

bool Host::ReadSDR() {
    struct sdr_get_rs *header;
    struct ipmi_sdr_iterator *itr;

    DEBUG(">>>");
    if (! ipmiIntf) {
        ERROR("ipmi interface not connected!");
        return false;
    }

    DEBUG("querying SDR for sensor list");

    itr = ipmi_sdr_start(ipmiIntf, 0);
    if (! itr) {
        ERROR("unable to open SDR for reading");
        return false;
    }

    size_t count = 0;
    while ((header = ipmi_sdr_get_next_header(ipmiIntf, itr))) {
        uint8_t *rec;

        rec = ipmi_sdr_get_record(ipmiIntf, header, itr);
        if (! rec) {
            ERROR("rec == NULL");
            continue;
        }

        unsigned uid = Sensor::MakeUID(header->type, rec);
        Sensor *s = GetSensor(uid);
        DEBUG("header type %d: sensor %p", header->type, s);

        if (header->type == SDR_RECORD_TYPE_FULL_SENSOR || header->type == SDR_RECORD_TYPE_COMPACT_SENSOR) {

            struct sdr_record_common_sensor *sen = (struct sdr_record_common_sensor *)rec;
            // ignore discrete sensors
            if (! IS_THRESHOLD_SENSOR(sen)) {
                DEBUG("header type %d not threshold sensor", header->type);
                continue;
            }

            DEBUG("sensor: %d, entity %x.%x", sen->keys.sensor_num, sen->entity.id, sen->entity.instance);
            struct sensor_reading *sr = ipmi_sdr_read_sensor_value(ipmiIntf, sen, header->type, 3);
            if (! sr) {
                ERROR("failed to read sensor value");
                continue;
            }

            if (sr->full) {
                struct sdr_record_full_sensor *full = sr->full;
                DEBUG(" full id string %s", full->id_string);

                if (! s) {
                    // new sensor found, obtain thresholds and units
                    int thresh_available = 1;
                    // get sensor thresholds
                    struct ipmi_rs *rsp = ipmi_sdr_get_sensor_thresholds(ipmiIntf,
                            sen->keys.sensor_num, sen->keys.owner_id,
                            sen->keys.lun, sen->keys.channel);

                    if (! rsp || rsp->ccode || ! rsp->data_len) {
                        thresh_available = 0;
                    }

                    double unr = 0.0;
                    double ucr = 0.0;
                    double unc = 0.0;
                    double lnc = 0.0;
                    double lcr = 0.0;
                    double lnr = 0.0;
                    if (thresh_available) {
                        if (rsp->data[0] & LOWER_NON_RECOV_SPECIFIED) {
                            lnr = sdr_convert_sensor_reading(full, rsp->data[3]);
                        }
                        if (rsp->data[0] & LOWER_CRIT_SPECIFIED) {
                            lcr = sdr_convert_sensor_reading(full, rsp->data[2]);
                        }
                        if (rsp->data[0] & LOWER_NON_CRIT_SPECIFIED) {
                            lnc = sdr_convert_sensor_reading(full, rsp->data[1]);
                        }
                        if (rsp->data[0] & UPPER_NON_CRIT_SPECIFIED) {
                            unc = sdr_convert_sensor_reading(full, rsp->data[4]);
                        }
                        if (rsp->data[0] & UPPER_CRIT_SPECIFIED) {
                            ucr = sdr_convert_sensor_reading(full, rsp->data[5]);
                        }
                        if (rsp->data[0] & UPPER_NON_RECOV_SPECIFIED) {
                            unr = sdr_convert_sensor_reading(full, rsp->data[6]);
                        }
                    }

                    s = CreateSensor(header->type, sen->sensor.type, (const char *)full->id_string, sen->entity.id, sen->entity.instance);
                    sensors[uid] = s;
                    s->SetUnits(sr->s_a_units);
                    s->SetThresholds(thresh_available, unr, ucr, unc, lnc, lcr, lnr);
                }
            } else if (sr->compact) {
                struct sdr_record_compact_sensor *compact = sr->compact;
                DEBUG(" compact id string %s", compact->id_string);
                if (! s) {
                    s = CreateSensor(header->type, sen->sensor.type, (const char *)compact->id_string, sen->entity.id, sen->entity.instance);
                    sensors[uid] = s;
                    s->SetUnits(sr->s_a_units);
                    s->SetThresholds(0, 0, 0, 0, 0, 0, 0);
                }
            }

            assert(s != nullptr);

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t timestamp = ts.tv_sec * 1000000000 + ts.tv_nsec;
            // TODO: decode threshold status!?
            // const char *thresh_status = ipmi_sdr_get_thresh_status(sr, "ns");
            s->AddReading(sr->s_a_val, timestamp, sr->s_data2);

        } else if (header->type == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR) {
            struct sdr_record_fru_locator *fru = (struct sdr_record_fru_locator *)rec;
            DEBUG("fruloc sensor:     Entity %x.%x", fru->entity.id, fru->entity.instance);
            DEBUG(" id string %s", fru->id_string);
            // GetSensor(header->type, 0, (const char *)fru->id_string, fru->entity.id, fru->entity.instance);
            // Sensor *s = GetSensor(uid);
            // DEBUG("sensor %p", s);

        } else {
            struct sdr_record_common_sensor *sen = (struct sdr_record_common_sensor *)rec;
            ERROR("?????? sensor: %d, Entity %x.%x", sen->keys.sensor_num, sen->entity.id, sen->entity.instance);
        }

        count++;

        free(rec);
        rec = NULL;
    }
    ipmi_sdr_end(itr);

    DEBUG("SDR has %ld sensors", count);
    return true;
}

unsigned Sensor::MakeUID(const int type, const uint8_t *rec) {
    unsigned uid = 0;
    if (type == SDR_RECORD_TYPE_FULL_SENSOR || type == SDR_RECORD_TYPE_COMPACT_SENSOR) {
        struct sdr_record_common_sensor *common = (struct sdr_record_common_sensor *)rec;
        uid = common->keys.sensor_num << 24 \
            | common->keys.lun << 16 \
            | common->entity.id << 8 \
            | common->entity.instance;
    } else if (type == SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR) {
        struct sdr_record_fru_locator *fru = (struct sdr_record_fru_locator *)rec;
        uid = 0 \
            | fru->lun << 16 \
            | fru->entity.id << 8 \
            | fru->entity.instance;
    }
    return uid;
}

Sensor *Host::CreateSensor(const int record_type, const int sensor_type, const char *name, const int entity_id, const int entity_instance) {
    Sensor *s = new Sensor(hostname, record_type, sensor_type, name, entity_id, entity_instance);
    DEBUG("created new sensor %s, %p", name, s);
    return s;
}

Sensor *Host::GetSensor(const unsigned uid) {
    auto found = sensors.find(uid);
    if (found == sensors.end()) {
        DEBUG("NOT found sensor UID %x", uid);
        return nullptr;
    }
    DEBUG("found sensor UID %x, name %s, %p", uid, found->second->info.name, found->second);
    return found->second;
}

void Host::Dump() {
    DEBUG(">>>");
    DEBUG("nr sensors %ld", sensors.size());

    for (auto it = sensors.begin(); it != sensors.end(); ++it) {
        Sensor *s = it->second;
        DEBUG("SENSOR: %s %x.%x   %f %s @ %ld %x",
            s->info.name, s->info.entityId, s->info.entityInstance, s->value.value, s->info.units, s->value.ts, s->value.status);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////    SENSOR     //////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////


Sensor::Sensor(const char *host, const int record_type, const int sensor_type, const char *name, const int entity_id, const int entity_instance) {
    // strncpy(info.host, host, 20);
    info.host = host;
    info.recordType = record_type;
    info.sensorType = sensor_type;
    info.entityId = entity_id;
    info.entityInstance = entity_instance;
    strncpy(info.name, name, 20);
    DEBUG("%s %s: new sensor entity %x.%x", info.host, info.name, info.entityId, info.entityInstance);
}

void Sensor::SetUnits(const char *units) {
    DEBUG("%s: units %s", info.name, units);
    strncpy(info.units, units, 20);
}

// unr = upper non-recoverable
// ucr = upper critical
// unc = upper non-critical
// lnc = lower non-critical
// lcr = lower critical
// lnr = lower non-recoverable
void Sensor::SetThresholds(const bool available, const double unr, const double ucr, const double unc, const double lnc, const double lcr, const double lnr) {
    DEBUG("%s %s: available %d unr %4.2f ucr %4.2f unc %4.2f lnc %4.2f lcr %4.2f lnr %4.2f", info.host, info.name, available, unr, ucr, unc, lnc, lcr, lnr);
    info.threshAvailable = available;
    info.unr = unr;
    info.ucr = ucr;
    info.unc = unc;
    info.lnc = lnc;
    info.lcr = lcr;
    info.lnc = lnc;
}

void Sensor::AddReading(const double val, const uint64_t ts, const unsigned status) {
    SensorReading r = {
        .value = val,
        .ts = ts,
        .status = status
    };
    value = r;
}
