/**
 * @file cw_stat.cpp
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

// C++ re-implementation of Ruby cw_stat tool.
// Runs significantly faster - useful on heavily-loaded cacti systems.
// Usage: cw_stat <service> <statname>
// Compile: g++ -o cw_stat cw_stat.cpp -lzmq

#include <string>
#include <sstream>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <zmq.h>

// Gets a block of messages from the specified host, for the specified
// statistic.
// Return true on success, false on failure.
bool get_msgs(char* service, char* stat, std::vector<std::string>& msgs)
{
  // Create the context.
  void* ctx = zmq_ctx_new();
  if (ctx == NULL)
  {
    perror("zmq_ctx_new");
    return false;
  }

  // Create the socket.
  void* sck = zmq_socket(ctx, ZMQ_SUB);
  if (sck == NULL)
  {
    perror("zmq_socket");
    return false;
  }

  // Set a timeout of 10secs on the socket - this stops the calls from
  // blocking indefinitely.
  int timeout = 10000;
  if (zmq_setsockopt(sck, ZMQ_RCVTIMEO, &timeout, sizeof(timeout)) != 0)
  {
    perror("zmq_setsockopt");
    return false;
  }

  // Connect - note this has to be after we've set the timeout on the
  // socket.
  std::ostringstream oss;
  oss << "ipc:///var/run/clearwater/stats/" << service;
  if (zmq_connect(sck, oss.str().c_str()) != 0)
  {
    perror("zmq_connect");
    return false;
  }

  // Subscribe to the specified statistic.
  if (zmq_setsockopt(sck, ZMQ_SUBSCRIBE, stat, strlen(stat)) != 0)
  {
    perror("zmq_setsockopt");
    return false;
  }

  // Spin round until we've got all the messages in this block.
  int64_t more = 0;
  size_t more_sz = sizeof(more);
  do
  {
    zmq_msg_t msg;
    if (zmq_msg_init(&msg) != 0)
    {
      perror("zmq_msg_init");
      return false;
    }
    if (zmq_msg_recv(&msg, sck, 0) == -1)
    {
      if (errno == EAGAIN)
      {
        // This is an expected and visible way for this script to fail, so
        // provide a friendly error message.
        fprintf(stderr, "Error: No statistics retrieved within %d ms.\n", timeout);
      }
      else
      {
        perror("zmq_msg_recv");
      }

      return false;
    }
    msgs.push_back(std::string((char*)zmq_msg_data(&msg), zmq_msg_size(&msg)));
    if (zmq_getsockopt(sck, ZMQ_RCVMORE, &more, &more_sz) != 0)
    {
      perror("zmq_getsockopt");
      return false;
    }
    zmq_msg_close(&msg);
  }
  while (more);

  // Close the socket.
  if (zmq_close(sck) != 0)
  {
    perror("zmq_close");
    return false;
  }
  sck = NULL;

  // Destroy the context.
  if (zmq_ctx_destroy(ctx) != 0)
  {
    perror("zmq_ctx_destroy");
    return false;
  }
  ctx = NULL;

  return true;
}

// Render a simple statistic - just output its value.
void render_simple_stat(std::vector<std::string>& msgs)
{
  if (msgs.size() >= 3)
  {
    printf("%s\n", msgs[2].c_str());
  }
  else
  {
    printf("No value returned\n");
  }
}

// Render a list of IP addresses and counts.
void render_connected_ips(std::vector<std::string>& msgs)
{
  for (int msg_idx = 2; msg_idx < (int)msgs.size(); msg_idx += 2)
  {
    printf("%s: %s\n", msgs[msg_idx].c_str(), msgs[msg_idx + 1].c_str());
  }
}

// Render a set of call statistics.  The names here match those in Ruby
// cw_stat.
void render_call_stats(std::vector<std::string>& msgs)
{
  if (msgs.size() >= 10 )
  {
    printf("initial_registers:%s\n", msgs[2].c_str());
    printf("initial_registers_delta:%s\n", msgs[6].c_str());
    printf("ongoing_registers:%s\n", msgs[3].c_str());
    printf("ongoing_registers_delta:%s\n", msgs[7].c_str());
    printf("call_attempts:%s\n", msgs[4].c_str());
    printf("call_attempts_delta:%s\n", msgs[8].c_str());
    printf("successful_calls:%s\n", msgs[5].c_str());
    printf("successful_calls_delta:%s\n", msgs[9].c_str());
  }
  else
  {
    fprintf(stderr, "Too short call statistics - %d < 10", (int)msgs.size());
  }
}

// Render a set of latency statistics with a total count. The names here
// match those in Ruby cw_stat.
void render_count_latency_us(std::vector<std::string>& msgs)
{
  if (msgs.size() >= 7 )
  {
    printf("mean:%s\n", msgs[2].c_str());
    printf("variance:%s\n", msgs[3].c_str());
    printf("lwm:%s\n", msgs[4].c_str());
    printf("hwm:%s\n", msgs[5].c_str());
    printf("count:%s\n", msgs[6].c_str());
  }
  else
  {
    fprintf(stderr, "Too short call statistics - %d < 7", (int)msgs.size());
  }
}

// Render a set of global Astaire statistics - just 5 integers.
void render_astaire_global(std::vector<std::string>& msgs)
{
  if (msgs.size() >= 7 )
  {
    printf("bucketsNeedingResync:%s\n", msgs[2].c_str());
    printf("bucketsResynchronized:%s\n", msgs[3].c_str());
    printf("entriesResynchronized:%s\n", msgs[4].c_str());
    printf("dataResynchronized:%s\n", msgs[5].c_str());
    printf("bandwidth:%s\n", msgs[6].c_str());
  }
  else
  {
    fprintf(stderr, "Too short Astaire globals - %d < 7", (int)msgs.size());
  }
}

// Render a set of Astaire per-connection statistics.  This consists of a series of
// connections, each of which has an arbitrary number of associated buckets.
void render_astaire_connections(std::vector<std::string>& msgs)
{
  int connection = 0;
  for (int ii = 2; ii < msgs.size(); )
  {
    if (msgs.size() >= ii + 5)
    {
      printf("connection[%d]InetAddr:%s\n", connection, msgs[ii].c_str());
      printf("connection[%d]InetPort:%s\n", connection, msgs[ii + 1].c_str());
      printf("connection[%d]BucketNeedingResync:%s\n", connection, msgs[ii + 2].c_str());
      printf("connection[%d]BucketEntriesResynchronized:%s\n", connection, msgs[ii + 3].c_str());
      int num_buckets = atoi(msgs[ii + 4].c_str());
      ii += 5;
      int end_buckets = ii + num_buckets * 4;

      if (msgs.size() >= end_buckets)
      {
        int bucket = 0;
        for (; ii < end_buckets; ii += 4)
        {
          printf("connection[%d]Bucket[%d]Id:%s\n", connection, bucket, msgs[ii].c_str());
          printf("connection[%d]Bucket[%d]EntriesResynchronized:%s\n", connection, bucket, msgs[ii + 1].c_str());
          printf("connection[%d]Bucket[%d]DataResynchronized:%s\n", connection, bucket, msgs[ii + 2].c_str());
          printf("connection[%d]Bucket[%d]Bandwidth:%s\n", connection, bucket, msgs[ii + 3].c_str());
          bucket++;
        }
      }
      else
      {
        fprintf(stderr, "Too short Astaire bucket list - %d < %d", (int)msgs.size(), end_buckets);
        break;
      }
    }
    else
    {
      fprintf(stderr, "Too short Astaire connection - %d < %d", (int)msgs.size(), ii + 4);
      break;
    }
    connection++;
  }
}

int main(int argc, char** argv)
{
  // Check arguments.
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <service> <statname>\n", argv[0]);
    return 1;
  }

  // Get messages from the server.
  std::vector<std::string> msgs;
  if (!get_msgs(argv[1], argv[2], msgs))
  {
    return 2;
  }

  // The messages start with the statistic name and "OK" (hopefully).
  //
  // Note that a homestead provisioning node can in principle have multiple
  // homestead-prov processes which report statistics individually.  The stat
  // name contains the (0-based) index of the process.  This program only
  // listens for stats from the first process.
  if ((msgs.size() >= 2) &&
      (msgs[1] == "OK"))
  {
    // Determine which statistic we have and output it.
    if ((msgs[0] == "client_count")                   ||
        (msgs[0] == "incoming_requests")              ||
        (msgs[0] == "rejected_overload")              ||
        (msgs[0] == "H_incoming_requests")            ||
        (msgs[0] == "H_rejected_overload")            ||
        (msgs[0] == "P_incoming_requests_0")          ||
        (msgs[0] == "P_rejected_overload_0")          ||
        (msgs[0] == "chronos_scale_nodes_to_query")   ||
        (msgs[0] == "chronos_scale_timers_processed") ||
        (msgs[0] == "chronos_scale_invalid_timers_processed"))
    {
      render_simple_stat(msgs);
    }
    else if ((msgs[0] == "connected_homesteads") ||
             (msgs[0] == "connected_homers")     ||
             (msgs[0] == "connected_sprouts"))
    {
      render_connected_ips(msgs);
    }
    else if (msgs[0] == "call_stats")
    {
      render_call_stats(msgs);
    }
    else if ((msgs[0] == "latency_us")                    ||
             (msgs[0] == "hss_latency_us")                ||
             (msgs[0] == "hss_digest_latency_us")         ||
             (msgs[0] == "hss_subscription_latency_us")   ||
             (msgs[0] == "hss_user_auth_latency_us")      ||
             (msgs[0] == "hss_location_latency_us")       ||
             (msgs[0] == "xdm_latency_us")                ||
             (msgs[0] == "queue_size")                    ||
             (msgs[0] == "H_latency_us")                  ||
             (msgs[0] == "H_cache_latency_us")            ||
             (msgs[0] == "H_hss_latency_us")              ||
             (msgs[0] == "H_hss_digest_latency_us")       ||
             (msgs[0] == "H_hss_subscription_latency_us") ||
             (msgs[0] == "P_queue_size_0")                ||
             (msgs[0] == "P_latency_us_0"))
    {
      render_count_latency_us(msgs);
    }
    else if (msgs[0] == "astaire_global")
    {
      render_astaire_global(msgs);
    }
    else if (msgs[0] == "astaire_connections")
    {
      render_astaire_connections(msgs);
    }
    else
    {
      fprintf(stderr, "Unknown statistic \"%s\"\n", msgs[0].c_str());
    }
  }
  else if (msgs.size() == 1)
  {
    fprintf(stderr, "Incomplete response \"%s\"\n", msgs[0].c_str());
  }
  else
  {
    fprintf(stderr, "Error response \"%s\" for statistic \"%s\"\n", msgs[1].c_str(), msgs[0].c_str());
  }

  return 0;
}
