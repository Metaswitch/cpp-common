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
    TokenBucket(int max_size,
                float initial_rate_s,
                float minimum_rate_s,
                float maximum_rate_s);

    // Tests if there's at least one token in the bucket. If there is, decrement
    // the token count.
    // @returns      - Whether there was at least one token
    bool get_token();

    // Updates the token replenishment rate
    // @param new_rate - The new rate to use
    void update_rate(float new_rate_s);

    // Get functions for member variables (used for logging)
    inline float token_count() { return _tokens; }
    inline float rate() { return _rate_s; }
    inline int max_size() { return _max_size; }

  private:
    // Replenishes the tokens in the bucket.
    void replenish_bucket();

    // The number of tokens in the bucket (doesn't need to be a whole number).
    float _tokens;

    // The maximum number of tokens that can be in the bucket.
    int _max_size;

    // The rate at which tokens are refilled into the bucket (in tokens/second).
    float _rate_s;

    // The minimum possible value for the token refill rate (in tokens/second).
    float _min_rate_s;

    // The maximum possible value for the token refill rate (in tokens/second).
    // If this is 0, then no maximum rate is applied.
    float _max_rate_s;

    // When the bucket was last replenished (in microseconds since the epoch).
    timespec _replenish_time_us;
};

class LoadMonitor
{
  public:
    LoadMonitor(uint64_t target_latency_us,
                int max_bucket_size,
                float initial_rate_s,
                float minimum_rate_s,
                float maximum_rate_s,
                SNMP::AbstractContinuousAccumulatorTable* token_rate_tbl = NULL,
                SNMP::AbstractScalar* smoothed_latency_scalar = NULL,
                SNMP::AbstractScalar* target_latency_scalar = NULL,
                SNMP::AbstractScalar* penalties_scalar = NULL,
                SNMP::AbstractScalar* token_rate_scalar = NULL);
    virtual ~LoadMonitor();

    // Tests whether a request can be admitted.
    //
    // @param trail        - The SAS trail associated with this request
    // @param allow_anyway - Whether the request should be allowed even if
    //                       there aren't enough tokens
    // @returns            - Whether the request can be admitted.
    virtual bool admit_request(SAS::TrailId trail, bool allow_anyway = false);

    // This is called after a request that the load monitor is interested in
    // completes successfully. It adds the latency of the request to the
    // smoothed mean of all request latencies. If REQUESTS_BEFORE_ADJUSTMENT
    // requests have completed then it recalculates the refill rate.
    //
    // @param latency_us - How long the request took in microseconds
    // @param trail      - The SAS trail associated with this request
    virtual void request_complete(uint64_t latency_us, SAS::TrailId trail);

    // This is called after a request that the load monitor is interested in
    // completes, but another node involved in the request has returned an
    // overload response. We don't want to include this request's latency in
    // our average as it will be artifically low. Instead we increment a
    // penalty counter.
    virtual void incr_penalties();

    // Get functions for member variables (used for logging)
    virtual int get_target_latency_us() { return _target_latency_us; }
    int get_current_latency_us() { return _smoothed_latency_us; }
    float get_rate_limit() { return _bucket.rate(); }

  private:
    // Updates the load monitor statistics
    virtual void update_statistics();

    // The underlying Token Bucket.
    TokenBucket _bucket;

    // The smoothed mean of the request latencies (in microseconds)
    uint64_t _smoothed_latency_us;

    // The latency (in microseconds) we expect the average request to take.
    // If the average latency is lower than this then we should accept more
    // work; if it's higher then we should accept less work.
    uint64_t _target_latency_us;

    // The smoothed mean of the current rate we're completing requests (in
    // requests/second).
    float _smoothed_rate_s;

    // Number of accepted requests (reset when the rate is recalculated).
    int _accepted;

    // Number of rejected requests (reset when the rate is recalculated).
    int _rejected;

    // Number of requests where a different node has returned an overload
    // response (reset when the rate is recalculated).
    int _penalties;

    // Number of requests processed since the refill rate was last calculated
    // (reset when the rate is recalculated).
    int _adjust_count;

    // Statistics tables for the load monitor statistics
    SNMP::AbstractContinuousAccumulatorTable* _token_rate_table;
    SNMP::AbstractScalar* _smoothed_latency_scalar;
    SNMP::AbstractScalar* _target_latency_scalar;
    SNMP::AbstractScalar* _penalties_scalar;
    SNMP::AbstractScalar* _token_rate_scalar;

    // This must be held when accessing any of this object's member variables.
    pthread_mutex_t _lock;

    // Time in microseconds since the refill rate was last calculated (reset
    // when the rate is recalculated).
    uint64_t _last_adjustment_time_us;

    // Number of requests processed before each adjustment of token bucket rate
    const int REQUESTS_BEFORE_ADJUSTMENT = 20;

    // Percentage of rate we must be processing before we'd increase the rate
    const float PERCENTAGE_BEFORE_ADJUSTMENT = 0.5;

    // Adjustment parameters to decide when to recalculate the rate
    const float DECREASE_THRESHOLD = 0.0;
    const float INCREASE_THRESHOLD = -0.005;

    // Adjustment parameters to decide what rate change we should apply
    const float DECREASE_FACTOR = 1.2;
    const float INCREASE_FACTOR = 0.5;
};

#endif
