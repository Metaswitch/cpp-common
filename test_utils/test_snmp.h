/**
 * * Project Clearwater - IMS in the Cloud
 * * Copyright (C) 2016 Metaswitch Networks Ltd
 * *
 * * This program is free software: you can redistribute it and/or modify it
 * * under the terms of the GNU General Public License as published by the
 * * Free Software Foundation, either version 3 of the License, or (at your
 * * option) any later version, along with the "Special Exception" for use of
 * * the program along with SSL, set forth below. This program is distributed
 * * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * * A PARTICULAR PURPOSE. See the GNU General Public License for more
 * * details. You should have received a copy of the GNU General Public
 * * License along with this program. If not, see
 * * <http://www.gnu.org/licenses/>.
 * *
 * * The author can be reached by email at clearwater@metaswitch.com or by
 * * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 * *
 * * Special Exception
 * * Metaswitch Networks Ltd grants you permission to copy, modify,
 * * propagate, and distribute a work formed by combining OpenSSL with The
 * * Software, or a work derivative of such a combination, even if such
 * * copying, modification, propagation, or distribution would otherwise
 * * violate the terms of the GPL. You must comply with the GPL in all
 * * respects for all of the code used other than OpenSSL.
 * * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * * Project and licensed under the OpenSSL Licenses, or a work based on such
 * * software and licensed under the OpenSSL Licenses.
 * * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * * as those licenses appear in the file LICENSE-OPENSSL.
 * */


#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "utils.h"

#include "snmp_event_accumulator_table.h"
#include "snmp_continuous_accumulator_table.h"
#include "snmp_counter_table.h"
#include "snmp_success_fail_count_table.h"
#include "snmp_ip_count_table.h"
#include "snmp_scalar.h"
#include "test_interposer.hpp"
#include "snmp_single_count_by_node_type_table.h"
#include "snmp_success_fail_count_by_request_type_table.h"
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

  static void* snmp_thread(void*);
  unsigned int snmp_get(std::string);
  char* snmp_get_raw(std::string, char*, int);
  std::vector<std::string> snmp_walk(std::string);

  static pthread_t thr;
  std::string test_oid = ".1.2.2";

  static void SetUpTestCase();
  static void TearDownTestCase();
private:
  std::string alarm_address;

};

pthread_t SNMPTest::thr;

void* SNMPTest::snmp_thread(void* data)
{
  while (1)
  {
    agent_check_and_process(1);
  }
  return NULL;
}

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
  // Log SNMPd output to a file
  snmp_enable_filelog("fvtest-snmpd.out", 0);

  init_agent("fvtest");
  init_snmp("fvtest");
  init_master_agent();


  // Run a thread to handle SNMP requests
  pthread_create(&thr, NULL, snmp_thread, NULL);
}

void SNMPTest::TearDownTestCase()
{
  pthread_cancel(thr);
  pthread_join(thr, NULL);
  snmp_shutdown("fvtest");
}

