/**
 * @file mock_sas.cpp Mock SAS library.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "sas.h"

struct MockSASMessage
{
  bool marker;
  uint32_t id;
  std::vector<uint32_t> static_params;
  std::vector<std::string> var_params;
};

extern std::vector<MockSASMessage> mock_sas_messages;

void mock_sas_collect_messages(bool collect);
void mock_sas_discard_messages();

MockSASMessage* mock_sas_find_marker(uint32_t marker_id);
MockSASMessage* mock_sas_find_event(uint32_t event_id);

// Helper macros for checking for the presence / absence of SAS events and
// markers.
#define EXPECT_SAS_EVENT(ID) EXPECT_TRUE(mock_sas_find_event(ID) != NULL)
#define EXPECT_NO_SAS_EVENT(ID) EXPECT_TRUE(mock_sas_find_event(ID) == NULL)

#define EXPECT_SAS_MARKER(ID) EXPECT_TRUE(mock_sas_find_marker(ID) != NULL)
#define EXPECT_NO_SAS_MARKER(ID) EXPECT_TRUE(mock_sas_find_marker(ID) == NULL)
