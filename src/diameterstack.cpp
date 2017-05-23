/**
 * @file diameterstack.cpp class implementation wrapping freeDiameter
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "diameterstack.h"
#include "log.h"
#include "sasevent.h"
#include "cpp_common_pd_definitions.h"

using namespace Diameter;

Stack* Stack::INSTANCE = &DEFAULT_INSTANCE;
Stack Stack::DEFAULT_INSTANCE;
struct fd_hook_data_hdl* Stack::_sas_cb_data_hdl = NULL;

Stack::Stack() : _initialized(false),
                 _allow_connections(true),
                 _callback_handler(NULL),
                 _callback_fallback_handler(NULL),
                 _exception_handler(NULL),
                 _comm_monitor(NULL),
                 _realm_counter(NULL),
                 _host_counter(NULL),
                 _peer_count(-1),
                 _connected_peer_count(-1)
{
  pthread_mutex_init(&_peer_counts_lock, NULL);
  pthread_rwlock_init(&_peer_connection_cbs_lock, NULL);
  pthread_rwlock_init(&_rt_out_cbs_lock, NULL);
}

Stack::~Stack()
{
  pthread_mutex_destroy(&_peer_counts_lock);
  pthread_rwlock_destroy(&_peer_connection_cbs_lock);
  pthread_rwlock_destroy(&_rt_out_cbs_lock);
}

void Stack::initialize()
{
  // Initialize if we haven't already done so.  We don't do this in the
  // constructor because we can't throw exceptions on failure.
  if (!_initialized)
  {
    CL_DIAMETER_START.log();
    TRC_STATUS("Initializing Diameter stack");
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
    rc = fd_hook_register(HOOK_MASK(HOOK_DATA_RECEIVED,
                                    HOOK_MESSAGE_LOCAL,
                                    HOOK_MESSAGE_ROUTING_FORWARD,
                                    HOOK_MESSAGE_ROUTING_LOCAL),
                          fd_null_hook_cb, this, NULL, &_null_cb_hdlr);
    if (rc != 0)
    {
      throw Exception("fd_hook_register(fd_null_hook_cb)", rc); // LCOV_EXCL_LINE
    }

    CL_DIAMETER_INIT_CMPL.log();
    if (_sas_cb_data_hdl == NULL)
    {
      rc = fd_hook_data_register(sizeof(struct fd_hook_permsgdata),
                                 NULL,
                                 NULL,
                                 &_sas_cb_data_hdl);
      if (rc != 0)
      {
        throw Exception("fd_hook_data_register", rc); // LCOV_EXCL_LINE
      }
    }
    rc = fd_hook_register(HOOK_MASK(HOOK_MESSAGE_ROUTING_ERROR),
                    fd_error_hook_cb, this, _sas_cb_data_hdl, &_error_cb_hdlr);
    if (rc != 0)
    {
      throw Exception("fd_hook_register(fd_error_hook_cb)", rc); // LCOV_EXCL_LINE
    }
    rc = fd_hook_register(HOOK_MASK(HOOK_MESSAGE_RECEIVED,
                                    HOOK_MESSAGE_SENT),
                          fd_sas_log_diameter_message,
                          this,
                          _sas_cb_data_hdl,
                          &_sas_cb_hdlr);
    if (rc != 0)
    {
      throw Exception("fd_hook_register(fd_sas_log_diameter_message)", rc); // LCOV_EXCL_LINE
    }

    _initialized = true;
  }
}

void Stack::register_peer_hook_hdlr(std::string listener_id,
                                    PeerConnectionCB peer_connection_cb)
{
  pthread_rwlock_wrlock(&_peer_connection_cbs_lock);
  if (_peer_connection_cbs.empty())
  {
    int rc = fd_hook_register(HOOK_MASK(HOOK_PEER_CONNECT_SUCCESS,
                                        HOOK_PEER_CONNECT_FAILED),
                              fd_peer_hook_cb,
                              this,
                              NULL,
                              &_peer_cb_hdlr);
    if (rc != 0)
    {
      throw Exception("fd_hook_register", rc); // LCOV_EXCL_LINE
    }
  }

  _peer_connection_cbs[listener_id] = peer_connection_cb;
  pthread_rwlock_unlock(&_peer_connection_cbs_lock);
}

void Stack::unregister_peer_hook_hdlr(std::string listener_id)
{
  pthread_rwlock_wrlock(&_peer_connection_cbs_lock);
  _peer_connection_cbs.erase(listener_id);
  if (_peer_cb_hdlr && _peer_connection_cbs.empty())
  {
    fd_hook_unregister(_peer_cb_hdlr);
  }
  pthread_rwlock_unlock(&_peer_connection_cbs_lock);
}

void Stack::register_rt_out_cb(std::string listener_id,
                               RtOutCB rt_out_cb)
{
  pthread_rwlock_wrlock(&_rt_out_cbs_lock);
  if (_rt_out_cbs.empty())
  {
    int rc = fd_rt_out_register(fd_rt_out_cb, this, 10, &_rt_out_cb_hdlr);

    if (rc != 0)
    {
      throw Exception("fd_rt_out_register", rc); // LCOV_EXCL_LINE
    }
  }
  _rt_out_cbs[listener_id] = rt_out_cb;
  pthread_rwlock_unlock(&_rt_out_cbs_lock);
}

void Stack::unregister_rt_out_cb(std::string listener_id)
{
  pthread_rwlock_wrlock(&_rt_out_cbs_lock);
  _rt_out_cbs.erase(listener_id);
  if (_rt_out_cb_hdlr && _rt_out_cbs.empty())
  {
    fd_rt_out_unregister(_rt_out_cb_hdlr, NULL);
  }
  pthread_rwlock_unlock(&_rt_out_cbs_lock);
}

void Stack::populate_avp_map()
{
  fd_list* vendor_sentinel;
  fd_dict_getlistof(VENDOR_BY_ID, fd_g_config->cnf_dict, &vendor_sentinel);

  for (fd_list* vend_li = vendor_sentinel->next;
       vend_li != vendor_sentinel;
       vend_li = vend_li->next)
  {
    struct dict_object* vendor_dict = (struct dict_object*)vend_li->o;
    struct dict_vendor_data vendor_data;
    fd_dict_getval(vendor_dict, &vendor_data);

    populate_vendor_map(vendor_data.vendor_name, vendor_dict);
  }

  // Repeat for vendor 0 (which isn't found by fd_dict_getlistof).
  struct dict_object* vendor_dict;
  vendor_id_t vendor_id = 0;
  fd_dict_search(fd_g_config->cnf_dict,
                 DICT_VENDOR,
                 VENDOR_BY_ID,
                 &vendor_id,
                 &vendor_dict,
                 ENOENT);

  populate_vendor_map("", vendor_dict);
}

void Stack::populate_vendor_map(const std::string& vendor_name,
                                struct dict_object* vendor_dict)
{
  fd_list* avp_sentinel;
  fd_dict_getlistof(AVP_BY_NAME,
                    vendor_dict,
                    &avp_sentinel);
  for (fd_list* avp_li = avp_sentinel->next;
       avp_li != avp_sentinel;
       avp_li = avp_li->next)
  {
    struct dict_object * avp_dict = (struct dict_object*)avp_li->o;
    struct dict_avp_data avp_data;
    fd_dict_getval(avp_dict, &avp_data);

    // Add this AVP to this vendor's map entry.
    _avp_map[vendor_name][avp_data.avp_name] = avp_dict;
  }
}

void Stack::fd_null_hook_cb(enum fd_hook_type type,
                            struct msg * msg,
                            struct peer_hdr * peer,
                            void * other,
                            struct fd_hook_permsgdata *pmd,
                            void * user_data)
{
  // Do nothing
}

void Stack::fd_error_hook_cb(enum fd_hook_type type,
                             struct msg * msg,
                             struct peer_hdr* peer,
                             void* other,
                             struct fd_hook_permsgdata* pmd,
                             void* stack_ptr)
{
  ((Diameter::Stack*)stack_ptr)->fd_error_hook_cb(type, msg, peer, other, pmd);
}

void Stack::fd_error_hook_cb(enum fd_hook_type type,
                             struct msg* msg,
                             struct peer_hdr* peer,
                             void *other,
                             struct fd_hook_permsgdata* pmd)
{
  // We don't have access to the real Dictionary object at this point,
  // and we only need access to base AVPs such as Destination-Host, so
  // just keep a generic Dictionary in this function.
  Dictionary dict;

  // This Message object is just for convenient access to some AVPs -
  // it shouldn't free the underlying message on destruction.
  Message msg2(&dict, msg, this);
  msg2.revoke_ownership();

  std::string dest_host, dest_realm;
  if (!msg2.get_destination_host(dest_host))
  {
    dest_host = "unknown";
  };
  if (!msg2.get_destination_realm(dest_realm))
  {
    dest_realm = "unknown";
  };

  TRC_ERROR("Routing error: '%s' for message with "
            "Command-Code %d, Destination-Host %s and Destination-Realm %s",
            (char *)other,
            msg2.command_code(),
            dest_host.c_str(),
            dest_realm.c_str());

  // Increment routing error stats if they're supported
  if ((_realm_counter != NULL) &&
      (strcmp(fd_g_config->cnf_diamrlm, dest_realm.c_str()) != 0))
  {
    _realm_counter->increment();
  }
  else if ((_host_counter != NULL) &&
           (strcmp(fd_g_config->cnf_diamid, dest_host.c_str()) != 0))
  {
    _host_counter->increment();
  }

  // Make a SAS log if either of the following conditions holds:
  //
  // - the number of managed peers is zero (suggesting a configuration or DNS problem).
  // - the number of currently connected peers is zero.
  //
  // FreeDiameter should only call us if the set of connected peers is zero (for one
  // of these reasons), but we check explicitly below to ensure that no SAS
  // log is made if we don't know the reason why.
  //
  // We use counts given to us by upstream applications, if any exist.
  bool no_peers;
  bool no_connected_peers;
  pthread_mutex_lock(&_peer_counts_lock);

  no_peers = (_peer_count == 0);
  no_connected_peers = (_connected_peer_count == 0);

  pthread_mutex_unlock(&_peer_counts_lock);

  if (pmd != NULL)
  {
    if (no_peers)
    {
      SAS::Event event(pmd->trail, SASEvent::DIAMETER_NO_PEERS, 0);
      event.add_var_param((char *)other);
      event.add_var_param(dest_realm);
      SAS::report_event(event);
    }
    else if (no_connected_peers)
    {
      SAS::Event event(pmd->trail, SASEvent::DIAMETER_NO_CONNECTED_PEERS, 0);
      event.add_var_param((char *)other);
      event.add_var_param(dest_host);
      event.add_var_param(dest_realm);
      SAS::report_event(event);
    }
  }
}

void Stack::fd_peer_hook_cb(enum fd_hook_type type,
                            struct msg * msg,
                            struct peer_hdr* peer,
                            void* other,
                            struct fd_hook_permsgdata* pmd,
                            void* stack_ptr)
{
  ((Diameter::Stack*)stack_ptr)->fd_peer_hook_cb(type, peer);
}

void Stack::fd_peer_hook_cb(enum fd_hook_type type,
                            struct peer_hdr* peer)
{
  // Check the type first.  We can't rely on peer being set if it's not the right type.
  if ((type != HOOK_PEER_CONNECT_SUCCESS) &&
      (type != HOOK_PEER_CONNECT_FAILED))
  {
    TRC_WARNING("Unexpected hook type on callback from freeDiameter: %d", type);
  }
  else if (peer == NULL)
  {
    TRC_ERROR("No peer supplied on callback of type %d", type);
  }
  else
  {
    TRC_DEBUG("Callback (type %d) from freeDiameter: %s", type, peer->info.pi_diamid);

    std::string host = peer->info.pi_diamid;

    pthread_rwlock_rdlock(&_peer_connection_cbs_lock);
    for (std::map<std::string, PeerConnectionCB>::const_iterator cb = _peer_connection_cbs.begin();
         cb != _peer_connection_cbs.end();
         ++cb)
    {
      (cb->second)((type == HOOK_PEER_CONNECT_SUCCESS) ? true : false,
                   host,
                   (type == HOOK_PEER_CONNECT_SUCCESS) ? peer->info.runtime.pir_realm : "");
    }
    pthread_rwlock_unlock(&_peer_connection_cbs_lock);
  }
  return;
}

int Stack::fd_rt_out_cb(void* stack_ptr, struct msg** pmsg, struct fd_list* candidates)
{
  ((Diameter::Stack*)stack_ptr)->fd_rt_out_cb(candidates);
  return 0;
}

void Stack::fd_rt_out_cb(struct fd_list* candidates)
{
  TRC_DEBUG("Routing out callback from freeDiameter");
  pthread_rwlock_rdlock(&_rt_out_cbs_lock);
  for (std::map<std::string, RtOutCB>::const_iterator cb = _rt_out_cbs.begin();
       cb != _rt_out_cbs.end();
       ++cb)
  {
    (cb->second)(candidates);
  }
  pthread_rwlock_unlock(&_rt_out_cbs_lock);
}

void Stack::configure(std::string filename,
                      ExceptionHandler* exception_handler,
                      BaseCommunicationMonitor* comm_monitor,
                      SNMP::CounterTable* realm_counter,
                      SNMP::CounterTable* host_counter)
{
  initialize();
  TRC_STATUS("Configuring Diameter stack from file %s", filename.c_str());
  int rc = fd_core_parseconf(filename.c_str());

  if (rc != 0)
  {
    throw Exception("fd_core_parseconf", rc); // LCOV_EXCL_LINE
  }

  rc = fd_define_untrusted_avp_vendors(fd_g_config->cnf_untrusted_avp_vendors);

  if (rc != 0)
  {
    throw Exception("fd_define_untrusted_avp_vendors", rc); // LCOV_EXCL_LINE
  }

  // Configure a peer connection validator. This is calls when processing
  // a CER, and rejects it if the Diameter stack is not accepting connections.
  // This must be done after loading any extensions, as we want this
  // validator to have the first choice at rejecting any connections
  rc = fd_peer_validate_register(allow_connections);

  if (rc != 0)
  {
    throw Exception("fd_peer_validate_register", rc); // LCOV_EXCL_LINE
  }

  populate_avp_map();
  _exception_handler = exception_handler;
  _comm_monitor = comm_monitor;
  _realm_counter = realm_counter;
  _host_counter = host_counter;
}

void Stack::advertize_application(const Dictionary::Application::Type type,
                                  const Dictionary::Application& app)
{
  initialize();
  int rc;

  if (type == Dictionary::Application::AUTH)
  {
    rc = fd_disp_app_support(app.dict(), NULL, 1, 0);
  }
  else
  {
    rc = fd_disp_app_support(app.dict(), NULL, 0, 1);
  }

  if (rc != 0)
  {
    throw Exception("fd_disp_app_support", rc); // LCOV_EXCL_LINE
  }
}

void Stack::advertize_application(const Dictionary::Application::Type type,
                                  const Dictionary::Vendor& vendor,
                                  const Dictionary::Application& app)
{
  initialize();
  int rc;

  if (type == Dictionary::Application::AUTH)
  {
    rc = fd_disp_app_support(app.dict(), vendor.dict(), 1, 0);
  }
  else
  {
    rc = fd_disp_app_support(app.dict(), vendor.dict(), 0, 1);
  }

  if (rc != 0)
  {
    throw Exception("fd_disp_app_support", rc); // LCOV_EXCL_LINE
  }
}

void Stack::register_handler(const Dictionary::Application& app,
                             const Dictionary::Message& msg,
                             HandlerInterface* handler)
{
  // Register a callback for messages from our application with the specified message type.
  // DISP_HOW_CC indicates that we want to match on command code (and allows us to optionally
  // match on application if specified). Use a pointer to our Handler to pass through
  // to our callback function.
  struct disp_when data;
  memset(&data, 0, sizeof(data));
  data.app = app.dict();
  data.command = msg.dict();
  int rc = fd_disp_register(request_callback_fn,
                            DISP_HOW_CC,
                            &data,
                            (void *)handler,
                            &_callback_handler);

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
  int rc = fd_disp_register(fallback_request_callback_fn, DISP_HOW_APPID, &data, NULL, &_callback_fallback_handler);

  if (rc != 0)
  {
    throw Exception("fd_disp_register", rc); //LCOV_EXCL_LINE
  }
}

int Stack::request_callback_fn(struct msg** req,
                               struct avp* avp,
                               struct session* sess,
                               void* handler_param,
                               enum disp_action* act)
{
  HandlerInterface* handler = (HandlerInterface*)handler_param;

  // A SAS trail should have already been allocated in fd_sas_log_diameter_message.
  // Get it if so (or create a new one if not).
  SAS::TrailId trail;
  struct fd_hook_permsgdata* pmd = fd_hook_get_pmd(_sas_cb_data_hdl, *req);
  if (pmd != NULL)
  {
    trail = pmd->trail;
  }
  else
  {
    trail = SAS::new_trail(0);
    TRC_WARNING("No per-message data found - allocated new trail ID: %lu", trail);
  }
  TRC_DEBUG("Invoke diameter request handler on trail %lu", trail);

  // Pass the request to the registered handler.
  CW_TRY
  {
    handler->process_request(req, trail);
  }
  CW_EXCEPT(get_instance()->_exception_handler)
  {
    *req = NULL;
    return EINVAL;
  }
  CW_END

  // The handler will turn the message associated with the task into an
  // answer.
  // Return 0 to indicate no errors with the callback.
  *req = NULL;
  *act = DISP_ACT_CONT;
  return 0;
}

int Stack::fallback_request_callback_fn(struct msg** msg, struct avp* avp, struct session* sess, void* opaque, enum disp_action* act)
{
  // This means we have received a message of an unexpected type.
  TRC_WARNING("Message of unexpected type received");
  return ENOTSUP;
}

void Stack::start()
{
  initialize();
  TRC_STATUS("Starting Diameter stack");
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
    TRC_STATUS("Stopping Diameter stack");
    if (_callback_handler)
    {
      (void)fd_disp_unregister(&_callback_handler, NULL);
    }

    if (_callback_fallback_handler)
    {
      (void)fd_disp_unregister(&_callback_fallback_handler, NULL);
    }

    if (_peer_cb_hdlr)
    {
      pthread_rwlock_rdlock(&_peer_connection_cbs_lock);
      if (!_peer_connection_cbs.empty())
      {
        TRC_WARNING("Diameter Stack is shutting down, but %s still has registered peer connection callback",
                    _peer_connection_cbs.begin()->first.c_str());
      }
      pthread_rwlock_unlock(&_peer_connection_cbs_lock);
    }

    if (_rt_out_cb_hdlr)
    {
      pthread_rwlock_rdlock(&_rt_out_cbs_lock);
      if (!_rt_out_cbs.empty())
      {
        TRC_WARNING("Diameter Stack is shutting down, but %s still has registered message routing callback",
                    _rt_out_cbs.begin()->first.c_str());
      }
      pthread_rwlock_unlock(&_rt_out_cbs_lock);
    }

    if (_error_cb_hdlr)
    {
      fd_hook_unregister(_error_cb_hdlr);
    }

    if (_null_cb_hdlr)
    {
      fd_hook_unregister(_null_cb_hdlr);
    }

    if (_sas_cb_hdlr)
    {
      fd_hook_unregister(_sas_cb_hdlr);
    }

    // freeDiameter does not allow you to unregister data handles. Also it
    // stores them in static data and has a maximum nunber that can be
    // registered, so we must not NULL the handle out either.

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
    TRC_STATUS("Waiting for Diameter stack to stop");
    int rc = fd_core_wait_shutdown_complete();
    if (rc != 0)
    {
      throw Exception("fd_core_wait_shutdown_complete", rc); // LCOV_EXCL_LINE
    }
    fd_log_handler_unregister();
    _initialized = false;
    _peer_count = -1;
    _connected_peer_count = -1;
  }
}

void Stack::close_connections()
{
  if (_allow_connections)
  {
    // Shut down any Diameter connections immediately.
    _allow_connections = false;
    (void)fd_connections_shutdown();
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

void Stack::set_trail_id(struct msg* fd_msg, SAS::TrailId trail)
{
  fd_hook_get_pmd(_sas_cb_data_hdl, fd_msg)->trail = trail;
}

void Stack::send(struct msg* fd_msg, SAS::TrailId trail)
{
  set_trail_id(fd_msg, trail);
  fd_msg_send(&fd_msg, NULL, NULL);
}

void Stack::send(struct msg* fd_msg, Transaction* tsx)
{
  set_trail_id(fd_msg, tsx->trail());
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

  set_trail_id(fd_msg, tsx->trail());
  fd_msg_send_timeout(&fd_msg, Transaction::on_response, tsx, Transaction::on_timeout, &timeout_ts);
}

void Stack::report_tsx_result(int32_t rc)
{
  if (_comm_monitor)
  {
    if (((rc >= 3001) && (rc <= 3010)) ||
        ((rc >= 4002) && (rc <= 4003)) ||
        ((rc >= 5001) && (rc <= 5002)) ||
        ((rc >= 5004) && (rc <= 5005)) ||
        ((rc >= 5007) && (rc <= 5011)) ||
        ((rc >= 5013) && (rc <= 5015)) ||
         (rc == 5017) || (rc == 4181)    )
    {
      _comm_monitor->inform_failure();
    }
    else
    {
      _comm_monitor->inform_success();
    }
  }
}

void Stack::report_tsx_timeout()
{
  if (_comm_monitor)
  {
    _comm_monitor->inform_failure();
  }
}

bool Stack::add(Peer* peer)
{
  // Set up the peer information structure.
  struct peer_info info;
  memset(&info, 0, sizeof(struct peer_info));
  fd_list_init(&info.pi_endpoints, NULL);
  info.pi_diamid = strdup(peer->host().c_str());
  info.pi_diamidlen = peer->host().length();
  if (peer->addr_info_specified())
  {
    info.config.pic_flags.diamid = PI_DIAMID_DYN;
    info.config.pic_port = peer->addr_info().port;
  }
  else
  {
    info.config.pic_flags.persist = PI_PRST_ALWAYS;
  }
  info.config.pic_flags.pro4 = PI_P4_TCP;
  info.config.pic_flags.sec = PI_SEC_NONE;
  if (peer->realm() != "")
  {
    info.config.pic_realm = strdup(peer->realm().c_str());
  }
  if (peer->idle_time() != 0)
  {
    info.config.pic_lft = peer->idle_time();
    info.config.pic_flags.exp = PI_EXP_INACTIVE;
  }

  // Fill in and insert the endpoint.  Note that this needs to be malloc-ed
  // (not new-ed) because it will be free-d by freeDiameter.
  struct fd_endpoint* endpoint = (struct fd_endpoint*)malloc(sizeof(struct fd_endpoint));
  memset(endpoint, 0, sizeof(struct fd_endpoint));
  fd_list_init(&endpoint->chain, &endpoint);
  endpoint->flags = EP_FL_DISC;
  if (peer->addr_info_specified())
  {
    if (peer->addr_info().address.af == AF_INET)
    {
      endpoint->sin.sin_family = AF_INET;
      endpoint->sin.sin_addr.s_addr = peer->addr_info().address.addr.ipv4.s_addr;
      fd_list_insert_before(&info.pi_endpoints, &endpoint->chain);
    }
    else if (peer->addr_info().address.af == AF_INET6)
    {
      endpoint->sin6.sin6_family = AF_INET6;
      memcpy(&endpoint->sin6.sin6_addr,
             &peer->addr_info().address.addr.ipv6,
             sizeof(struct sockaddr_in6));
      fd_list_insert_before(&info.pi_endpoints, &endpoint->chain);
    }
    else
    {
      TRC_ERROR("Unrecognized address family %d - omitting endpoint", peer->addr_info().address.af);
    }
  }

  // Add the peer in freeDiameter.  The second parameter is just a debug string.
  int rc = fd_peer_add(&info, "Diameter::Stack", NULL, NULL);

  free(info.pi_diamid);
  free(info.config.pic_realm);
  while (!FD_IS_LIST_EMPTY(&info.pi_endpoints))
  {
    struct fd_list * li = info.pi_endpoints.next;
    fd_list_unlink(li);
    free(li);
  }

  if (rc != 0)
  {
    TRC_INFO("Peer already exists");
    return false;
  }

  return true;
}

void Stack::remove(Peer* peer)
{
  std::string host = peer->host();
  fd_peer_remove((char*)host.c_str(), host.length());
}

void Stack::peer_count(int count, int connected_count)
{
  pthread_mutex_lock(&_peer_counts_lock);
  _peer_count = count;
  _connected_peer_count = connected_count;

  if (_peer_count == 0)
  {
    TRC_ERROR("No Diameter peers have been found");
  }
  else if (_connected_peer_count == 0)
  {
    TRC_WARNING("No connected Diameter peers have been found");
  }

  pthread_mutex_unlock(&_peer_counts_lock);
}

void Stack::fd_sas_log_diameter_message(enum fd_hook_type type,
                                        struct msg * msg,
                                        struct peer_hdr * peer,
                                        void * other,
                                        struct fd_hook_permsgdata *pmd,
                                        void * stack_ptr)
{
  struct msg_hdr * hdr;
  SAS::TrailId trail;
  Stack* stack = (Stack*)stack_ptr;

  fd_msg_hdr(msg, &hdr);

  // Don't log connection management requests.
  if ((hdr->msg_code == CC_CAPABILITIES_EXCHANGE) ||
      (hdr->msg_code == CC_DEVICE_WATCHDOG) ||
      (hdr->msg_code == CC_DISCONNECT_PEER))
  {
    return;
  }

  // Get the trail ID we should be logging on.
  if (type == HOOK_MESSAGE_RECEIVED)
  {
    TRC_DEBUG("Processing a received diameter message");

    if (hdr->msg_flags & CMD_FLAG_REQUEST)
    {
      // Received request. Allocate a new trail and store it in PMD.
      trail = SAS::new_trail(0);
      TRC_DEBUG("Allocated new trail ID: %lu", trail);

      // If there is a permsgdata, store the trail.  This might not be true
      // in some error scenarios, in which case we'll log, but might not
      // correlate correctly.
      if (pmd != NULL)
      {
        pmd->trail = trail;
      }
    }
    else
    {
      // Received answer. Get the trail from the request.
      struct fd_hook_permsgdata *req_pmd = fd_hook_get_request_pmd(stack->_sas_cb_data_hdl, msg);
      if (req_pmd != NULL)
      {
        trail = req_pmd->trail;
        TRC_DEBUG("Got existing trail ID: %lu", trail);
      }
      else
      {
        trail = SAS::new_trail(0);
        TRC_WARNING("No per-message data found - allocated new trail ID: %lu", trail);
      }
    }
  }
  else if (type == HOOK_MESSAGE_SENT)
  {
    // Sent request / answer. Use the trail ID that the diameter stack set on
    // the message (if available) or create a new one if not.
    TRC_DEBUG("Processing a sent diameter message");
    if (pmd != NULL)
    {
      trail = pmd->trail;
      TRC_DEBUG("Got existing trail ID: %lu", trail);
    }
    else
    {
      trail = SAS::new_trail(0);
      TRC_WARNING("No per-message data found - allocated new trail ID: %lu", trail);
    }
  }
  else
  {
    TRC_ERROR("Unexpected freeDiameter hook type: %d", type);
    return;
  }

  // Construct an event and add the remote IP/port, local IP/port, and message
  // data.
  char ip[64];
  unsigned short port;

  const int event_type = ((type == HOOK_MESSAGE_RECEIVED) ?
                          SASEvent::DIAMETER_RX :
                          SASEvent::DIAMETER_TX);
  SAS::Event event(trail, event_type, 0);

  if (fd_peer_cnx_remote_ip_port(peer, ip, sizeof(ip), &port) == 0)
  {
    event.add_var_param(ip);
    event.add_static_param(port);
  }
  else
  {
    event.add_var_param("unknown");
    event.add_static_param(0);
  }

  if (fd_peer_cnx_local_ip_port(peer, ip, sizeof(ip), &port) == 0)
  {
    event.add_var_param(ip);
    event.add_static_param(port);
  }
  else
  {
    event.add_var_param("unknown");
    event.add_static_param(0);
  }

  struct fd_cnx_rcvdata* data = (struct fd_cnx_rcvdata*)other;
  event.add_compressed_param(data->length, data->buffer, &SASEvent::PROFILE_LZ4);

  SAS::report_event(event);

  // Now construct a correlating marker based on the diameter session ID.
  struct session* sess;
  int dummy_is_new;

  if ((fd_msg_sess_get(fd_g_config->cnf_dict, msg, &sess, &dummy_is_new) == 0) &&
      (sess != NULL))
  {
    os0_t session_id;
    size_t session_id_len;

    if (fd_sess_getsid(sess, &session_id, &session_id_len) == 0)
    {
      TRC_DEBUG("Raising correlating marker with diameter session ID = %.*s",
                session_id_len, session_id);
      SAS::Marker corr(trail, MARKED_ID_GENERIC_CORRELATOR, 0);
      corr.add_static_param((uint32_t)UniquenessScopes::DIAMETER_SID_RFC6733);
      corr.add_var_param(session_id_len, session_id);

      // The marker should be trace-scoped, and should not reactivate any trail
      // groups (this means that diameter flows occurring after the end of the
      // call will not delay it from appearing in SAS).
      SAS::report_marker(corr, SAS::Marker::Scope::Trace, false);
    }
  }
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
  return find("", avp);
}

struct dict_object* Dictionary::AVP::find(const std::string vendor, const std::string avp)
{
  Stack* stack = Stack::get_instance();
  std::unordered_map<std::string, std::unordered_map<std::string, struct dict_object*>>::iterator vendor_entry;
  vendor_entry = stack->avp_map().find(vendor);
  if (vendor_entry != stack->avp_map().end())
  {
    // Found the vendor, now find the AVP
    std::unordered_map<std::string, struct dict_object*>::iterator avp_entry;
    avp_entry = vendor_entry->second.find(avp);
    if (avp_entry != vendor_entry->second.end())
    {
      return avp_entry->second;
    }
  }
  return NULL;
}

struct dict_object* Dictionary::AVP::find(const std::vector<std::string>& vendors, const std::string avp)
{
  struct dict_object* dict = NULL;
  for (std::vector<std::string>::const_iterator vendor = vendors.begin();
       vendor != vendors.end();
       ++vendor)
  {
    dict = find(*vendor, avp);
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
  AUTH_APPLICATION_ID("Auth-Application-Id"),
  ACCT_APPLICATION_ID("Acct-Application-Id"),
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

  TRC_VERBOSE("Got Diameter response of type %u - calling callback on transaction %p",
              msg.command_code(), tsx);

  int32_t rc;
  msg.result_code(rc);
  stack->report_tsx_result(rc);

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

  TRC_VERBOSE("Diameter request of type %u timed out - calling callback on transaction %p",
              msg.command_code(), tsx);

  // log the timeout to SAS.
  {
    SAS::Event event(tsx->trail(), SASEvent::DIAMETER_TIMEOUT, 0);
    uint8_t* buf = NULL;
    size_t len;

    if (fd_msg_bufferize(*req, &buf, &len) == 0)
    {
      event.add_compressed_param(len, buf, &SASEvent::PROFILE_LZ4);
    }
    else
    {
      event.add_compressed_param("unknown", &SASEvent::PROFILE_LZ4);
    }

    SAS::report_event(event);
    free(buf); buf = NULL;
  }

  stack->report_tsx_timeout();

  tsx->stop_timer();
  tsx->on_timeout();
  delete tsx;
  // Null out the message so that freeDiameter doesn't try to send it on.
  *req = NULL;
}

// Given an AVP type, search an AVP for a child of this type. If one exists, return true
// and set str to the string value of the child AVP. Otherwise return false.
bool AVP::get_str_from_avp(const Dictionary::AVP& type, std::string& str) const
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

// Given an AVP type, search an AVP for a child of this type. If one exists, return true
// and set i32 to the integer value of the child AVP. Otherwise return false.
bool AVP::get_i32_from_avp(const Dictionary::AVP& type, int32_t& i32) const
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

bool AVP::get_u32_from_avp(const Dictionary::AVP& type, uint32_t& u32) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    u32 = avps->val_i32();
    return true;
  }
  else
  {
    return false;
  }
}

AVP& AVP::val_json(const std::vector<std::string>& vendors,
                   const Diameter::Dictionary::AVP& dict,
                   const rapidjson::Value& value)
{
  switch (value.GetType())
  {
    case rapidjson::kFalseType:
    case rapidjson::kTrueType:
      TRC_ERROR("Invalid format (true/false) in JSON block (%d), ignoring",
                avp_hdr()->avp_code);
      break;
    case rapidjson::kNullType:
      TRC_ERROR("Invalid NULL in JSON block, ignoring");
      break;
    case rapidjson::kArrayType:
      TRC_ERROR("Cannot store multiple values in one ACR, ignoring");
      break;
    case rapidjson::kStringType:
      val_str(std::string(value.GetString(), value.GetStringLength()));
      break;
    case rapidjson::kNumberType:
      // Parse the value out of the JSON as the appropriate type
      // for for AVP.
      switch (dict.base_type())
      {
      case AVP_TYPE_GROUPED:
        TRC_ERROR("Cannot store integer in grouped AVP, ignoring");
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
        TRC_ERROR("Floating point AVPs are not supportedi, ignoring");
        break;
      default:
        TRC_ERROR("Unexpected AVP type, ignoring"); // LCOV_EXCL_LINE
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
            TRC_ERROR("Invalid format (true/false) in JSON block, ignoring");
            continue;
          case rapidjson::kNullType:
            TRC_ERROR("Invalid NULL in JSON block, ignoring");
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
          TRC_WARNING("AVP %s not recognised, ignoring", it->name.GetString());
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

// Given an AVP type, search a Diameter message for an AVP of this type. If one exists,
// return true and set u32 to the integer value of this AVP. Otherwise return false.
bool Message::get_u32_from_avp(const Dictionary::AVP& type, uint32_t& u32) const
{
  AVP::iterator avps = begin(type);
  if (avps != end())
  {
    u32 = avps->val_i32();
    return true;
  }
  else
  {
    return false;
  }
}

// Get the experimental result code and it's vendor from the
// EXPERIMENTAL_RESULT_CODE AVP and VENDOR_ID AVP
// of a Diameter message if it is present. These AVPs are inside the
// EXPERIMENTAL_RESULT AVP.
bool Message::experimental_result(int32_t& experimental_result_code, uint32_t& vendor_id) const
{
  bool found_experimental_result = false;
  AVP::iterator avps = begin(dict()->EXPERIMENTAL_RESULT);
  if (avps != end())
  {
    AVP::iterator code = avps->begin(dict()->EXPERIMENTAL_RESULT_CODE);
    AVP::iterator vendor = avps->begin(dict()->VENDOR_ID);
    if (code != avps->end() && vendor != avps->end())
    {
      experimental_result_code = code->val_i32();
      vendor_id = vendor->val_u32();
      found_experimental_result = true;
      TRC_DEBUG("Got Experimental-Result-Code %d for Vendor %d",
                experimental_result_code, vendor_id);
    }
  }
  return found_experimental_result;
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
      TRC_DEBUG("Got Vendor-Id %d", vendor_id);
    }
  }
  return vendor_id;
}

Message& Message::add_session_id(const std::string& session_id)
{
  Diameter::AVP session_id_avp(dict()->SESSION_ID);
  session_id_avp.val_str(session_id);
  add(session_id_avp);
  return *this;
}

const std::string Message::get_session_id()
{
  Diameter::AVP::iterator avps = begin((dict())->SESSION_ID);

  if (avps != end())
  {
    return avps->val_str();
  }
  else
  {
    TRC_ERROR("No Session-ID found in request");
    throw Diameter::AVPException("Session-ID");
  }
}

void Message::send(SAS::TrailId trail)
{
  TRC_VERBOSE("Sending Diameter message of type %u", command_code());
  revoke_ownership();

  _stack->send(_fd_msg, trail);
}

void Message::send(Transaction* tsx)
{
  TRC_VERBOSE("Sending Diameter message of type %u on transaction %p", command_code(), tsx);
  tsx->start_timer();
  revoke_ownership();

  _stack->send(_fd_msg, tsx);
}

void Message::send(Transaction* tsx, unsigned int timeout_ms)
{
  TRC_VERBOSE("Sending Diameter message of type %u on transaction %p with timeout %u",
              command_code(), tsx, timeout_ms);
  tsx->start_timer();
  revoke_ownership();

  _stack->send(_fd_msg, tsx, timeout_ms);
}
