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

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

namespace Diameter
{
class Stack;
class Transaction;
class AVP;
class Message;

class Dictionary
{
public:
  class Object
  {
  public:
    inline Object(struct dict_object* dict) : _dict(dict) {};
    inline struct dict_object* dict() const {return _dict;}

  private:
    struct dict_object *_dict;
  };

  class Vendor : public Object
  {
  public:
    inline Vendor(const std::string vendor) : Object(find(vendor)) {};
    static struct dict_object* find(const std::string vendor);
  };

  class Application : public Object
  {
  public:
    inline Application(const std::string application) : Object(find(application)) {};
    static struct dict_object* find(const std::string application);
  };

  class Message : public Object
  {
  public:
    inline Message(const std::string message) : Object(find(message)) {};
    static struct dict_object* find(const std::string message);
  };

  class AVP : public Object
  {
  public:
    inline AVP(const std::string avp) : Object(find(avp)) {};
    inline AVP(const std::string vendor, const std::string avp) : Object(find(vendor, avp)) {};
    static struct dict_object* find(const std::string avp);
    static struct dict_object* find(const std::string vendor, const std::string avp);
  };

  Dictionary();
  const AVP SESSION_ID;
  const AVP VENDOR_SPECIFIC_APPLICATION_ID;
  const AVP VENDOR_ID;
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

  static inline Stack* get_instance() {return INSTANCE;};
  virtual void initialize();
  virtual void configure(std::string filename);
  virtual void advertize_application(const Dictionary::Application& app);
  virtual void start();
  virtual void stop();
  virtual void wait_stopped();

private:
  static Stack* INSTANCE;
  static Stack DEFAULT_INSTANCE;

  Stack();
  virtual ~Stack();

  // Don't implement the following, to avoid copies of this instance.
  Stack(Stack const&);
  void operator=(Stack const&);

  static void logger(int fd_log_level, const char* fmt, va_list args);

  bool _initialized;
};

class Transaction
{
public:
  Transaction(Dictionary* dict);
  virtual ~Transaction();

  virtual void on_response(Message& rsp) = 0;
  virtual void on_timeout() = 0;

  static void on_response(void* data, struct msg** rsp);
  static void on_timeout(void* data, DiamId_t to, size_t to_len, struct msg** req);

private:
  Dictionary* _dict;
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
    union avp_value val;
    val.i32 = i32;
    fd_msg_avp_setvalue(_avp, &val);
    return *this;
  }
  inline AVP& val_i64(int64_t i64)
  {
    union avp_value val;
    val.i64 = i64;
    fd_msg_avp_setvalue(_avp, &val);
    return *this;
  }
  inline AVP& val_u32(uint32_t u32)
  {
    union avp_value val;
    val.u32 = u32;
    fd_msg_avp_setvalue(_avp, &val);
    return *this;
  }
  inline AVP& val_u64(uint64_t u64)
  {
    union avp_value val;
    val.u64 = u64;
    fd_msg_avp_setvalue(_avp, &val);
    return *this;
  }

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
  inline Message(const Dictionary* dict, const Dictionary::Message& type) : _dict(dict), _free_on_delete(true)
  {
    fd_msg_new(type.dict(), MSGFL_ALLOC_ETEID, &_msg);
  }
  inline Message(Dictionary* dict, struct msg* msg) : _dict(dict), _msg(msg), _free_on_delete(true) {};
  inline Message(const Message& msg) : _dict(msg._dict), _msg(msg._msg), _free_on_delete(false) {};
  virtual ~Message();
  inline const Dictionary* dict() const {return _dict;}
  inline struct msg* msg() const {return _msg;}
  inline Message& add_new_session_id()
  {
    fd_msg_new_session(_msg, NULL, 0);
    return *this;
  }
  inline Message& add_vendor_spec_app_id()
  {
    Diameter::AVP vendor_specific_application_id(dict()->VENDOR_SPECIFIC_APPLICATION_ID);
    vendor_specific_application_id.add(Diameter::AVP(dict()->VENDOR_ID).val_i32(10415));
    add(vendor_specific_application_id);
    return *this;
  }
  inline Message& add_origin()
  {
    fd_msg_add_origin(_msg, 0);
    return *this;
  }
  inline Message& add(AVP& avp)
  {
    fd_msg_avp_add(_msg, MSG_BRW_LAST_CHILD, avp.avp());
    return *this;
  }
  bool get_str_from_avp(const Dictionary::AVP& type, std::string* str) const;
  bool get_i32_from_avp(const Dictionary::AVP& type, int* i32) const;
  inline bool result_code(int* i32)
  {
    return get_i32_from_avp(dict()->RESULT_CODE, i32);
  }
  int experimental_result_code() const;
  int vendor_id() const;
  inline bool impi(std::string* str) const
  {
    return get_str_from_avp(dict()->USER_NAME, str);
  }
  inline AVP::iterator begin() const;
  inline AVP::iterator begin(const Dictionary::AVP& type) const;
  inline AVP::iterator end() const;
  virtual void send();
  virtual void send(Transaction* tsx);
  virtual void send(Transaction* tsx, unsigned int timeout_ms);
  void operator=(Message const&);

private:
  const Dictionary* _dict;
  struct msg* _msg;
  bool _free_on_delete;
};

class AVP::iterator
{
public:
  inline iterator(const AVP& parent_avp) : _avp(find_first_child(parent_avp.avp())) {memset(&_filter_avp_data, 0, sizeof(_filter_avp_data));}
  inline iterator(const AVP& parent_avp, const Dictionary::AVP& child_type) : _filter_avp_data(get_avp_data(child_type.dict())), _avp(find_first_child(parent_avp.avp(), _filter_avp_data)) {}
  inline iterator(const Message& parent_msg) : _avp(find_first_child(parent_msg.msg())) {memset(&_filter_avp_data, 0, sizeof(_filter_avp_data));}
  inline iterator(const Message& parent_msg, const Dictionary::AVP& child_type) : _filter_avp_data(get_avp_data(child_type.dict())), _avp(find_first_child(parent_msg.msg(), _filter_avp_data)) {}
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
  inline static dict_avp_data get_avp_data(struct dict_object* dict)
  {
    struct dict_avp_data avp_data;
    fd_dict_getval(dict, &avp_data);
    return avp_data;
  }
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

AVP::iterator AVP::begin() const {return AVP::iterator(*this);}
AVP::iterator AVP::begin(const Dictionary::AVP& type) const {return AVP::iterator(*this, type);}
AVP::iterator AVP::end() const {return AVP::iterator(NULL);}

AVP::iterator Message::begin() const {return AVP::iterator(*this);}
AVP::iterator Message::begin(const Dictionary::AVP& type) const {return AVP::iterator(*this, type);}
AVP::iterator Message::end() const {return AVP::iterator(NULL);}
};

#endif
