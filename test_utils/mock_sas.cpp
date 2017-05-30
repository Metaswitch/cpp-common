/**
 * @file mock_sas.cpp Mock SAS library.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "mock_sas.h"

static bool _collect_messages = false;
std::vector<MockSASMessage> mock_sas_messages;

void mock_sas_collect_messages(bool collect)
{
  _collect_messages = collect;

  if (!_collect_messages)
  {
    mock_sas_messages.clear();
  }
}

void mock_sas_discard_messages()
{
  mock_sas_messages.clear();
}

void record_message(bool is_marker,
                    uint32_t id,
                    const std::vector<uint32_t>& static_params,
                    const std::vector<std::string>& var_params)
{
  if (_collect_messages)
  {
    MockSASMessage mock_msg;
    mock_msg.marker = is_marker;
    mock_msg.id = id;
    mock_msg.static_params = static_params;
    mock_msg.var_params = var_params;

    mock_sas_messages.push_back(mock_msg);
  }
}

MockSASMessage* mock_sas_find_marker(uint32_t marker_id)
{
  for(std::vector<MockSASMessage>::iterator msg = mock_sas_messages.begin();
      msg != mock_sas_messages.end();
      ++msg)
  {
    if (msg->marker && (msg->id == marker_id))
    {
      return &(*msg);
    }
  }
  return NULL;
}

MockSASMessage* mock_sas_find_event(uint32_t event_id)
{
  // The 3rd party API sets the top byte to 0x0F
  event_id = ((event_id & 0x00FFFFFF) | 0x0F000000);

  for(std::vector<MockSASMessage>::iterator msg = mock_sas_messages.begin();
      msg != mock_sas_messages.end();
      ++msg)
  {
    if (!msg->marker && (msg->id == event_id))
    {
      return &(*msg);
    }
  }
  return NULL;
}

int SAS::init(const std::string& system_name,
              const std::string& system_type,
              const std::string& resource_identifier,
              const std::string& sas_address,
              sas_log_callback_t* log_callback,
              create_socket_callback_t* socket_callback)
{
  return 0;
}

void SAS::term()
{
}

SAS::TrailId SAS::new_trail(uint32_t instance)
{
  return 0x123456789abcdef0;
}

class FakeCompressor : public SAS::Compressor
{
public:
  std::string compress(const std::string& s, const SAS::Profile* profile)
  {
    if (profile != NULL)
    {
      return "compress(\"" + s + "\", \"" + profile->get_dictionary() + "\")";
    }
    else
    {
      return "compress(\"" + s + "\")";
    }
  }
};

FakeCompressor _compressor;
SAS::Compressor* SAS::Compressor::get(Profile::Algorithm algorithm)
{
  return &_compressor;
}

void SAS::report_event(const SAS::Event& event)
{
  record_message(false, event._id, event._static_params, event._var_params);
}

void SAS::report_marker(const SAS::Marker& marker,
                        Marker::Scope scope,
                        bool reactivate)
{
  record_message(true, marker._id, marker._static_params, marker._var_params);
}

void SAS::associate_trails(SAS::TrailId a, SAS::TrailId b, Marker::Scope scope)
{
  // no-op in UT
}

SAS::Timestamp SAS::get_current_timestamp()
{
  // Return a fake timestamp.
  return 1400000000000;
}
