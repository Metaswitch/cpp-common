/**
 * @file diameterstack.h class definition wrapping a Diameter stack
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

#ifndef DIAMETER_H__
#define DIAMETER_H__

#include <string>
#include <unordered_map>

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <freeDiameter/libfdproto.h>
#include <rapidjson/document.h>

#include "utils.h"
#include "sas.h"
#include "baseresolver.h"
#include "communicationmonitor.h"

namespace Diameter
{
class Stack;
class Transaction;
class AVP;
class Message;
class PeerListener;

class Dictionary
{
public:
  class Object
  {
  public:
    inline Object(struct dict_object* dict) : _dict(dict) {};
    inline struct dict_object* dict() const { return _dict; }

  private:
    struct dict_object *_dict;
  };

  class Vendor : public Object
  {
  public:
    inline Vendor(const std::string vendor) : Object(find(vendor))
    {
      fd_dict_getval(dict(), &_vendor_data);
    }
    static struct dict_object* find(const std::string vendor);
    inline uint32_t vendor_id() const { return _vendor_data.vendor_id; }
    inline const struct dict_vendor_data* vendor_data() const { return &_vendor_data; }

  private:
    struct dict_vendor_data _vendor_data;
  };

  class Application : public Object
  {
  public:
    enum Type
    {
      ACCT,
      AUTH
    };
    inline Application(const std::string application) : Object(find(application))
    {
      fd_dict_getval(dict(), &_application_data);
    }
    static struct dict_object* find(const std::string application);
    inline uint32_t application_id() const { return _application_data.application_id; }
    inline const struct dict_application_data* application_data() const { return &_application_data; }

  private:
    struct dict_application_data _application_data;
  };

  class Message : public Object
  {
  public:
    inline Message(const std::string message) : Object(find(message))
    {
      fd_dict_getval(dict(), &_cmd_data);
    }
    static struct dict_object* find(const std::string message);
    inline const struct dict_cmd_data* cmd_data() const { return &_cmd_data; }

  private:
    struct dict_cmd_data _cmd_data;
  };

  class AVP : public Object
  {
  public:
    inline AVP(const std::string avp) : Object(find(avp))
    {
      fd_dict_getval(dict(), &_avp_data);
    };
    inline AVP(const std::string vendor,
               const std::string avp) : Object(find(vendor, avp))
    {
      fd_dict_getval(dict(), &_avp_data);
    };
    inline AVP(const std::vector<std::string>& vendors,
               const std::string avp) : Object(find(vendors, avp))
    {
      fd_dict_getval(dict(), &_avp_data);
    };
    static struct dict_object* find(const std::string avp);
    static struct dict_object* find(const std::string vendor, const std::string avp);
    static struct dict_object* find(const std::vector<std::string>& vendor, const std::string avp);

    inline const struct dict_avp_data* avp_data() const { return &_avp_data; }
    inline enum dict_avp_basetype base_type() const { return _avp_data.avp_basetype; };

  private:
    struct dict_avp_data _avp_data;
  };

  Dictionary();
  const AVP SESSION_ID;
  const AVP VENDOR_SPECIFIC_APPLICATION_ID;
  const AVP VENDOR_ID;
  const AVP AUTH_APPLICATION_ID;
  const AVP ACCT_APPLICATION_ID;
  const AVP AUTH_SESSION_STATE;
  const AVP ORIGIN_REALM;
  const AVP ORIGIN_HOST;
  const AVP DESTINATION_REALM;
  const AVP DESTINATION_HOST;
  const AVP USER_NAME;
  const AVP RESULT_CODE;
  const AVP DIGEST_HA1;
  const AVP DIGEST_REALM;
  const AVP DIGEST_QOP;
  const AVP EXPERIMENTAL_RESULT;
  const AVP EXPERIMENTAL_RESULT_CODE;
  const AVP ACCT_INTERIM_INTERVAL;
};

class Transaction
{
public:
  Transaction(Dictionary* dict, SAS::TrailId);
  virtual ~Transaction();

  virtual void on_response(Message& rsp) = 0;
  virtual void on_timeout() = 0;

  // Methods to start and stop the duration stopwatch.  Should only be called by
  // the diameter stack.
  void start_timer() { _stopwatch.start(); }
  void stop_timer() { _stopwatch.stop(); }

  /// Get the duration of the transaction in microseconds.
  ///
  /// @param duration_us The duration. Only valid if the function returns true.
  /// @return whether the duration was obtained successfully.
  bool get_duration(unsigned long& duration_us)
  {
    return _stopwatch.read(duration_us);
  }

  static void on_response(void* data, struct msg** rsp);
  static void on_timeout(void* data, DiamId_t to, size_t to_len, struct msg** req);

  SAS::TrailId trail() { return _trail; }

protected:
  Dictionary* _dict;
  Utils::StopWatch _stopwatch;
  SAS::TrailId _trail;
};

class AVP
{
public:
  class iterator;

  inline AVP(const Dictionary::AVP& type)
  {
    fd_msg_avp_new(type.dict(), 0, &_avp);
  }

  inline AVP(struct avp* avp) : _avp(avp) {}
  inline AVP(AVP const& avp) : _avp(avp.avp()) {}
  inline AVP& operator=(AVP const& avp) {_avp = avp.avp(); return *this;}
  inline struct avp* avp() const {return _avp;}

  inline iterator begin() const;
  inline iterator begin(const Dictionary::AVP& type) const;
  inline iterator end() const;

  inline std::string val_str() const
  {
    struct avp_hdr* hdr = avp_hdr();
    return std::string((char*)hdr->avp_value->os.data, hdr->avp_value->os.len);
  }
  inline const uint8_t* val_os(size_t& len) const
  {
    struct avp_hdr* hdr = avp_hdr();
    len = hdr->avp_value->os.len;
    return hdr->avp_value->os.data;
  }
  inline int32_t val_i32() const {return avp_hdr()->avp_value->i32;}
  inline int32_t val_i64() const {return avp_hdr()->avp_value->i64;}
  inline int32_t val_u32() const {return avp_hdr()->avp_value->u32;}
  inline int32_t val_u64() const {return avp_hdr()->avp_value->u64;}

  inline AVP& val_str(std::string str)
  {
    return val_os((uint8_t*)str.c_str(), str.length());
  }
  inline AVP& val_os(uint8_t* data, size_t len)
  {
    union avp_value val;
    val.os.data = data;
    val.os.len = len;
    fd_msg_avp_setvalue(_avp, &val);
    return *this;
  }
  inline AVP& val_i32(int32_t i32)
  {
    fd_msg_avp_value_encode(&i32, _avp);
    return *this;
  }
  inline AVP& val_i64(int64_t i64)
  {
    fd_msg_avp_value_encode(&i64, _avp);
    return *this;
  }
  inline AVP& val_u32(uint32_t u32)
  {
    fd_msg_avp_value_encode(&u32, _avp);
    return *this;
  }
  inline AVP& val_u64(uint64_t u64)
  {
    fd_msg_avp_value_encode(&u64, _avp);
    return *this;
  }

  bool get_str_from_avp(const Dictionary::AVP& type, std::string& str) const;
  bool get_i32_from_avp(const Dictionary::AVP& type, int32_t& i32) const;
  bool get_u32_from_avp(const Dictionary::AVP& type, uint32_t& u32) const;

  // Populate this AVP from a JSON object
  AVP& val_json(const std::vector<std::string>& vendors,
                const Diameter::Dictionary::AVP& dict,
                const rapidjson::Value& contents);

  inline AVP& add(AVP& avp)
  {
    fd_msg_avp_add(_avp, MSG_BRW_LAST_CHILD, avp.avp());
    return *this;
  }

private:
  struct avp* _avp;

  inline struct avp_hdr* avp_hdr() const
  {
    struct avp_hdr* hdr;
    fd_msg_avp_hdr(_avp, &hdr);
    return hdr;
  }
};

class Message
{
public:
  inline Message(const Dictionary* dict, const Dictionary::Message& type, Stack* stack) : _dict(dict), _stack(stack), _free_on_delete(true), _master_msg(this), _result(0)
  {
    fd_msg_new(type.dict(), MSGFL_ALLOC_ETEID, &_fd_msg);
  }
  inline Message(const Dictionary* dict, struct msg* msg, Stack* stack) : _dict(dict), _fd_msg(msg), _stack(stack),  _free_on_delete(true), _master_msg(this), _result(0) {};
  inline Message(const Message& msg) : _dict(msg._dict), _fd_msg(msg._fd_msg), _stack(msg._stack),  _free_on_delete(false), _master_msg(msg._master_msg), _result(0) {};
  virtual ~Message();
  inline const Dictionary* dict() const {return _dict;}
  inline struct msg* fd_msg() const {return _fd_msg;}
  inline uint32_t command_code() const {return msg_hdr()->msg_code;}
  inline void build_response(Message &msg)
  {
    // When we construct an answer from a request, freeDiameter associates
    // the request with the new answer, so we only need to keep track of the
    // answer.
    msg.revoke_ownership();

    // _msg will point to the answer once this function is done.
    fd_msg_new_answer_from_req(fd_g_config->cnf_dict, &_fd_msg, MSGFL_ANSW_NOSID);
    copy_session_id(msg);
    claim_ownership();
  }

  inline Message& copy_session_id(Message &msg)
  {
    std::string str;
    msg.get_str_from_avp(dict()->SESSION_ID, str);
    add_session_id(str);
    return *this;
  }

  // Add a Session-ID to a message (either a new one or a specified).
  inline Message& add_new_session_id()
  {
    fd_msg_new_session(_fd_msg, NULL, 0);
    return *this;
  }
  Message& add_session_id(const std::string& session_id);

  inline Message& add_app_id(const Dictionary::Application::Type type,
                             const Dictionary::Vendor& vendor,
                             const Dictionary::Application& app)
  {
    Diameter::AVP vendor_specific_application_id(dict()->VENDOR_SPECIFIC_APPLICATION_ID);
    vendor_specific_application_id.add(Diameter::AVP(dict()->VENDOR_ID).val_i32(vendor.vendor_id()));
    if (type == Dictionary::Application::ACCT)
    {
      // LCOV_EXCL_START - we never use a vendor-specific accounting application
      vendor_specific_application_id.add(Diameter::AVP(dict()->ACCT_APPLICATION_ID).val_i32(app.application_id()));
      // LCOV_EXCL_STOP - we never use a vendor-specific accounting application
    }
    else
    {
      vendor_specific_application_id.add(Diameter::AVP(dict()->AUTH_APPLICATION_ID).val_i32(app.application_id()));
    }
    add(vendor_specific_application_id);
    return *this;
  }
  inline Message& add_app_id(const Dictionary::Application::Type type,
                             const Dictionary::Application& app)
  {
    if (type == Dictionary::Application::ACCT)
    {
      add(Diameter::AVP(dict()->ACCT_APPLICATION_ID).val_i32(app.application_id()));
    }
    else
    {
      // LCOV_EXCL_START - we only use vendor-specific auth applications
      add(Diameter::AVP(dict()->AUTH_APPLICATION_ID).val_i32(app.application_id()));
      // LCOV_EXCL_STOP - we only use vendor-specific auth applications
    }
    return *this;
  }
  inline Message& add_origin()
  {
    fd_msg_add_origin(_fd_msg, 0);
    return *this;
  }
  inline Message& set_result_code(const std::string result_code)
  {
    // Remove const from result_code. This is safe because freeDiameter doesn't change
    // result_code, although it is complicated to change the fd_msg_rescode_set function
    // to accept a const argument.
    fd_msg_rescode_set(_fd_msg, const_cast<char*>(result_code.c_str()), NULL, NULL, 1);
    return *this;
  }
  inline Message& add(AVP& avp)
  {
    fd_msg_avp_add(_fd_msg, MSG_BRW_LAST_CHILD, avp.avp());
    return *this;
  }
  bool get_str_from_avp(const Dictionary::AVP& type, std::string& str) const;
  bool get_i32_from_avp(const Dictionary::AVP& type, int32_t& i32) const;
  bool get_u32_from_avp(const Dictionary::AVP& type, uint32_t& i32) const;
  inline bool result_code(int32_t& result)
  {
    if (!_result)
    {
      get_i32_from_avp(dict()->RESULT_CODE, _result);
    }
    result = _result;
    return (result != 0);
  }
  int32_t experimental_result_code() const;
  int32_t vendor_id() const;
  inline std::string impi() const
  {
    std::string str;
    get_str_from_avp(dict()->USER_NAME, str);
    return str;
  }
  inline int32_t auth_session_state() const
  {
    int32_t i32;
    get_i32_from_avp(dict()->AUTH_SESSION_STATE, i32);
    return i32;
  }

  inline bool get_origin_host(std::string& str) { return get_str_from_avp(_dict->ORIGIN_HOST, str); }
  inline bool get_origin_realm(std::string& str) { return get_str_from_avp(_dict->ORIGIN_REALM, str); }
  inline bool get_destination_host(std::string& str) { return get_str_from_avp(_dict->DESTINATION_HOST, str); }
  inline bool get_destination_realm(std::string& str) { return get_str_from_avp(_dict->DESTINATION_REALM, str); }
  inline bool is_request() { return bool(msg_hdr()->msg_flags & CMD_FLAG_REQUEST); }

  inline AVP::iterator begin() const;
  inline AVP::iterator begin(const Dictionary::AVP& type) const;
  inline AVP::iterator end() const;
  virtual void send(SAS::TrailId trail);
  virtual void send(Transaction* tsx);
  virtual void send(Transaction* tsx, unsigned int timeout_ms);
  void operator=(Message const&);

  inline void revoke_ownership()
  {
    _master_msg->_free_on_delete = false;
  }

  inline void claim_ownership()
  {
    _free_on_delete = true;
    _master_msg = this;
  }

private:
  const Dictionary* _dict;
  struct msg* _fd_msg;
  Stack* _stack;
  bool _free_on_delete;
  Message* _master_msg;
  int32_t _result;

  inline struct msg_hdr* msg_hdr() const
  {
    struct msg_hdr* hdr;
    fd_msg_hdr(_fd_msg, &hdr);
    return hdr;
  }
};

class AVP::iterator
{
public:
  inline iterator(const AVP& parent_avp) : _avp(find_first_child(parent_avp.avp())) {memset(&_filter_avp_data, 0, sizeof(_filter_avp_data));}
  inline iterator(const AVP& parent_avp, const Dictionary::AVP& child_type) : _filter_avp_data(*child_type.avp_data()), _avp(find_first_child(parent_avp.avp(), _filter_avp_data)) {}
  inline iterator(const Message& parent_msg) : _avp(find_first_child(parent_msg.fd_msg())) {memset(&_filter_avp_data, 0, sizeof(_filter_avp_data));}
  inline iterator(const Message& parent_msg, const Dictionary::AVP& child_type) : _filter_avp_data(*child_type.avp_data()), _avp(find_first_child(parent_msg.fd_msg(), _filter_avp_data)) {}
  inline iterator(struct avp* avp) : _avp(avp) {memset(&_filter_avp_data, 0, sizeof(_filter_avp_data));};
  inline ~iterator() {};

  inline iterator& operator=(const iterator& other)
  {
    _avp = other._avp;
    _filter_avp_data = other._filter_avp_data;
    return *this;
  }
  inline bool operator==(const iterator& other) const {return (_avp.avp() == other._avp.avp());}
  inline bool operator!=(const iterator& other) const {return (_avp.avp() != other._avp.avp());}

  iterator& operator++()
  {
    if (_avp.avp() != NULL)
    {
      _avp = AVP(find_next(_avp.avp(), _filter_avp_data));
    }
    return *this;
  }
  inline iterator operator++(int)
  {
    iterator old(*this);
    ++(*this);
    return old;
  }

  inline AVP& operator*() {return _avp;}
  inline AVP* operator->() {return &_avp;}

private:
  inline static struct avp* find_first_child(msg_or_avp* parent)
  {
    msg_or_avp* first_child = NULL;
    fd_msg_browse_internal(parent, MSG_BRW_FIRST_CHILD, &first_child, NULL);
    return (struct avp*)first_child;
  }
  static struct avp* find_first_child(msg_or_avp* parent, struct dict_avp_data& avp_data)
  {
    struct avp* avp = find_first_child(parent);
    if (avp != NULL)
    {
      struct avp_hdr* hdr;
      fd_msg_avp_hdr(avp, &hdr);
      if (!(((avp_data.avp_code == 0) && (avp_data.avp_vendor == 0)) ||
           ((hdr->avp_code == avp_data.avp_code) && (hdr->avp_vendor == avp_data.avp_vendor))))
      {
        avp = find_next(avp, avp_data);
      }
    }
    return avp;
  }
  inline static struct avp* find_next(struct avp* avp, struct dict_avp_data& avp_data)
  {
    fd_msg_browse_internal(avp, MSG_BRW_NEXT, (msg_or_avp**)&avp, NULL);
    if ((avp_data.avp_code != 0) || (avp_data.avp_vendor != 0))
    {
       while (avp != NULL)
      {
        struct avp_hdr* hdr;
        fd_msg_avp_hdr(avp, &hdr);
        if ((hdr->avp_code == avp_data.avp_code) && (hdr->avp_vendor == avp_data.avp_vendor))
        {
          break;
        }
        fd_msg_browse_internal(avp, MSG_BRW_NEXT, (msg_or_avp**)&avp, NULL);
      }
    }
    return avp;
  }

  struct dict_avp_data _filter_avp_data;
  AVP _avp;
};

class Peer
{
public:
  Peer(const std::string& host,
       const std::string& realm = "",
       uint32_t idle_time = 0,
       PeerListener* listener = NULL) :
       _addr_info_specified(false),
       _host(host),
       _realm(realm),
       _idle_time(idle_time),
       _listener(listener),
       _connected(false) {}

  Peer(AddrInfo addr_info,
       const std::string& host,
       const std::string& realm = "",
       uint32_t idle_time = 0,
       PeerListener* listener = NULL) :
       _addr_info(addr_info),
       _addr_info_specified(true),
       _host(host),
       _realm(realm),
       _idle_time(idle_time),
       _listener(listener),
       _connected(false) {}

  inline const AddrInfo& addr_info() const {return _addr_info;}
  inline const bool& addr_info_specified() const {return _addr_info_specified;}
  inline const std::string& host() const {return _host;}
  inline const std::string& realm() const {return _realm;}
  inline uint32_t idle_time() const {return _idle_time;}
  inline PeerListener* listener() const {return _listener;}
  inline const bool& connected() const {return _connected;}
  inline void set_connected() {_connected = true;}

private:
  AddrInfo _addr_info;
  bool _addr_info_specified;
  std::string _host;
  std::string _realm;
  uint32_t _idle_time;
  PeerListener* _listener;
  bool _connected;
};

class PeerListener
{
public:
  virtual void connection_succeeded(Peer* peer) = 0;
  virtual void connection_failed(Peer* peer) = 0;
};

class Stack
{
public:
  class Exception
  {
  public:
    inline Exception(const char* func, int rc) : _func(func), _rc(rc) {};
    const char* _func;
    const int _rc;
  };

  class HandlerInterface
  {
  public:
    /// Process a new Diameter request message.
    ///
    /// @param req pointer to a freeDiameter message. This function takes
    ///   ownership of the message and is responsible for sending an appropriate
    ///   answer.
    /// @param trail the SAS trail ID associated with the reqeust.
    virtual void process_request(struct msg** req, SAS::TrailId trail) = 0;

    /// Get the diameter dictionary this handler uses (this is required for
    /// SAS logging).
    ///
    /// @return a pointer to a Diameter::Dictionary object.
    virtual const Dictionary* get_dict() = 0;
  };


  static inline Stack* get_instance() {return INSTANCE;};
  virtual void initialize();
  virtual void configure(std::string filename, 
                         CommunicationMonitor* comm_monitor = NULL);
  virtual void advertize_application(const Dictionary::Application::Type type,
                                     const Dictionary::Application& app);
  virtual void advertize_application(const Dictionary::Application::Type type,
                                     const Dictionary::Vendor& vendor,
                                     const Dictionary::Application& app);
  virtual void register_handler(const Dictionary::Application& app,
                                const Dictionary::Message& msg,
                                HandlerInterface* handler);
  virtual void register_fallback_handler(const Dictionary::Application& app);
  virtual void start();
  virtual void stop();
  virtual void wait_stopped();
  std::unordered_map<std::string, std::unordered_map<std::string, struct dict_object*>>& avp_map()
  {
    return _avp_map;
  }

  virtual void send(struct msg* fd_msg, SAS::TrailId trail);
  virtual void send(struct msg* fd_msg, Transaction* tsx);
  virtual void send(struct msg* fd_msg, Transaction* tsx, unsigned int timeout_ms);

  virtual void report_tsx_result(int32_t rc);
  virtual void report_tsx_timeout();

  virtual bool add(Peer* peer);
  virtual void remove(Peer* peer);

private:
  static Stack* INSTANCE;
  static Stack DEFAULT_INSTANCE;

  Stack();
  virtual ~Stack();
  static int request_callback_fn(struct msg** req, struct avp* avp, struct session* sess, void* handler, enum disp_action* act);
  static int fallback_request_callback_fn(struct msg** msg, struct avp* avp, struct session* sess, void* opaque, enum disp_action* act);

  // Don't implement the following, to avoid copies of this instance.
  Stack(Stack const&);
  void operator=(Stack const&);

  static void logger(int fd_log_level, const char* fmt, va_list args);

  static void fd_null_hook_cb(enum fd_hook_type type, struct msg* msg, struct peer_hdr* peer, void *other, struct fd_hook_permsgdata* pmd, void* user_data);

  void fd_peer_hook_cb(enum fd_hook_type type, struct msg* msg, struct peer_hdr* peer, void *other, struct fd_hook_permsgdata* pmd);
  static void fd_peer_hook_cb(enum fd_hook_type type, struct msg* msg, struct peer_hdr* peer, void* other, struct fd_hook_permsgdata* pmd, void* stack_ptr);

  void fd_error_hook_cb(enum fd_hook_type type, struct msg* msg, struct peer_hdr* peer, void *other, struct fd_hook_permsgdata* pmd);
  static void fd_error_hook_cb(enum fd_hook_type type, struct msg* msg, struct peer_hdr* peer, void* other, struct fd_hook_permsgdata* pmd, void* stack_ptr);

  void set_trail_id(struct msg* fd_msg, SAS::TrailId trail);
  static void fd_sas_log_diameter_message(enum fd_hook_type type,
                                          struct msg * msg,
                                          struct peer_hdr * peer,
                                          void * other,
                                          struct fd_hook_permsgdata *pmd,
                                          void * stack_ptr);

  bool _initialized;
  struct disp_hdl* _callback_handler; /* Handler for requests callback */
  struct disp_hdl* _callback_fallback_handler; /* Handler for unexpected messages callback */
  struct fd_hook_hdl* _peer_cb_hdlr; /* Handler for the callback registered for connections to peers */
  struct fd_hook_hdl* _error_cb_hdlr; /* Handler for the callback
                                       * registered for routing errors */
  struct fd_hook_hdl* _null_cb_hdlr; /* Handler for the NULL callback registered to overload the default hook handlers */
  struct fd_hook_hdl* _sas_cb_hdlr;

  // Data handle for the SAS logging hook. This must be static because
  // freeDiameter has a limit on the maximum number of handles that can be
  // registered, it stores the handles statically, and there is no way to
  // unregister them.
  static struct fd_hook_data_hdl* _sas_cb_data_hdl;
  std::vector<Peer*> _peers;
  pthread_mutex_t _peers_lock;
  CommunicationMonitor* _comm_monitor;

  // Map of Vendor->AVP name->AVP dictionary
  std::unordered_map<std::string, std::unordered_map<std::string, struct dict_object*>> _avp_map;

  void populate_avp_map();
  void populate_vendor_map(const std::string& vendor_name,
                           struct dict_object* vendor_dict);

  void remove_int(Peer* peer);
};

/// @class SpawningHandler
///
/// Many handlers use an asynchronous non-blocking execution model.
/// Instead of blocking the current thread when doing external operations,
/// they register callbacks that are called (potentially on a different
/// thread) when the operation completes.  These handlers create a new
/// "task" object per request that tracks the state necessary to continue
/// processing when the callback is triggered.
///
/// This class is an implementation of the handler part of this model.
///
/// It takes two template parameters:
/// @tparam T the type of the task.
/// @tparam C Although not mandatory according to the HandlerInterface, in
///   practice all handlers have some sort of associated config. This is
///   the type of the config object.
template <class T, class C>
class SpawningHandler : public Stack::HandlerInterface
{
public:
  SpawningHandler(const Dictionary* dict, const C* cfg) :
    _cfg(cfg), _dict(dict)
  {}

  /// Process a diameter request by spawning a new task and running it.
  /// @param fd_msg the diameter request.
  /// @param trail the SAS trail ID for the request.
  void process_request(struct msg** fd_msg, SAS::TrailId trail)
  {
    T* task= new T(_dict, fd_msg, _cfg, trail);
    task->run();
  }

  /// Get the dictionary used by the tasks.
  /// @return a pointer to the dictionary.
  inline const Dictionary* get_dict()
  {
    return _dict;
  }

private:
  const C* _cfg;
  const Dictionary* _dict;
};

/// @class Task
///
/// Base class for per-request task objects spawned by a SpawningHandler.
class Task
{
public:
  inline Task(const Dictionary* dict,
              struct msg** fd_msg,
              SAS::TrailId trail) :
    _msg(dict, *fd_msg, Stack::get_instance()), _trail(trail)
  {}
  virtual ~Task() {}

  virtual void run() = 0;

  SAS::TrailId trail() { return _trail; }

protected:
  Diameter::Message _msg;
  SAS::TrailId _trail;
};

AVP::iterator AVP::begin() const {return AVP::iterator(*this);}
AVP::iterator AVP::begin(const Dictionary::AVP& type) const {return AVP::iterator(*this, type);}
AVP::iterator AVP::end() const {return AVP::iterator(NULL);}

AVP::iterator Message::begin() const {return AVP::iterator(*this);}
AVP::iterator Message::begin(const Dictionary::AVP& type) const {return AVP::iterator(*this, type);}
AVP::iterator Message::end() const {return AVP::iterator(NULL);}
};

/// Per-message data structure for SAS logging in free-diameter hooks.  This
/// must have the exact name fd_hook_permsgdata.
struct fd_hook_permsgdata
{
  SAS::TrailId trail;
};

#endif
