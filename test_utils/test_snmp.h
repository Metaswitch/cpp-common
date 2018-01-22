/**
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */


#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "utils.h"

#include "snmp_agent.h"
#include "snmp_event_accumulator_table.h"
#include "snmp_event_accumulator_by_scope_table.h"
#include "snmp_continuous_accumulator_table.h"
#include "snmp_counter_table.h"
#include "snmp_success_fail_count_table.h"
#include "snmp_ip_count_table.h"
#include "snmp_scalar.h"
#include "test_interposer.hpp"
#include "snmp_single_count_by_node_type_table.h"
#include "snmp_success_fail_count_by_request_type_table.h"
#include "snmp_time_and_string_based_event_table.h"
#include "snmp_cx_counter_table.h"
#include "snmp_ip_time_based_counter_table.h"

#include "snmp_internal/snmp_includes.h"
using ::testing::AnyOf;
using ::testing::Contains;

class SNMPTest : public ::testing::Test
{
public:
  SNMPTest() : alarm_address("16161") { }
  SNMPTest(std::string address) : alarm_address(address) { }

  unsigned int snmp_get(std::string);
  char* snmp_get_raw(std::string, char*, int);
  std::vector<std::string> snmp_walk(std::string);

  static pthread_t thr;
  std::string test_oid = ".1.2.2";

  static void SetUpTestCase();
  static void TearDownTestCase();

  std::string time_string_event_oid(std::string base, int stat, int time, std::string string_index);
  void snmp_walk_debug(std::string base);

private:
  std::string alarm_address;

};

pthread_t SNMPTest::thr;

unsigned int SNMPTest::snmp_get(std::string oid)
{
  char buf[1024];
  return atol(snmp_get_raw(oid, buf, sizeof(buf)));
}

char* SNMPTest::snmp_get_raw(std::string oid, char* buf, int size)
{
  // Returns integer value found at that OID.
  std::string command = "snmpget -v2c -Ovqn -c clearwater 127.0.0.1:" + alarm_address + " " + oid;
  std::string mode = "r";
  FILE* fd = popen(command.c_str(), mode.c_str());
  fgets(buf, size, fd);
  return buf;
}

std::vector<std::string> SNMPTest::snmp_walk(std::string oid)
{
  // Returns the results of an snmpwalk performed at that oid as a list of
  // strings.
  std::vector<std::string> res;
  std::string entry;

  std::string command = "snmpwalk -v2c -OQn -c clearwater 127.0.0.1:" + alarm_address + " " + oid;
  std::string mode = "r";
  FILE* fd = popen(command.c_str(), mode.c_str());
  char buf[1024];
  char* fgets_pointer;
  // fgets returns a null pointer when it reaches EOF.
  fgets_pointer = fgets(buf, sizeof(buf), fd);
  char empty[] = "No Such Object available on this agent at this OID";
  // Checks that the table is not empty.
  if (fgets_pointer && !strstr(buf,empty))
  {
  entry = buf;
  std::size_t end = entry.find("No more variables left in this MIB View");
  while (end == std::string::npos && fgets_pointer)
  {
    res.push_back(Utils::rtrim(entry));
    fgets_pointer = fgets(buf, sizeof(buf), fd);
    entry = buf;
    end = entry.find("No more variables left in this MIB View");
  }
  }
  return res;
}

// Sets up an SNMP master agent on port 16161 for us to register tables with and query
void SNMPTest::SetUpTestCase()
{
  // Configure SNMPd to use the fvtest.conf in the local directory
  char cwd[256];
  getcwd(cwd, sizeof(cwd));
  netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                        NETSNMP_DS_LIB_CONFIGURATION_DIR,
                        cwd);

  snmp_setup("fvtest");

  // Override SNMPd logging to send output to a file
  snmp_disable_calllog();
  snmp_enable_filelog("fvtest-snmpd.out", 0);

  // Start as a master agent, not a subagent.
  netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 0);
  init_snmp("fvtest");
  init_master_agent();

  init_snmp_handler_threads("fvtest");
}

void SNMPTest::TearDownTestCase()
{
  snmp_terminate("fvtest");

  // Not 100% thread-safe, but we need to do it to avoid memory leaks.
  SNMP::Agent::deinstantiate();
}

std::string SNMPTest::time_string_event_oid(std::string base, int stat, int time, std::string string_index)
{
  std::string oid = base + ".1." + std::to_string(stat+2) + "." + std::to_string(time) + "." + string_index;
  return oid;
}

void SNMPTest::snmp_walk_debug(std::string base)
{
  std::vector<std::string> entries = snmp_walk(base);
  for (auto entry: entries)
  {
    printf("%s\n", entry.c_str());
  }
}

