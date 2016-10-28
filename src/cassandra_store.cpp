/**
 * @file cassandra_store.cpp Implementation of a generic cassandra backed
 * store.
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

#include <boost/format.hpp>
#include <time.h>

#include "cassandra_store.h"
#include "sasevent.h"
#include "sas.h"

using namespace apache::thrift;
using namespace apache::thrift::transport;
using namespace apache::thrift::protocol;
using namespace org::apache::cassandra;

namespace CassandraStore
{
// The thrift get_slice function requires the user to specify a maximum number
// of rows to return. We don't want a limit, but thrift does not allow this, so
// set the limit very high instead.
const int32_t GET_SLICE_MAX_COLUMNS = 1000000;

// No-op operation class
class NoOperation : public Operation
{
public:
  bool perform(Client* client, SAS::TrailId trail)
  {
    return true;
  }
};

//
// Client methods
//

// LCOV_EXCL_START real clients are not tested in UT.
RealThriftClient::RealThriftClient(boost::shared_ptr<TProtocol> prot,
                                   boost::shared_ptr<TFramedTransport> transport) :
  _cass_client(prot),
  _transport(transport),
  _connected(false)
{
}

RealThriftClient::~RealThriftClient()
{
  _transport->close();
}

bool RealThriftClient::is_connected()
{
  return _connected;
}

void RealThriftClient::connect()
{
  _transport->open();
  _connected = true;
}

void RealThriftClient::set_keyspace(const std::string& keyspace)
{
  _cass_client.set_keyspace(keyspace);
}

void RealThriftClient::batch_mutate(const std::map<std::string, std::map<std::string, std::vector<cass::Mutation> > >& mutation_map,
                                    const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.batch_mutate(mutation_map, consistency_level);
}

void RealThriftClient::get_slice(std::vector<cass::ColumnOrSuperColumn>& _return,
                                 const std::string& key,
                                 const cass::ColumnParent& column_parent,
                                 const cass::SlicePredicate& predicate,
                                 const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.get_slice(_return, key, column_parent, predicate, consistency_level);
}

void RealThriftClient::multiget_slice(std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& _return,
                                      const std::vector<std::string>& keys,
                                      const cass::ColumnParent& column_parent,
                                      const cass::SlicePredicate& predicate,
                                      const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.multiget_slice(_return, keys, column_parent, predicate, consistency_level);
}

void RealThriftClient::remove(const std::string& key,
                              const cass::ColumnPath& column_path,
                              const int64_t timestamp,
                              const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.remove(key, column_path, timestamp, consistency_level);
}

void RealThriftClient::get_range_slices(std::vector<KeySlice> & _return,
                                        const ColumnParent& column_parent,
                                        const SlicePredicate& predicate,
                                        const KeyRange& range,
                                        const ConsistencyLevel::type consistency_level)
{
  _cass_client.get_range_slices(_return, column_parent, predicate, range, consistency_level);
}
// LCOV_EXCL_STOP


//
// Store methods
//

int64_t Store::generate_timestamp()
{
  // Return the current time in microseconds.
  timespec clock_time;
  int64_t timestamp;

  clock_gettime(CLOCK_REALTIME, &clock_time);
  timestamp = clock_time.tv_sec;
  timestamp *= 1000000;
  timestamp += (clock_time.tv_nsec / 1000);

  TRC_DEBUG("Generated Cassandra timestamp %llu", timestamp);
  return timestamp;
}


Store::Store(const std::string& keyspace) :
  _keyspace(keyspace),
  _cass_hostname(""),
  _cass_port(0),
  _num_threads(0),
  _max_queue(0),
  _thread_pool(NULL),
  _comm_monitor(NULL),
  _conn_pool(new CassandraConnectionPool())
{
}

void Store::configure_connection(std::string cass_hostname,
                                 uint16_t cass_port,
                                 BaseCommunicationMonitor* comm_monitor,
                                 CassandraResolver* resolver)
{
  TRC_STATUS("Configuring store connection");
  TRC_STATUS("  Hostname:  %s", cass_hostname.c_str());
  TRC_STATUS("  Port:      %u", cass_port);
  _cass_hostname = cass_hostname;
  _cass_port = cass_port;
  _comm_monitor = comm_monitor;
  _resolver = resolver;
}


ResultCode Store::connection_test()
{
  TRC_DEBUG("Testing cassandra connection");

  ResultCode rc = UNKNOWN_ERROR;
  std::string cass_error_text = "";

  // Check that we can connect to cassandra by getting a client. This logs in
  // and switches to the specified keyspace, so is a good test of whether
  // cassandra is working properly.
  NoOperation no_op;
  perform_op(&no_op, 0, rc, cass_error_text);
  return rc;
}


void Store::configure_workers(ExceptionHandler* exception_handler,
                              unsigned int num_threads,
                              unsigned int max_queue)
{
  TRC_STATUS("Configuring store worker pool");
  TRC_STATUS("  Threads:   %u", num_threads);
  TRC_STATUS("  Max Queue: %u", max_queue);
  _exception_handler = exception_handler;
  _num_threads = num_threads;
  _max_queue = max_queue;
}


ResultCode Store::start()
{
  ResultCode rc = OK;

  // Start the store.  We don't check for connectivity to Cassandra at this
  // point as some store users want the store to load even when Cassandra has
  // failed (it will recover later).  If a store user cares about the status
  // of Cassandra it should use the test() method.
  TRC_STATUS("Starting store");

  // Start the thread pool.
  if (_num_threads > 0)
  {
    _thread_pool = new Pool(this,
                            _num_threads,
                            _exception_handler,
                            _max_queue);

    if (!_thread_pool->start())
    {
      rc = RESOURCE_ERROR; // LCOV_EXCL_LINE
    }
  }

  return rc;
}


void Store::stop()
{
  TRC_STATUS("Stopping store");
  if (_thread_pool != NULL)
  {
    _thread_pool->stop();
  }
}


void Store::wait_stopped()
{
  TRC_STATUS("Waiting for store to stop");
  if (_thread_pool != NULL)
  {
    _thread_pool->join();

    delete _thread_pool; _thread_pool = NULL;
  }
}


Store::~Store()
{
  if (_thread_pool != NULL)
  {
    // It is only safe to destroy the store once the thread pool has been deleted
    // (as the pool stores a pointer to the store). Make sure this is the case.
    stop();
    wait_stopped();
  }

  delete _conn_pool; _conn_pool = NULL;
}


bool Store::perform_op(Operation* op,
                       SAS::TrailId trail,
                       ResultCode& cass_result,
                       std::string& cass_error_text)
{
  bool success = false;
  bool retry = true;
  int attempt_count = 0;

  // Resolve the host
  BaseAddrIterator* target_it = _resolver->resolve_iter(_cass_hostname,
                                                        _cass_port,
                                                        trail);
  AddrInfo target;

  // Iterate over targets until either we succeed in connecting, run out of
  // targets or hit the maximum (2).
  // If there is only one target, try it twice.
  while (retry &&
         (attempt_count <= 2) &&
         (target_it->next(target) || (attempt_count == 1)))
  {
    cass_result = OK;
    attempt_count++;

    // Only retry on connection errors
    retry = false;

    // Get a client to execute the operation.
    ConnectionHandle<Client*> conn_handle = _conn_pool->get_connection(target);

    // Call perform() to actually do the business logic of the request.  Catch
    // exceptions and turn them into return codes and error text.
    try
    {
      // Ensure the client is connected and perform the operation
      Client* client = conn_handle.get_connection();

      if (!client->is_connected())
      {
        TRC_DEBUG("Connecting to %s", target.to_string().c_str());
        client->connect();
        client->set_keyspace(_keyspace);
      }

      success = op->perform(client, trail);
    }
    catch(TTransportException& te)
    {
      cass_result = CONNECTION_ERROR;
      cass_error_text = (boost::format("Exception: %s [%d]")
                         % te.what() % te.getType()).str();

      // SAS log the connection error.
      SAS::Event event(trail, SASEvent::CASS_CONNECT_FAIL, 0);
      event.add_var_param(cass_error_text);
      SAS::report_event(event);

      // Tell the pool not to reuse this connection
      conn_handle.set_return_to_pool(false);

      TRC_DEBUG("Connection error");
      retry = true;
    }
    catch(InvalidRequestException& ire)
    {
      cass_result = INVALID_REQUEST;
      cass_error_text = (boost::format("Exception: %s [%s]")
                         % ire.what() % ire.why.c_str()).str();
    }
    catch(NotFoundException& nfe)
    {
      cass_result = NOT_FOUND;
      cass_error_text = (boost::format("Exception: %s")
                         % nfe.what()).str();
    }
    catch(RowNotFoundException& nre)
    {
      cass_result = NOT_FOUND;
      cass_error_text = (boost::format("Row %s not present in column_family %s")
                         % nre.key % nre.column_family).str();
    }
    catch(UnavailableException& ue)
    {
      cass_result = UNAVAILABLE;
      cass_error_text = (boost::format("Exception: %s")
                         % ue.what()).str();
    }
    catch(...)
    {
      cass_result = UNKNOWN_ERROR;
      cass_error_text = "Unknown error";
    }

    // Since we only retry on connection errors, the value of retry is used to
    // determine whether we connected to the target successfully
    if (retry)
    {
      _resolver->blacklist(target);
    }
    else
    {
      _resolver->success(target);
    }
  }
  return success;
}


bool Store::do_sync(Operation* op, SAS::TrailId trail)
{
  ResultCode cass_result = UNKNOWN_ERROR;
  std::string cass_error_text = "";

  // perform_op() does the actual work
  bool success  = perform_op(op, trail, cass_result, cass_error_text);

  if (cass_result == OK)
  {
    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else
  {
    if (_comm_monitor)
    {
      if (cass_result == CONNECTION_ERROR)
      {
        _comm_monitor->inform_failure();
      }
      else
      {
        _comm_monitor->inform_success();
      }
    }

    if (cass_result == NOT_FOUND)
    {
      // We expect to get "not found" errors during normal operation
      // (e.g. invalid usernames) so log it at a much lower level.
      TRC_DEBUG("Cassandra request failed: rc=%d, %s",
                cass_result, cass_error_text.c_str());
    }
    else
    {
      TRC_ERROR("Cassandra request failed: rc=%d, %s",
                cass_result, cass_error_text.c_str());

    }

    op->unhandled_exception(cass_result, cass_error_text, trail);
  }

  return success;
}


void Store::do_async(Operation*& op, Transaction*& trx)
{
  if (_thread_pool == NULL)
  {
    TRC_ERROR("Can't process async operation as no thread pool has been configured");
    assert(!"Can't process async operation as no thread pool has been configured");
  }

  std::pair<Operation*, Transaction*> params(op, trx);
  _thread_pool->add_work(params);

  // The caller no longer owns the operation or transaction, so null them out.
  op = NULL;
  trx = NULL;
}


//
// Pool methods
//

Store::Pool::Pool(Store* store,
                  unsigned int num_threads,
                  ExceptionHandler* exception_handler,
                  unsigned int max_queue) :
  ThreadPool<std::pair<Operation*, Transaction*> >(num_threads,
                                                   exception_handler,
                                                   exception_callback,
                                                   max_queue),
  _store(store)
{}


Store::Pool::~Pool() {}


void Store::Pool::process_work(std::pair<Operation*, Transaction*>& params)
{
  bool success = false;

  // Extract the operation and transaction.
  Operation* op = params.first;
  Transaction* trx = params.second;

  // Run the operation.  Catch all exceptions to stop an error from killing the
  // worker thread.
  try
  {
    trx->start_timer();
    success = _store->do_sync(op, trx->trail);
  }
  // LCOV_EXCL_START Transaction catches all exceptions so the thread pool
  // fallback code is never triggered.
  catch(...)
  {
    TRC_ERROR("Unhandled exception when processing cassandra request");
  }
  trx->stop_timer();
  // LCOV_EXCL_STOP

  // Call the application back via the transaction.
  if (success)
  {
    trx->on_success(op);
  }
  else
  {
    trx->on_failure(op);
  }

  // We own the transaction and operation so have to free them.
  delete trx; trx = NULL;
  delete op; op = NULL;
}


//
// Operation methods.
//

Operation::Operation() : _cass_status(OK), _cass_error_text() {}

ResultCode Operation::get_result_code()
{
  return _cass_status;
}


std::string Operation::get_error_text()
{
  return _cass_error_text;
}


void Operation::unhandled_exception(ResultCode rc,
                                    std::string& description,
                                    SAS::TrailId trail)
{
  _cass_status = rc;
  _cass_error_text = description;
}


void Client::
put_columns(const std::string& column_family,
            const std::vector<std::string>& keys,
            const std::map<std::string, std::string>& columns,
            int64_t timestamp,
            int32_t ttl,
            cass::ConsistencyLevel::type consistency_level)
{
  // Vector of mutations (one per column being modified).
  std::vector<Mutation> mutations;

  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  TRC_DEBUG("Constructing cassandra put request with timestamp %lld and per-column TTLs", timestamp);
  for (std::map<std::string, std::string>::const_iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    Mutation mutation;
    Column* column = &mutation.column_or_supercolumn.column;

    column->name = it->first;
    column->value = it->second;
    TRC_DEBUG("  %s => %s (TTL %d)", column->name.c_str(), column->value.c_str(), ttl);
    column->__isset.value = true;
    column->timestamp = timestamp;
    column->__isset.timestamp = true;

    // A ttl of 0 => no expiry.
    if (ttl > 0)
    {
      column->ttl = ttl;
      column->__isset.ttl = true;
    }

    mutation.column_or_supercolumn.__isset.column = true;
    mutation.__isset.column_or_supercolumn = true;
    mutations.push_back(mutation);
  }

  // Update the mutation map.
  for (std::vector<std::string>::const_iterator it = keys.begin();
       it != keys.end();
       ++it)
  {
    mutmap[*it][column_family] = mutations;
  }

  // Execute the database operation.
  TRC_DEBUG("Executing put request operation");
  batch_mutate(mutmap, consistency_level);
}


void Client::
put_columns(const std::vector<RowColumns>& to_put,
            int64_t timestamp,
            int32_t ttl)
{
  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  TRC_DEBUG("Constructing cassandra put request with timestamp %lld and per-column TTLs", timestamp);
  for (std::vector<RowColumns>::const_iterator it = to_put.begin();
       it != to_put.end();
       ++it)
  {
    // Vector of mutations (one per column being modified).
    std::vector<Mutation> mutations;

    for (std::map<std::string, std::string>::const_iterator col = it->columns.begin();
         col != it->columns.end();
         ++col)
    {
      Mutation mutation;
      Column* column = &mutation.column_or_supercolumn.column;

      column->name = col->first;
      column->value = col->second;
      TRC_DEBUG("  %s => %s (TTL %d)", column->name.c_str(), column->value.c_str(), ttl);
      column->__isset.value = true;
      column->timestamp = timestamp;
      column->__isset.timestamp = true;

      // A ttl of 0 => no expiry.
      if (ttl > 0)
      {
        column->ttl = ttl;
        column->__isset.ttl = true;
      }

      mutation.column_or_supercolumn.__isset.column = true;
      mutation.__isset.column_or_supercolumn = true;
      mutations.push_back(mutation);
    }

    mutmap[it->key][it->cf] = mutations;
  }

  // Execute the database operation.
  TRC_DEBUG("Executing put request operation");
  batch_mutate(mutmap, ConsistencyLevel::ONE);
}

// A map from quorum consistency levels to their corresponding SAS enum value.
// Any changes made to this must be consistently applied to the sas resource
// bundle.
enum class Quorum_Consistency_Levels
{
  LOCAL_QUORUM = ::cass::ConsistencyLevel::LOCAL_QUORUM,
  QUORUM = ::cass::ConsistencyLevel::QUORUM,
  TWO = ::cass::ConsistencyLevel::TWO
};

// Macro to turn an underlying (non-HA) get method into an HA one.
//
// This macro takes the following arguments:
// -  The name of the underlying get method to call.
// -  The arguments for the underlying get method.
//
// It works as follows:
// -  Call the underlying method with a consistency level of TWO.
//    If successful, this indicates that we've managed to find multiple nodes
//    with the same data, and so we can trust the response (if we're wrong, it
//    will be because at least two local nodes failed or were down simultaneously
//    which we can consider a non-mainline failure case for which some level of
//    service impact is acceptable).
// -  If this fails with UnavailableException, perform a ONE read.  In
//    this case at most one of the replicas for the data is still up, so we
//    can't do any better.
//
#define HA(METHOD, TRAIL_ID, ...)                                            \
        try                                                                  \
        {                                                                    \
          METHOD(__VA_ARGS__, ConsistencyLevel::TWO);                        \
        }                                                                    \
        catch(UnavailableException& ue)                                      \
        {                                                                    \
          TRC_DEBUG("Failed TWO read for %s. Try ONE", #METHOD);             \
          int event_id = SASEvent::QUORUM_FAILURE;                           \
          SAS::Event event(TRAIL_ID, event_id, 0);                           \
          event.add_static_param(                                            \
            static_cast<uint32_t>(Quorum_Consistency_Levels::TWO));          \
          SAS::report_event(event);                                          \
          METHOD(__VA_ARGS__, ConsistencyLevel::ONE);                        \
        }                                                                    \
        catch(TimedOutException& te)                                         \
        {                                                                    \
          TRC_DEBUG("Failed TWO read for %s. Try ONE", #METHOD);             \
          int event_id = SASEvent::QUORUM_FAILURE;                           \
          SAS::Event event(TRAIL_ID, event_id, 1);                           \
          event.add_static_param(                                            \
            static_cast<uint32_t>(Quorum_Consistency_Levels::TWO));          \
          SAS::report_event(event);                                          \
          METHOD(__VA_ARGS__, ConsistencyLevel::ONE);                        \
        }


void Client::
ha_get_columns(const std::string& column_family,
               const std::string& key,
               const std::vector<std::string>& names,
               std::vector<ColumnOrSuperColumn>& columns,
               SAS::TrailId trail)
{
  HA(get_columns, trail, column_family, key, names, columns);
}


void Client::
ha_get_columns_with_prefix(const std::string& column_family,
                           const std::string& key,
                           const std::string& prefix,
                           std::vector<ColumnOrSuperColumn>& columns,
                           SAS::TrailId trail)
{
  HA(get_columns_with_prefix, trail, column_family, key, prefix, columns);
}


void Client::
ha_multiget_columns_with_prefix(const std::string& column_family,
                                const std::vector<std::string>& keys,
                                const std::string& prefix,
                                std::map<std::string, std::vector<ColumnOrSuperColumn> >& columns,
                                SAS::TrailId trail)
{
  HA(multiget_columns_with_prefix, trail, column_family, keys, prefix, columns);
}

void Client::
ha_get_all_columns(const std::string& column_family,
                   const std::string& key,
                   std::vector<ColumnOrSuperColumn>& columns,
                   SAS::TrailId trail)
{
  HA(get_row, trail, column_family, key, columns);
}


void Client::
get_columns(const std::string& column_family,
            const std::string& key,
            const std::vector<std::string>& names,
            std::vector<ColumnOrSuperColumn>& columns,
            ConsistencyLevel::type consistency_level)
{
  // Get only the specified column names.
  SlicePredicate sp;
  sp.column_names = names;
  sp.__isset.column_names = true;

  issue_get_for_key(column_family, key, sp, columns, consistency_level);
}


void Client::
get_columns_with_prefix(const std::string& column_family,
                        const std::string& key,
                        const std::string& prefix,
                        std::vector<ColumnOrSuperColumn>& columns,
                        ConsistencyLevel::type consistency_level)
{
  // This slice range gets all columns with the specified prefix.
  SliceRange sr;
  sr.start = prefix;
  // Increment the last character of the "finish" field.
  sr.finish = prefix;
  *sr.finish.rbegin() = (*sr.finish.rbegin() + 1);
  sr.count = GET_SLICE_MAX_COLUMNS;

  SlicePredicate sp;
  sp.slice_range = sr;
  sp.__isset.slice_range = true;

  issue_get_for_key(column_family, key, sp, columns, consistency_level);

  // Remove the prefix from the returned column names.
  for (std::vector<ColumnOrSuperColumn>::iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    it->column.name = it->column.name.substr(prefix.length());
  }
}


void Client::
multiget_columns_with_prefix(const std::string& column_family,
                             const std::vector<std::string>& keys,
                             const std::string& prefix,
                             std::map<std::string, std::vector<ColumnOrSuperColumn> >& columns,
                             ConsistencyLevel::type consistency_level)
{
  // This slice range gets all columns with the specified prefix.
  SliceRange sr;
  sr.start = prefix;
  // Increment the last character of the "finish" field.
  sr.finish = prefix;
  *sr.finish.rbegin() = (*sr.finish.rbegin() + 1);
  sr.count = GET_SLICE_MAX_COLUMNS;

  SlicePredicate sp;
  sp.slice_range = sr;
  sp.__isset.slice_range = true;

  issue_multiget_for_key(column_family, keys, sp, columns, consistency_level);

  // Remove the prefix from the returned column names.
  for (std::map<std::string, std::vector<ColumnOrSuperColumn> >::iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    for (std::vector<ColumnOrSuperColumn>::iterator it2 = it->second.begin();
         it2 != it->second.end();
         ++it2)
    {
      it2->column.name = it2->column.name.substr(prefix.length());
    }
  }
}


void Client::
get_row(const std::string& column_family,
        const std::string& key,
        std::vector<ColumnOrSuperColumn>& columns,
        ConsistencyLevel::type consistency_level)
{
  // This slice range gets all columns in the row
  SliceRange sr;
  sr.start = "";
  sr.finish = "";
  sr.count = GET_SLICE_MAX_COLUMNS;

  SlicePredicate sp;
  sp.slice_range = sr;
  sp.__isset.slice_range = true;

  issue_get_for_key(column_family, key, sp, columns, consistency_level);
}


void Client::
issue_get_for_key(const std::string& column_family,
                  const std::string& key,
                  const SlicePredicate& predicate,
                  std::vector<ColumnOrSuperColumn>& columns,
                  ConsistencyLevel::type consistency_level)
{
  ColumnParent cparent;
  cparent.column_family = column_family;

  get_slice(columns, key, cparent, predicate, consistency_level);

  if (columns.size() == 0)
  {
    RowNotFoundException row_not_found_ex(column_family, key);
    throw row_not_found_ex;
  }
}


void Client::
issue_multiget_for_key(const std::string& column_family,
                       const std::vector<std::string>& keys,
                       const SlicePredicate& predicate,
                       std::map<std::string, std::vector<ColumnOrSuperColumn> >& columns,
                       ConsistencyLevel::type consistency_level)
{
  ColumnParent cparent;
  cparent.column_family = column_family;

  multiget_slice(columns, keys, cparent, predicate, consistency_level);

  if (columns.size() == 0)
  {
    RowNotFoundException row_not_found_ex(column_family, keys.front());
    throw row_not_found_ex;
  }
}


void Client::
delete_row(const std::string& column_family,
           const std::string& key,
           int64_t timestamp)
{
  ColumnPath cp;
  cp.column_family = column_family;

  TRC_DEBUG("Deleting row with key %s (timestamp %lld", key.c_str(), timestamp);
  remove(key, cp, timestamp, ConsistencyLevel::ONE);
}


void Client::
delete_columns(const std::vector<RowColumns>& to_rm,
               int64_t timestamp)
{
  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  TRC_DEBUG("Constructing cassandra delete request with timestamp %lld", timestamp);
  for (std::vector<RowColumns>::const_iterator it = to_rm.begin();
       it != to_rm.end();
       ++it)
  {
    if (it->columns.empty())
    {
      TRC_DEBUG("Deleting row %s:%s", it->cf.c_str(), it->key.c_str());
      ColumnPath cp;
      cp.column_family = it->cf;

      remove(it->key, cp, timestamp, ConsistencyLevel::ONE);
    }
    else
    {
      std::vector<Mutation> mutations;
      Mutation mutation;
      Deletion deletion;
      SlicePredicate what;

      std::vector<std::string> column_names;

      for (std::map<std::string, std::string>::const_iterator col = it->columns.begin();
           col != it->columns.end();
           ++col)
      {
        // Vector of mutations (one per column being modified).
        column_names.push_back(col->first);
      }

      what.__set_column_names(column_names);
      TRC_DEBUG("Deleting %d columns from %s:%s", what.column_names.size(), it->cf.c_str(), it->key.c_str());

      deletion.__set_predicate(what);
      deletion.__set_timestamp(timestamp);
      mutation.__set_deletion(deletion);
      mutations.push_back(mutation);

      mutmap[it->key][it->cf] = mutations;
    }
  }

  // Execute the batch delete operation if we've constructed one.
  if (!mutmap.empty()) {
    TRC_DEBUG("Executing delete request operation");
    batch_mutate(mutmap, ConsistencyLevel::ONE);
  }
}

void Client::
delete_slice(const std::string& column_family,
             const std::string& key,
             const std::string& start,
             const std::string& finish,
             const int64_t timestamp)
{
  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // We need to build a mutation that contains a deletion that specifies a slice
  // predicate that is a range of column names. Set all of this up.
  Mutation mutation;
  Deletion deletion;
  SlicePredicate predicate;
  SliceRange range;

  range.__set_start(start);
  range.__set_finish(finish);
  predicate.__set_slice_range(range);
  deletion.__set_predicate(predicate);
  deletion.__set_timestamp(timestamp);
  mutation.__set_deletion(deletion);

  // Add the mutation to the mutation map with the correct key and column
  // family and call into thrift.
  mutmap[key][column_family].push_back(mutation);
  batch_mutate(mutmap, ConsistencyLevel::ONE);
}


bool find_column_value(std::vector<cass::ColumnOrSuperColumn> cols,
                                  const std::string& name,
                                  std::string& value)
{
  for (std::vector<ColumnOrSuperColumn>::const_iterator it = cols.begin();
       it != cols.end();
       ++it)
  {
    if ((it->__isset.column) && (it->column.name == name))
    {
      value = it->column.value;
      return true;
    }
  }
  return false;
}

} // namespace CassandraStore
