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

using namespace Diameter;

static struct disp_hdl * callback_handler = NULL; /* Handler for requests callback */

Stack* Stack::INSTANCE = &DEFAULT_INSTANCE;
Stack Stack::DEFAULT_INSTANCE;

Stack::Stack() : _initialized(false)
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

void Stack::register_handler(const Dictionary::Application& app, const Dictionary::Message& msg, BaseHandlerFactory* factory)
{
  // Register a callback for messages from our application with the specified message type. DISP_HOW_CC indicates that we
  // want to match on command code (and allows us to optionally match on application if specified). Use a pointer to our
  // HandlerFactory to pass through to our callback function.
  struct disp_when data;
  memset(&data, 0, sizeof(data));
  data.app = app.dict();
  data.command = msg.dict();
  int rc = fd_disp_register(handler_callback_fn, DISP_HOW_CC, &data, (void *)factory, &callback_handler);

  if (rc != 0)
  {
    throw Exception("fd_disp_register", rc); //LCOV_EXCL_LINE
  }
}

void Stack::register_fallback_handler(const Dictionary::Application &app)
{
  // Register a fallback callback for messages of an unexpected type to our application
  // so that we can log receiving an unexpected message. We use the same callback function
  // to handle unexpected messages as expected messages, but we don't pass through a HandlerFactory.
  struct disp_when data;
  memset(&data, 0, sizeof(data));
  data.app = app.dict();
  int rc = fd_disp_register(handler_callback_fn, DISP_HOW_APPID, &data, NULL, &callback_handler);

  if (rc != 0)
  {
    throw Exception("fd_disp_register", rc); //LCOV_EXCL_LINE
  }
}


int Stack::handler_callback_fn(struct msg** req, struct avp* avp, struct session* sess, void* handler_factory, enum disp_action* act)
{
  if (handler_factory == NULL)
  {
    // This means we have received a message of an unexpected type.
    LOG_DEBUG("Message of unexpected type received");
    return 0;
  }

  // Convert the received message into one our Message objects, and create a new handler instance of the 
  // correct type.
  Message msg(((Diameter::Stack::BaseHandlerFactory*)handler_factory)->_dict, *req);
  Handler* handler = ((Diameter::Stack::BaseHandlerFactory*)handler_factory)->create(msg);
  handler->run();

  // The handler will turn the message associated with the handler into an answer which we wish to send to the HSS.
  // Setting the action to DISP_ACT_SEND ensures that we will send this answer on without going to any other callbacks.
  // Return 0 to indicate no errors with the callback.
  *req = handler->fd_msg();
  *act = DISP_ACT_SEND;
  return 0;
}

void Stack::start()
{
  initialize();
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
    if (callback_handler)
    {
      (void)fd_disp_unregister(&callback_handler, NULL);
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

Transaction::Transaction(Dictionary* dict) : _dict(dict)
{
}

Transaction::~Transaction()
{
}

void Transaction::on_response(void* data, struct msg** rsp)
{
  Transaction* tsx = (Transaction*)data;
  Message msg(tsx->_dict, *rsp);
  tsx->on_response(msg);
  delete tsx;
  // Null out the message so that freeDiameter doesn't try to send it on.
  *rsp = NULL;
}

void Transaction::on_timeout(void* data, DiamId_t to, size_t to_len, struct msg** req)
{
  Transaction* tsx = (Transaction*)data;
  Message msg(tsx->_dict, *req);
  tsx->on_timeout();
  delete tsx;
  // Null out the message so that freeDiameter doesn't try to send it on.
  *req = NULL;
}

AVP& AVP::val_json(const rapidjson::Value& data)
{
  for (rapidjson::Value::ConstMemberIterator it = data.MemberBegin();
      it != data.MemberEnd();
      ++it)
  {
    switch (it->value.GetType())
    {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
    case rapidjson::kNullType:
      LOG_ERROR("Invalid format (true/false) in JSON block, ignoring");
      continue;
    case rapidjson::kStringType:
      add(Diameter::AVP(it->name.GetString()).val_str(it->value.GetString()));
      break;
    case rapidjson::kNumberType:
      add(Diameter::AVP(it->name.GetString()).val_u32(it->value.GetUint()));
      break;
    case rapidjson::kArrayType:
      for (rapidjson::Value::ConstValueIterator ary_it = it->value.Begin();
          ary_it != it->value.End();
          ary_it++)
      {
        switch (ary_it->GetType())
        {
        case rapidjson::kNullType:
        case rapidjson::kTrueType:
        case rapidjson::kFalseType:
        case rapidjson::kObjectType:
        case rapidjson::kArrayType:
          LOG_ERROR("Invalid format for body of repeated AVP, ignoring");
          continue;
        case rapidjson::kNumberType:
          add(Diameter::AVP(it->name.GetString()).val_u32(ary_it->GetUint()));
          break;
        case rapidjson::kStringType:
          add(Diameter::AVP(it->name.GetString()).val_str(ary_it->GetString()));
          break;
        }
      }
      break;
    case rapidjson::kObjectType:
      add(Diameter::AVP(it->name.GetString()).val_json(it->value));
      break;
    }
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
  _free_on_delete = msg._free_on_delete;
}

// Given an AVP type, search a Diameter message for an AVP of this type. If one exists,
// return true and set str to the string value of this AVP. Otherwise return false.
bool Message::get_str_from_avp(const Dictionary::AVP& type, std::string* str) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    (*str) = avps->val_str();
    return true;
  }
  else
  {
    return false;
  }
}

// Given an AVP type, search a Diameter message for an AVP of this type. If one exists,
// return true and set i32 to the integer value of this AVP. Otherwise return false.
bool Message::get_i32_from_avp(const Dictionary::AVP& type, int* i32) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    (*i32) = avps->val_i32();
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
int Message::experimental_result_code() const
{
  int experimental_result_code = 0;
  AVP::iterator avps = begin(dict()->EXPERIMENTAL_RESULT);
  if (avps != end())
  {
    avps = avps->begin(dict()->EXPERIMENTAL_RESULT_CODE);
    if (avps != end())
    {
      experimental_result_code = avps->val_i32();
    }
  }
  return experimental_result_code;
}

// Get the vendor ID from the VENDOR_ID AVP of a Diameter message if it
// is present. This AVP is inside the VENDOR_SPECIFIC_APPLICATION_ID AVP.
int Message::vendor_id() const
{
  int vendor_id = 0;
  AVP::iterator avps = begin(dict()->VENDOR_SPECIFIC_APPLICATION_ID);
  if (avps != end())
  {
    avps = avps->begin(dict()->VENDOR_ID);
    if (avps != end())
    {
      vendor_id = avps->val_i32();
    }
  }
  return vendor_id;
}

void Message::send()
{
  fd_msg_send(&_fd_msg, NULL, NULL);
  _free_on_delete = false;
}

void Message::send(Transaction* tsx)
{
  fd_msg_send(&_fd_msg, Transaction::on_response, tsx);
  _free_on_delete = false;
}

void Message::send(Transaction* tsx, unsigned int timeout_ms)
{
  struct timespec timeout_ts;
  // TODO: Check whether this should be CLOCK_MONOTONIC - freeDiameter uses CLOCK_REALTIME but
  //       this feels like it might suffer over time changes.
  clock_gettime(CLOCK_REALTIME, &timeout_ts);
  timeout_ts.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;
  timeout_ts.tv_sec += timeout_ms / 1000 + timeout_ts.tv_nsec / (1000 * 1000 * 1000);
  timeout_ts.tv_nsec = timeout_ts.tv_nsec % (1000 * 1000 * 1000);
  fd_msg_send_timeout(&_fd_msg, Transaction::on_response, tsx, Transaction::on_timeout, &timeout_ts);
  _free_on_delete = false;
}
