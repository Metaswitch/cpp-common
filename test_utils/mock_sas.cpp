/**
 * @file mock_sas.cpp Mock SAS library.
 *
 * project clearwater - ims in the cloud
 * copyright (c) 2013  metaswitch networks ltd
 *
 * this program is free software: you can redistribute it and/or modify it
 * under the terms of the gnu general public license as published by the
 * free software foundation, either version 3 of the license, or (at your
 * option) any later version, along with the "special exception" for use of
 * the program along with ssl, set forth below. this program is distributed
 * in the hope that it will be useful, but without any warranty;
 * without even the implied warranty of merchantability or fitness for
 * a particular purpose.  see the gnu general public license for more
 * details. you should have received a copy of the gnu general public
 * license along with this program.  if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * the author can be reached by email at clearwater@metaswitch.com or by
 * post at metaswitch networks ltd, 100 church st, enfield en2 6bq, uk
 *
 * special exception
 * metaswitch networks ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining openssl with the
 * software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the gpl. you must comply with the gpl in all
 * respects for all of the code used other than openssl.
 * "openssl" means openssl toolkit software distributed by the openssl
 * project and licensed under the openssl licenses, or a work based on such
 * software and licensed under the openssl licenses.
 * "openssl licenses" means the openssl license and original ssleay license
 * under which the openssl project distributes the openssl toolkit software,
 * as those licenses appear in the file license-openssl.
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
              sas_log_callback_t* log_callback)
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

SAS::Compressor::Compressor() {}
SAS::Compressor::~Compressor() {}

std::string SAS::Compressor::compress(const std::string& s, const Profile* profile)
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

SAS::Compressor _compressor;
SAS::Compressor* SAS::Compressor::get()
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

SAS::Timestamp SAS::get_current_timestamp()
{
  // Return a fake timestamp.
  return 1400000000000;
}
