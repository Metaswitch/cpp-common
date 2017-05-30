/**
 * @file load_monitor.h Definitions for LoadMonitor class.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef LOAD_MONITOR_H__
#define LOAD_MONITOR_H__

#include <time.h>
#include <pthread.h>
#include "snmp_continuous_accumulator_table.h"
#include "snmp_abstract_scalar.h"
#include "sas.h"

class TokenBucket
{
  public:
    TokenBucket(int s, float r);
    float rate;
    int max_size;
    bool get_token();
    void update_rate(float new_rate);
    inline float token_count() { return tokens; }
  private:
    timespec replenish_time;
    float tokens;
    void replenish_bucket();
};

class LoadMonitor
{
  public:
    LoadMonitor(int init_target_latency, int max_bucket_size,
                float init_token_rate, float init_min_token_rate,
                SNMP::AbstractContinuousAccumulatorTable* token_rate_tbl = NULL,
                SNMP::AbstractScalar* smoothed_latency_scalar = NULL,
                SNMP::AbstractScalar* target_latency_scalar = NULL,
                SNMP::AbstractScalar* penalties_scalar = NULL,
                SNMP::AbstractScalar* token_rate_scalar = NULL);
    virtual ~LoadMonitor();
    virtual bool admit_request(SAS::TrailId trail);
    virtual void incr_penalties();
    virtual int get_target_latency_us();
    virtual void request_complete(int latency);
    virtual void update_statistics();

    int get_target_latency() { return target_latency; }
    int get_current_latency() { return smoothed_latency; }
    float get_rate_limit() { return bucket.rate; }

  private:
    // This must be held when accessing any of this object's member variables.
     pthread_mutex_t _lock;

     // Number of requests processed before each adjustment of token bucket rate
     int REQUESTS_BEFORE_ADJUSTMENT;
     // Number of seconds that must pass before each adjustment of token bucket rate
     int SECONDS_BEFORE_ADJUSTMENT;

    // Adjustment parameters for token bucket
    float DECREASE_THRESHOLD;
    float DECREASE_FACTOR;
    float INCREASE_THRESHOLD;
    float INCREASE_FACTOR;

    int accepted;
    int rejected;
    int penalties;
    int pending_count;
    int max_pending_count;
    int target_latency;
    int smoothed_latency;
    int adjust_count;
    unsigned long last_adjustment_time_ms;
    float min_token_rate;
    TokenBucket bucket;
    SNMP::AbstractContinuousAccumulatorTable* _token_rate_table;
    SNMP::AbstractScalar* _smoothed_latency_scalar;
    SNMP::AbstractScalar* _target_latency_scalar;
    SNMP::AbstractScalar* _penalties_scalar;
    SNMP::AbstractScalar* _token_rate_scalar;
};

#endif
