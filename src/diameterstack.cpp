/**
 * @file diameterstack.cpp class implementation wrapping freeDiameter
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "diameterstack.h"
#include "log.h"
#include "sasevent.h"

using namespace Diameter;

Stack* Stack::INSTANCE = &DEFAULT_INSTANCE;
Stack Stack::DEFAULT_INSTANCE;

Stack::Stack() : _initialized(false), _callback_handler(NULL), _callback_fallback_handler(NULL)
{
}

Stack::~Stack()
{
}

void Stack::initialize()
{
  // Initialize if we haven't already done so.  We don't do this in the
  // constructor because we can't throw exceptions on failure.
  if (!_initialized)
  {
    LOG_STATUS("Initializing Diameter stack");
    int rc = fd_core_initialize();
    if (rc != 0)
    {
      throw Exception("fd_core_initialize", rc); // LCOV_EXCL_LINE
    }
    rc = fd_log_handler_register(Stack::logger);
    if (rc != 0)
    {
      throw Exception("fd_log_handler_register", rc); // LCOV_EXCL_LINE
    }
    _initialized = true;
  }
}

void Stack::configure(std::string filename)
{
  initialize();
  LOG_STATUS("Configuring Diameter stack from file %s", filename.c_str());
  int rc = fd_core_parseconf(filename.c_str());
  if (rc != 0)
  {
    throw Exception("fd_core_parseconf", rc); // LCOV_EXCL_LINE
  }
}

void Stack::advertize_application(const Dictionary::Application& app)
{
  initialize();
  int rc = fd_disp_app_support(app.dict(), NULL, 1, 0);
  if (rc != 0)
  {
    throw Exception("fd_disp_app_support", rc); // LCOV_EXCL_LINE
  }
}

void Stack::advertize_application(const Dictionary::Vendor& vendor, const Dictionary::Application& app)
{
  initialize();
  int rc = fd_disp_app_support(app.dict(), vendor.dict(), 1, 0);
  if (rc != 0)
  {
    throw Exception("fd_disp_app_support", rc); // LCOV_EXCL_LINE
  }
}

void Stack::register_handler(const Dictionary::Application& app, const Dictionary::Message& msg, BaseHandlerFactory* factory)
{
  // Register a callback for messages from our application with the specified message type.
  // DISP_HOW_CC indicates that we want to match on command code (and allows us to optionally
  // match on application if specified). Use a pointer to our HandlerFactory to pass through
  // to our callback function.
  struct disp_when data;
  memset(&data, 0, sizeof(data));
  data.app = app.dict();
  data.command = msg.dict();
  int rc = fd_disp_register(handler_callback_fn, DISP_HOW_CC, &data, (void *)factory, &_callback_handler);

  if (rc != 0)
  {
    throw Exception("fd_disp_register", rc); //LCOV_EXCL_LINE
  }
}

void Stack::register_fallback_handler(const Dictionary::Application &app)
{
  // Register a fallback callback for messages of an unexpected type to our application
  // so that we can log receiving an unexpected message.
  struct disp_when data;
  memset(&data, 0, sizeof(data));
  data.app = app.dict();
  int rc = fd_disp_register(fallback_handler_callback_fn, DISP_HOW_APPID, &data, NULL, &_callback_fallback_handler);

  if (rc != 0)
  {
    throw Exception("fd_disp_register", rc); //LCOV_EXCL_LINE
  }
}

int Stack::handler_callback_fn(struct msg** req, struct avp* avp, struct session* sess, void* handler_factory, enum disp_action* act)
{
  Stack* stack = Stack::get_instance();
  Dictionary* dict = ((Diameter::Stack::BaseHandlerFactory*)handler_factory)->_dict;

  SAS::TrailId trail = SAS::new_trail(0);

  // Create a new message object so and raise the necessary SAS logs.
  Message msg(dict, *req, stack);
  msg.sas_log_rx(trail, 0);
  msg.revoke_ownership();

  // Create and run the correct handler based on the received message and the dictionary
  // object we've passed through.
  Handler* handler = ((Diameter::Stack::BaseHandlerFactory*)handler_factory)->create(dict, req);
  handler->set_trail(trail);

  handler->run();

  // The handler will turn the message associated with the handler into an answer which we wish to send to the HSS.
  // Setting the action to DISP_ACT_SEND ensures that we will send this answer on without going to any other callbacks.
  // Return 0 to indicate no errors with the callback.
  *req = NULL;
  *act = DISP_ACT_CONT;
  return 0;
}

int Stack::fallback_handler_callback_fn(struct msg** msg, struct avp* avp, struct session* sess, void* opaque, enum disp_action* act)
{
  // This means we have received a message of an unexpected type.
  LOG_WARNING("Message of unexpected type received");
  return ENOTSUP;
}

void Stack::start()
{
  initialize();
  LOG_STATUS("Starting Diameter stack");
  int rc = fd_core_start();
  if (rc != 0)
  {
    throw Exception("fd_core_start", rc); // LCOV_EXCL_LINE
  }
}

void Stack::stop()
{
  if (_initialized)
  {
    LOG_STATUS("Stopping Diameter stack");
    if (_callback_handler)
    {
      (void)fd_disp_unregister(&_callback_handler, NULL);
    }

    if (_callback_fallback_handler)
    {
      (void)fd_disp_unregister(&_callback_fallback_handler, NULL);
    }

    int rc = fd_core_shutdown();
    if (rc != 0)
    {
      throw Exception("fd_core_shutdown", rc); // LCOV_EXCL_LINE
    }
  }
}

void Stack::wait_stopped()
{
  if (_initialized)
  {
    LOG_STATUS("Waiting for Diameter stack to stop");
    int rc = fd_core_wait_shutdown_complete();
    if (rc != 0)
    {
      throw Exception("fd_core_wait_shutdown_complete", rc); // LCOV_EXCL_LINE
    }
    fd_log_handler_unregister();
    _initialized = false;
  }
}

void Stack::logger(int fd_log_level, const char* fmt, va_list args)
{
  // freeDiameter log levels run from 1 (debug) to 6 (fatal).  (It also defines 0 for "annoying"
  // logs that are only compiled into debug builds, which we don't use.)  See libfdproto.h for
  // details.
  //
  // Our logger uses levels 0 (error) to 5 (debug).
  //
  // Map between the two.
  int log_level;
  switch (fd_log_level)
  {
    case FD_LOG_FATAL:
    case FD_LOG_ERROR:
      log_level = Log::ERROR_LEVEL;
      break;

    case FD_LOG_NOTICE:
      log_level = Log::STATUS_LEVEL;
      break;

    case FD_LOG_DEBUG:
    case FD_LOG_ANNOYING:
    default:
      log_level = Log::DEBUG_LEVEL;
      break;
  }
  Log::_write(log_level, "freeDiameter", 0, fmt, args);
}

void Stack::send(struct msg* fd_msg)
{
  fd_msg_send(&fd_msg, NULL, NULL);
}

void Stack::send(struct msg* fd_msg, Transaction* tsx)
{
  fd_msg_send(&fd_msg, Transaction::on_response, tsx);
}

void Stack::send(struct msg* fd_msg, Transaction* tsx, unsigned int timeout_ms)
{
  struct timespec timeout_ts;
  // TODO: Check whether this should be CLOCK_MONOTONIC - freeDiameter uses CLOCK_REALTIME but
  //       this feels like it might suffer over time changes.
  clock_gettime(CLOCK_REALTIME, &timeout_ts);
  timeout_ts.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;
  timeout_ts.tv_sec += timeout_ms / 1000 + timeout_ts.tv_nsec / (1000 * 1000 * 1000);
  timeout_ts.tv_nsec = timeout_ts.tv_nsec % (1000 * 1000 * 1000);
  fd_msg_send_timeout(&fd_msg, Transaction::on_response, tsx, Transaction::on_timeout, &timeout_ts);
}

struct dict_object* Dictionary::Vendor::find(const std::string vendor)
{
  struct dict_object* dict;
  fd_dict_search(fd_g_config->cnf_dict, DICT_VENDOR, VENDOR_BY_NAME, vendor.c_str(), &dict, ENOENT);
  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(vendor.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

struct dict_object* Dictionary::Application::find(const std::string application)
{
  struct dict_object* dict;
  fd_dict_search(fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, application.c_str(), &dict, ENOENT);
  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(application.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

struct dict_object* Dictionary::Message::find(const std::string message)
{
  struct dict_object* dict;
  fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, message.c_str(), &dict, ENOENT);
  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(message.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

struct dict_object* Dictionary::AVP::find(const std::string avp)
{
  struct dict_object* dict;
  fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, avp.c_str(), &dict, ENOENT);
  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(avp.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

struct dict_object* Dictionary::AVP::find(const std::string vendor, const std::string avp)
{
  struct dict_avp_request avp_req;
  if (!vendor.empty())
  {
    struct dict_object* vendor_dict = Dictionary::Vendor::find(vendor);
    struct dict_vendor_data vendor_data;
    fd_dict_getval(vendor_dict, &vendor_data);
    avp_req.avp_vendor = vendor_data.vendor_id;
  }
  else
  {
    avp_req.avp_vendor = 0;
  }
  avp_req.avp_name = (char*)avp.c_str();
  struct dict_object* dict;
  fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR, &avp_req, &dict, ENOENT);
  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(avp.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

struct dict_object* Dictionary::AVP::find(const std::vector<std::string>& vendors, const std::string avp)
{
  struct dict_object* dict = NULL;
  for (std::vector<std::string>::const_iterator vendor = vendors.begin();
       vendor != vendors.end();
       ++vendor)
  {
    struct dict_avp_request avp_req;

    if (!vendor->empty())
    {
      struct dict_object* vendor_dict = Dictionary::Vendor::find(*vendor);
      struct dict_vendor_data vendor_data;
      fd_dict_getval(vendor_dict, &vendor_data);
      avp_req.avp_vendor = vendor_data.vendor_id;
    }
    else
    {
      avp_req.avp_vendor = 0;
    }
    avp_req.avp_name = (char*)avp.c_str();
    fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_AND_VENDOR, &avp_req, &dict, ENOENT);
    if (dict != NULL)
    {
      break;
    }
  }

  if (dict == NULL)
  {
    throw Diameter::Stack::Exception(avp.c_str(), 0); // LCOV_EXCL_LINE
  }
  return dict;
}

Dictionary::Dictionary() :
  SESSION_ID("Session-Id"),
  VENDOR_SPECIFIC_APPLICATION_ID("Vendor-Specific-Application-Id"),
  VENDOR_ID("Vendor-Id"),
  AUTH_SESSION_STATE("Auth-Session-State"),
  ORIGIN_REALM("Origin-Realm"),
  ORIGIN_HOST("Origin-Host"),
  DESTINATION_REALM("Destination-Realm"),
  DESTINATION_HOST("Destination-Host"),
  USER_NAME("User-Name"),
  RESULT_CODE("Result-Code"),
  DIGEST_HA1("Digest-HA1"),
  DIGEST_REALM("Digest-Realm"),
  DIGEST_QOP("Digest-QoP"),
  EXPERIMENTAL_RESULT("Experimental-Result"),
  EXPERIMENTAL_RESULT_CODE("Experimental-Result-Code"),
  ACCT_INTERIM_INTERVAL("Acct-Interim-Interval")
{
}

Transaction::Transaction(Dictionary* dict, SAS::TrailId trail) :
  _dict(dict), _trail(trail)
{
}

Transaction::~Transaction()
{
}

void Transaction::on_response(void* data, struct msg** rsp)
{
  Transaction* tsx = (Transaction*)data;
  Stack* stack = Stack::get_instance();
  Message msg(tsx->_dict, *rsp, stack);

  LOG_VERBOSE("Got Diameter response of type %u - calling callback on transaction %p",
              msg.command_code(), tsx);
  msg.sas_log_rx(tsx->trail(), 0);

  tsx->stop_timer();
  tsx->on_response(msg);
  delete tsx;
  // Null out the message so that freeDiameter doesn't try to send it on.
  *rsp = NULL;
}

void Transaction::on_timeout(void* data, DiamId_t to, size_t to_len, struct msg** req)
{
  Transaction* tsx = (Transaction*)data;
  Stack* stack = Stack::get_instance();
  Message msg(tsx->_dict, *req, stack);

  LOG_VERBOSE("Diameter request of type %u timed out - calling callback on transaction %p",
              msg.command_code(), tsx);
  msg.sas_log_timeout(tsx->trail(), 0);

  tsx->stop_timer();
  tsx->on_timeout();
  delete tsx;
  // Null out the message so that freeDiameter doesn't try to send it on.
  *req = NULL;
}

AVP& AVP::val_json(const std::vector<std::string>& vendors,
                   const Diameter::Dictionary::AVP& dict,
                   const rapidjson::Value& value)
{
  switch (value.GetType())
  {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      LOG_ERROR("Invalid format (true/false) in JSON block (%d), ignoring",
                avp_hdr()->avp_code);
      break;
    case rapidjson::kNullType:
      LOG_ERROR("Invalid NULL in JSON block, ignoring");
      break;
    case rapidjson::kArrayType:
      LOG_ERROR("Cannot store multiple values in one ACR, ignoring");
      break;
    case rapidjson::kStringType:
      val_str(value.GetString());
      break;
    case rapidjson::kNumberType:
      // Parse the value out of the JSON as the appropriate type
      // for for AVP.
      switch (dict.base_type())
      {
      case AVP_TYPE_GROUPED:
        LOG_ERROR("Cannot store integer in grouped AVP, ignoring");
        break;
      case AVP_TYPE_OCTETSTRING:
        // The only time this occurs is for types that have custom
        // encoders (e.g. TIME types).  In those cases, Uint64 is
        // the correct format.
        val_u64(value.GetUint64());
        break;
      case AVP_TYPE_INTEGER32:
        val_i32(value.GetInt());
        break;
      case AVP_TYPE_INTEGER64:
        val_i64(value.GetInt64());
        break;
      case AVP_TYPE_UNSIGNED32:
        val_u32(value.GetUint());
        break;
      case AVP_TYPE_UNSIGNED64:
        val_u64(value.GetUint64());
        break;
      case AVP_TYPE_FLOAT32:
      case AVP_TYPE_FLOAT64:
        LOG_ERROR("Floating point AVPs are not supportedi, ignoring");
        break;
      default:
        LOG_ERROR("Unexpected AVP type, ignoring"); // LCOV_EXCL_LINE
        break;
      }
      break;
    case rapidjson::kObjectType:
      for (rapidjson::Value::ConstMemberIterator it = value.MemberBegin();
           it != value.MemberEnd();
           ++it)
      {
        try
        {
          switch (it->value.GetType())
          {
          case rapidjson::kFalseType:
          case rapidjson::kTrueType:
            LOG_ERROR("Invalid format (true/false) in JSON block, ignoring");
            continue;
          case rapidjson::kNullType:
            LOG_ERROR("Invalid NULL in JSON block, ignoring");
            break;
          case rapidjson::kArrayType:
            for (rapidjson::Value::ConstValueIterator ary_it = it->value.Begin();
                 ary_it != it->value.End();
                 ++ary_it)
            {
              Diameter::Dictionary::AVP new_dict(vendors, it->name.GetString());
              Diameter::AVP avp(new_dict);
              add(avp.val_json(vendors, new_dict, *ary_it));
            }
            break;
          case rapidjson::kStringType:
          case rapidjson::kNumberType:
          case rapidjson::kObjectType:
            Diameter::Dictionary::AVP new_dict(vendors, it->name.GetString());
            Diameter::AVP avp(new_dict);
            add(avp.val_json(vendors, new_dict, it->value));
            break;
          }
        }
        catch (Diameter::Stack::Exception e)
        {
          LOG_WARNING("AVP %s not recognised, ignoring", it->value.GetString());
        }
      }
      break;
  }

  return *this;
}

Message::~Message()
{
  if (_free_on_delete)
  {
    fd_msg_free(_fd_msg);
  }
}

void Message::operator=(Message const& msg)
{
  if (_free_on_delete)
  {
    fd_msg_free(_fd_msg);
  }
  _dict = msg._dict;
  _fd_msg = msg._fd_msg;
  _free_on_delete = false;
  _master_msg = msg._master_msg;
}

// Given an AVP type, search a Diameter message for an AVP of this type. If one exists,
// return true and set str to the string value of this AVP. Otherwise return false.
bool Message::get_str_from_avp(const Dictionary::AVP& type, std::string& str) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    str = avps->val_str();
    return true;
  }
  else
  {
    return false;
  }
}

// Given an AVP type, search a Diameter message for an AVP of this type. If one exists,
// return true and set i32 to the integer value of this AVP. Otherwise return false.
bool Message::get_i32_from_avp(const Dictionary::AVP& type, int32_t& i32) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    i32 = avps->val_i32();
    return true;
  }
  else
  {
    return false;
  }
}

// Get the experimental result code from the EXPERIMENTAL_RESULT_CODE AVP
// of a Diameter message if it is present. This AVP is inside the
// EXPERIMENTAL_RESULT AVP.
int32_t Message::experimental_result_code() const
{
  int32_t experimental_result_code = 0;
  AVP::iterator avps = begin(dict()->EXPERIMENTAL_RESULT);
  if (avps != end())
  {
    AVP::iterator avps2 = avps->begin(dict()->EXPERIMENTAL_RESULT_CODE);
    if (avps2 != avps->end())
    {
      experimental_result_code = avps2->val_i32();
      LOG_DEBUG("Got Experimental-Result-Code %d", experimental_result_code);
    }
  }
  return experimental_result_code;
}

// Get the vendor ID from the VENDOR_ID AVP of a Diameter message if it
// is present. This AVP is inside the VENDOR_SPECIFIC_APPLICATION_ID AVP.
int32_t Message::vendor_id() const
{
  int32_t vendor_id = 0;
  AVP::iterator avps = begin(dict()->VENDOR_SPECIFIC_APPLICATION_ID);
  if (avps != end())
  {
    AVP::iterator avps2 = avps->begin(dict()->VENDOR_ID);
    if (avps2 != avps->end())
    {
      vendor_id = avps2->val_i32();
      LOG_DEBUG("Got Vendor-Id %d", vendor_id);
    }
  }
  return vendor_id;
}

void Message::send(SAS::TrailId trail)
{
  LOG_VERBOSE("Sending Diameter message of type %u", command_code());
  revoke_ownership();

  sas_log_tx(trail, 0);
  _stack->send(_fd_msg);
}

void Message::send(Transaction* tsx)
{
  LOG_VERBOSE("Sending Diameter message of type %u on transaction %p", command_code(), tsx);
  tsx->start_timer();
  revoke_ownership();

  sas_log_tx(tsx->trail(), 0);
  _stack->send(_fd_msg, tsx);
}

void Message::send(Transaction* tsx, unsigned int timeout_ms)
{
  LOG_VERBOSE("Sending Diameter message of type %u on transaction %p with timeout %u",
              command_code(), tsx, timeout_ms);
  tsx->start_timer();
  revoke_ownership();

  sas_log_tx(tsx->trail(), 0);
  _stack->send(_fd_msg, tsx, timeout_ms);
}

void Message::sas_log_rx(SAS::TrailId trail, uint32_t instance_id)
{
  int event_id = (is_request() ? SASEvent::DIAMETER_RX_REQ : SASEvent::DIAMETER_RX_RSP);
  SAS::Event event(trail, event_id, instance_id);

  event.add_static_param(command_code());

  std::string origin_host;
  get_origin_host(origin_host);
  event.add_var_param(origin_host);

  std::string origin_realm;
  get_origin_realm(origin_realm);
  event.add_var_param(origin_realm);

  if (!is_request())
  {
    sas_add_response_params(event);
  }

  SAS::report_event(event);
}

void Message::sas_log_tx(SAS::TrailId trail, uint32_t instance_id)
{
  int event_id = (is_request() ? SASEvent::DIAMETER_TX_REQ : SASEvent::DIAMETER_TX_RSP);
  SAS::Event event(trail, event_id, instance_id);

  event.add_static_param(command_code());

  std::string destination_host;
  get_destination_host(destination_host);
  event.add_var_param(destination_host);

  std::string destination_realm;
  get_destination_realm(destination_realm);
  event.add_var_param(destination_realm);

  if (!is_request())
  {
    sas_add_response_params(event);
  }

  SAS::report_event(event);
}

void Message::sas_add_response_params(SAS::Event& event)
{
  // For a response, add the result code and experimental result code to the SAS
  // event.  If either isn't present we log a value of 0 amd let the decoder
  // explain that the code was not present.
  int32_t result = 0;
  int32_t exp_result = 0;

  result_code(result);
  event.add_static_param(result);

  exp_result = experimental_result_code();
  event.add_static_param(exp_result);
}

void Message::sas_log_timeout(SAS::TrailId trail, uint32_t instance_id)
{
  SAS::Event event(trail, SASEvent::DIAMETER_REQ_TIMEOUT, instance_id);

  event.add_static_param(command_code());

  std::string destination_host;
  get_destination_host(destination_host);
  event.add_var_param(destination_host);

  std::string destination_realm;
  get_destination_realm(destination_realm);
  event.add_var_param(destination_realm);

  SAS::report_event(event);
}
