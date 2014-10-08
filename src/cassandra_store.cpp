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

//
// Client methods
//

// LCOV_EXCL_START real clients are not tested in UT.
Client::Client(boost::shared_ptr<TProtocol> prot,
               boost::shared_ptr<TFramedTransport> transport) :
  _cass_client(prot),
  _transport(transport)
{
  _transport->open();
}

Client::~Client()
{
  _transport->close();
}

void Client::set_keyspace(const std::string& keyspace)
{
  _cass_client.set_keyspace(keyspace);
}

void Client::batch_mutate(const std::map<std::string, std::map<std::string, std::vector<cass::Mutation> > >& mutation_map,
                          const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.batch_mutate(mutation_map, consistency_level);
}

void Client::get_slice(std::vector<cass::ColumnOrSuperColumn>& _return,
                       const std::string& key,
                       const cass::ColumnParent& column_parent,
                       const cass::SlicePredicate& predicate,
                       const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.get_slice(_return, key, column_parent, predicate, consistency_level);
}

void Client::multiget_slice(std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& _return,
                            const std::vector<std::string>& keys,
                            const cass::ColumnParent& column_parent,
                            const cass::SlicePredicate& predicate,
                            const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.multiget_slice(_return, keys, column_parent, predicate, consistency_level);
}

void Client::remove(const std::string& key,
                    const cass::ColumnPath& column_path,
                    const int64_t timestamp,
                    const cass::ConsistencyLevel::type consistency_level)
{
  _cass_client.remove(key, column_path, timestamp, consistency_level);
}
// LCOV_EXCL_STOP


//
// Store methods
//

Store::Store(const std::string& keyspace) :
  _keyspace(keyspace),
  _cass_hostname(""),
  _cass_port(0),
  _num_threads(0),
  _max_queue(0),
  _thread_pool(NULL),
  _comm_monitor(NULL),
  _thread_local()
{
  // Create the thread-local storage that stores cassandra connections.
  // delete_client is a destroy callback that is called when the thread exits.
  pthread_key_create(&_thread_local, delete_client);
}


void Store::initialize()
{
  // There is nothing to initialize - this is intentionally a no-op.
}


void Store::configure(std::string cass_hostname,
                      uint16_t cass_port,
                      unsigned int num_threads,
                      unsigned int max_queue,
                      CommunicationMonitor* comm_monitor)
{
  LOG_STATUS("Configuring store");
  LOG_STATUS("  Hostname:  %s", cass_hostname.c_str());
  LOG_STATUS("  Port:      %u", cass_port);
  LOG_STATUS("  Threads:   %u", num_threads);
  LOG_STATUS("  Max Queue: %u", max_queue);
  _cass_hostname = cass_hostname;
  _cass_port = cass_port;
  _num_threads = num_threads;
  _max_queue = max_queue;
  _comm_monitor = comm_monitor;
}


ResultCode Store::start()
{
  ResultCode rc = OK;

  // Check that we can connect to cassandra by getting a client. This logs in
  // and switches to the specified keyspace, so is a good test of whether
  // cassandra is working properly.
  LOG_STATUS("Starting store");
  try
  {
    get_client();
    release_client();
  }
  catch(TTransportException te)
  {
    LOG_ERROR("Cache caught TTransportException: %s", te.what());
    rc = CONNECTION_ERROR;
  }
  catch(NotFoundException nfe)
  {
    LOG_ERROR("Cache caught NotFoundException: %s", nfe.what());
    rc = NOT_FOUND;
  }
  catch(...)
  {
    LOG_ERROR("Cache caught unknown exception!");
    rc = UNKNOWN_ERROR;
  }

  // Start the thread pool.
  if (rc == OK)
  {
    if (_num_threads > 0)
    {
      _thread_pool = new Pool(this, _num_threads, _max_queue);

      if (!_thread_pool->start())
      {
        rc = RESOURCE_ERROR; // LCOV_EXCL_LINE
      }
    }
  }

  return rc;
}


void Store::stop()
{
  LOG_STATUS("Stopping cache");
  if (_thread_pool != NULL)
  {
    _thread_pool->stop();
  }
}


void Store::wait_stopped()
{
  LOG_STATUS("Waiting for cache to stop");
  if (_thread_pool != NULL)
  {
    _thread_pool->join();

    delete _thread_pool; _thread_pool = NULL;
  }
}


int64_t Store::generate_timestamp()
{
  // Return the current time in microseconds.
  timespec clock_time;
  int64_t timestamp;

  clock_gettime(CLOCK_REALTIME, &clock_time);
  timestamp = clock_time.tv_sec;
  timestamp *= 1000000;
  timestamp += (clock_time.tv_nsec / 1000);

  LOG_DEBUG("Generated Cassandra timestamp %llu", timestamp);
  return timestamp;
}


Store::~Store()
{
  // It is only safe to destroy the store once the thread pool has been deleted
  // (as the pool stores a pointer to the store). Make sure this is the case.
  stop();
  wait_stopped();
}


// LCOV_EXCL_START - UTs do not cover relationship of clients to threads.
ClientInterface* Store::get_client()
{
  // See if we've already got a client for this thread.  If not allocate a new
  // one and write it back into thread-local storage.
  LOG_DEBUG("Getting thread-local CacheClientInterface");
  ClientInterface* client = (ClientInterface*)pthread_getspecific(_thread_local);

  if (client == NULL)
  {
    LOG_DEBUG("No thread-local CacheClientInterface - creating one");
    boost::shared_ptr<TTransport> socket =
      boost::shared_ptr<TSocket>(new TSocket(_cass_hostname, _cass_port));
    boost::shared_ptr<TFramedTransport> transport =
      boost::shared_ptr<TFramedTransport>(new TFramedTransport(socket));
    boost::shared_ptr<TProtocol> protocol =
      boost::shared_ptr<TBinaryProtocol>(new TBinaryProtocol(transport));
    client = new Client(protocol, transport);
    client->set_keyspace(_keyspace);
    pthread_setspecific(_thread_local, client);
  }

  return client;
}


void Store::release_client()
{
  // If this thread already has a client delete it and remove it from
  // thread-local storage.
  LOG_DEBUG("Looking to release thread-local client");
  ClientInterface* client = (ClientInterface*)pthread_getspecific(_thread_local);

  if (client != NULL)
  {
    LOG_DEBUG("Found thread-local client - destroying");
    delete_client(client);
    client = NULL;
    pthread_setspecific(_thread_local, NULL);
  }
}


void Store::delete_client(void* client)
{
  delete (ClientInterface*)client; client = NULL;
}
// LCOV_EXCL_STOP


bool Store::do_sync(Operation* op, SAS::TrailId trail)
{
  return run(op, trail);
}


void Store::do_async(Operation*& op, Transaction*& trx)
{
  if (_thread_pool == NULL)
  {
    LOG_ERROR("Can't process async operation as no thread pool has been configured");
    assert(!"Can't process async operation as no thread pool has been configured");
  }

  std::pair<Operation*, Transaction*> params(op, trx);
  _thread_pool->add_work(params);

  // The caller no longer owns the operation or transaction, so null them out.
  op = NULL;
  trx = NULL;
}


bool Store::run(Operation* op, SAS::TrailId trail)
{
  bool success = false;
  ResultCode cass_result = OK;
  std::string cass_error_text = "";

  // Set up whether the perform should be retried on failure.
  // Only try once, unless there's connection error, in which case try twice.
  bool retry = false;
  int attempt_count = 0;

  // Get a client to execute the operation.
  ClientInterface* client = get_client();

  // Call perform() to actually do the business logic of the request.  Catch
  // exceptions and turn them into return codes and error text.
  do
  {
    retry = false;

    try
    {
      attempt_count++;
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

      // Recycle the connection.
      release_client(); client = NULL;

      if (attempt_count <= 1)
      {
        // Connection error, destroy and recreate the connection, and retry the
        //  request once
        LOG_DEBUG("Connection error, retrying");
        try
        {
          client = get_client();
          retry = true;
          cass_result = OK;
        }
        catch(...)
        {
          // If there's any error recreating the connection just catch the
          // exception, and the connection will be recreated in the next cycle
          LOG_ERROR("Cache caught unknown exception");
        }
      }
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
    catch(...)
    {
      cass_result = UNKNOWN_ERROR;
      cass_error_text = "Unknown error";
    }
  }
  while (retry);

  if (cass_result == OK)
  {
    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
  else
  {
    if (cass_result == CONNECTION_ERROR && _comm_monitor)
    {
      _comm_monitor->inform_failure();
    }

    LOG_ERROR("Cassandra request failed: rc=%d, %s",
              cass_result, cass_error_text.c_str());
    op->unhandled_exception(cass_result, cass_error_text, trail);
  }

  return success;
}


//
// Pool methods
//

Store::Pool::Pool(Store* store, unsigned int num_threads, unsigned int max_queue) :
  ThreadPool<std::pair<Operation*, Transaction*> >(num_threads, max_queue),
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
    success = _store->run(op, trx->trail);
  }
  // LCOV_EXCL_START Transaction catches all exceptions so the thread pool
  // fallback code is never triggered.
  catch(...)
  {
    LOG_ERROR("Unhandled exception when processing cache request");
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


void Operation::
put_columns(ClientInterface* client,
            const std::string& column_family,
            const std::vector<std::string>& keys,
            const std::map<std::string, std::string>& columns,
            int64_t timestamp,
            int32_t ttl)
{
  // Vector of mutations (one per column being modified).
  std::vector<Mutation> mutations;

  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  LOG_DEBUG("Constructing cache put request with timestamp %lld and per-column TTLs", timestamp);
  for (std::map<std::string, std::string>::const_iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    Mutation mutation;
    Column* column = &mutation.column_or_supercolumn.column;

    column->name = it->first;
    column->value = it->second;
    LOG_DEBUG("  %s => %s (TTL %d)", column->name.c_str(), column->value.c_str(), ttl);
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
  LOG_DEBUG("Executing put request operation");
  client->batch_mutate(mutmap, ConsistencyLevel::ONE);
}


void Operation::
put_columns(ClientInterface* client,
            const std::vector<RowColumns>& to_put,
            int64_t timestamp,
            int32_t ttl)
{
  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  LOG_DEBUG("Constructing cache put request with timestamp %lld and per-column TTLs", timestamp);
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
      LOG_DEBUG("  %s => %s (TTL %d)", column->name.c_str(), column->value.c_str(), ttl);
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
  LOG_DEBUG("Executing put request operation");
  client->batch_mutate(mutmap, ConsistencyLevel::ONE);
}


// Macro to turn an underlying (non-HA) get method into an HA one.
//
// This macro takes the following arguments:
// -  The name of the underlying get method to call.
// -  The arguments for the underlying get method.
//
// It works as follows:
// -  Call the underlying method with a consistency level of ONE.
// -  If this raises a NotFoundException, try again with a consistency level of
//    QUORUM.
// -  If this fails again with either NotFoundException or UnavailableException
//    (meaning the necessary servers are not currently available), re-throw the
//    original exception.
#define HA(METHOD, ...)                                                      \
        try                                                                  \
        {                                                                    \
          METHOD(__VA_ARGS__, ConsistencyLevel::ONE);                        \
        }                                                                    \
        catch(NotFoundException& nfe)                                        \
        {                                                                    \
          LOG_DEBUG("Failed ONE read for %s. Try QUORUM", #METHOD);          \
                                                                             \
          try                                                                \
          {                                                                  \
            METHOD(__VA_ARGS__, ConsistencyLevel::QUORUM);                   \
          }                                                                  \
          catch(NotFoundException)                                           \
          {                                                                  \
            throw nfe;                                                       \
          }                                                                  \
          catch(UnavailableException)                                        \
          {                                                                  \
            throw nfe;                                                       \
          }                                                                  \
        }


void Operation::
ha_get_columns(ClientInterface* client,
               const std::string& column_family,
               const std::string& key,
               const std::vector<std::string>& names,
               std::vector<ColumnOrSuperColumn>& columns)
{
  HA(get_columns, client, column_family, key, names, columns);
}


void Operation::
ha_get_columns_with_prefix(ClientInterface* client,
                           const std::string& column_family,
                           const std::string& key,
                           const std::string& prefix,
                           std::vector<ColumnOrSuperColumn>& columns)
{
  HA(get_columns_with_prefix, client, column_family, key, prefix, columns);
}


void Operation::
ha_multiget_columns_with_prefix(ClientInterface* client,
                                const std::string& column_family,
                                const std::vector<std::string>& keys,
                                const std::string& prefix,
                                std::map<std::string, std::vector<ColumnOrSuperColumn> >& columns)
{
  HA(multiget_columns_with_prefix, client, column_family, keys, prefix, columns);
}

void Operation::
ha_get_all_columns(ClientInterface* client,
                   const std::string& column_family,
                   const std::string& key,
                   std::vector<ColumnOrSuperColumn>& columns)
{
  HA(get_row, client, column_family, key, columns);
}


void Operation::
get_columns(ClientInterface* client,
            const std::string& column_family,
            const std::string& key,
            const std::vector<std::string>& names,
            std::vector<ColumnOrSuperColumn>& columns,
            ConsistencyLevel::type consistency_level)
{
  // Get only the specified column names.
  SlicePredicate sp;
  sp.column_names = names;
  sp.__isset.column_names = true;

  issue_get_for_key(client, column_family, key, sp, columns, consistency_level);
}


void Operation::
get_columns_with_prefix(ClientInterface* client,
                        const std::string& column_family,
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

  issue_get_for_key(client, column_family, key, sp, columns, consistency_level);

  // Remove the prefix from the returned column names.
  for (std::vector<ColumnOrSuperColumn>::iterator it = columns.begin();
       it != columns.end();
       ++it)
  {
    it->column.name = it->column.name.substr(prefix.length());
  }
}


void Operation::
multiget_columns_with_prefix(ClientInterface* client,
                             const std::string& column_family,
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

  issue_multiget_for_key(client, column_family, keys, sp, columns, consistency_level);

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


void Operation::
get_row(ClientInterface* client,
        const std::string& column_family,
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

  issue_get_for_key(client, column_family, key, sp, columns, consistency_level);
}


void Operation::
issue_get_for_key(ClientInterface* client,
                  const std::string& column_family,
                  const std::string& key,
                  const SlicePredicate& predicate,
                  std::vector<ColumnOrSuperColumn>& columns,
                  ConsistencyLevel::type consistency_level)
{
  ColumnParent cparent;
  cparent.column_family = column_family;

  client->get_slice(columns, key, cparent, predicate, consistency_level);

  if (columns.size() == 0)
  {
    RowNotFoundException row_not_found_ex(column_family, key);
    throw row_not_found_ex;
  }
}


void Operation::
issue_multiget_for_key(ClientInterface* client,
                       const std::string& column_family,
                       const std::vector<std::string>& keys,
                       const SlicePredicate& predicate,
                       std::map<std::string, std::vector<ColumnOrSuperColumn> >& columns,
                       ConsistencyLevel::type consistency_level)
{
  ColumnParent cparent;
  cparent.column_family = column_family;

  client->multiget_slice(columns, keys, cparent, predicate, consistency_level);

  if (columns.size() == 0)
  {
    RowNotFoundException row_not_found_ex(column_family, keys.front());
    throw row_not_found_ex;
  }
}


void Operation::
delete_row(ClientInterface* client,
           const std::string& column_family,
           const std::string& key,
           int64_t timestamp)
{
  ColumnPath cp;
  cp.column_family = column_family;

  LOG_DEBUG("Deleting row with key %s (timestamp %lld", key.c_str(), timestamp);
  client->remove(key, cp, timestamp, ConsistencyLevel::ONE);
}


void Operation::
delete_columns(ClientInterface* client,
               const std::vector<RowColumns>& to_rm,
               int64_t timestamp)
{
  // The mutation map is of the form {"key": {"column_family": [mutations] } }
  std::map<std::string, std::map<std::string, std::vector<Mutation> > > mutmap;

  // Populate the mutations vector.
  LOG_DEBUG("Constructing cache delete request with timestamp %lld", timestamp);
  for (std::vector<RowColumns>::const_iterator it = to_rm.begin();
       it != to_rm.end();
       ++it)
  {
    if (it->columns.empty())
    {
      LOG_DEBUG("Deleting row %s:%s", it->cf.c_str(), it->key.c_str());
      ColumnPath cp;
      cp.column_family = it->cf;

      client->remove(it->key, cp, timestamp, ConsistencyLevel::ONE);
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
      LOG_DEBUG("Deleting %d columns from %s:%s", what.column_names.size(), it->cf.c_str(), it->key.c_str());

      deletion.__set_predicate(what);
      deletion.__set_timestamp(timestamp);
      mutation.__set_deletion(deletion);
      mutations.push_back(mutation);

      mutmap[it->key][it->cf] = mutations;
    }
  }

  // Execute the batch delete operation if we've constructed one.
  if (!mutmap.empty()) {
    LOG_DEBUG("Executing delete request operation");
    client->batch_mutate(mutmap, ConsistencyLevel::ONE);
  }
}

void Operation::
delete_slice(ClientInterface* client,
             const std::string& column_family,
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
  client->batch_mutate(mutmap, ConsistencyLevel::ONE);
}

} // namespace CassandraStore
