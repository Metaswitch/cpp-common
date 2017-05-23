/**
 * @file cassandra_store.h Base classes for a cassandra-backed store.
 *
 * Copyright (C) Metaswitch Networks 2017
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#ifndef CASSANDRA_STORE_H_
#define CASSANDRA_STORE_H_

// Free diameter uses cmake to define some compile time options.  Thrift also
// defines these options.  So an app that uses both won't compile because of the
// multiple definition.
//
// To work around this undefine any troublesome macros here. This means that any
// code that includes DiameterStack and a Cassandra store must include the
// store last.
#ifdef HAVE_CLOCK_GETTIME
#undef HAVE_CLOCK_GETTIME
#endif

#ifdef HAVE_MALLOC_H
#undef HAVE_MALLOC_H
#endif

#ifdef ntohll
#undef ntohll
#endif

#ifdef htonll
#undef htonll
#endif

#include "thrift/Thrift.h"
#include "thrift/transport/TSocket.h"
#include "thrift/transport/TTransport.h"
#include "thrift/transport/TBufferTransports.h"
#include "thrift/protocol/TProtocol.h"
#include "thrift/protocol/TBinaryProtocol.h"
#include "Cassandra.h"

#include "threadpool.h"
#include "utils.h"
#include "sas.h"
#include "communicationmonitor.h"
#include "a_record_resolver.h"
#include "cassandra_connection_pool.h"

// Shortcut for the apache cassandra namespace.
namespace cass = org::apache::cassandra;

namespace CassandraStore {

// Forward declarations to break circular dependencies.
class Operation;
class Transaction;

/// Simple data structure to allow specifying a set of column names and values
/// for a particular row (i.e. key in a column family). Useful when batching
/// operations across multiple column families into one Thrift request.
struct RowColumns
{
  /// Default constructor. Allows the structure to be filled in bit by bit by
  /// the user code.
  RowColumns() {};

  /// Constructor to build the complete object. Useful if all the parameters
  /// are known ahead of time.
  RowColumns(const std::string& cf_param,
             const std::string& key_param,
             const std::map<std::string, std::string>& columns_param) :
    cf(cf_param), key(key_param), columns(columns_param)
  {}

  /// Constructor to build the complete object but not specifying any columns
  /// (useful when deleting an entire row).
  RowColumns(const std::string& cf_param,
             const std::string& key_param) :
    cf(cf_param), key(key_param), columns()
  {}

  std::string cf;
  std::string key;
  std::map<std::string, std::string> columns;
};

/// Utility method to extract the value of a named column.
///
/// @param cols  - The slice to search.
/// @param name  - The name of the column to look for.
/// @param value - (out) The column's value. Only valid if this function
///                returns true.
///
/// @return      - Whether the column was found.
bool find_column_value(std::vector<cass::ColumnOrSuperColumn> cols,
                       const std::string& name,
                       std::string& value);

/// Cassandra does not treat a non-existent row as a special case. If the user
/// gets a non-existent row thrift simply returns 0 columns.  This is almost
/// never the desired behaviour, so the store converts such a result into a
/// RowNotFoundException.
///
/// If an operation does not want to treat this an error, it should simply
/// catch the exception.
struct RowNotFoundException
{
  RowNotFoundException(const std::string& column_family, const std::string& key) :
    column_family(column_family), key(key)
  {};

  virtual ~RowNotFoundException() {} ;

  const std::string column_family;
  const std::string key;
};

/// Cassandra client that the store uses.  This class contains some virtual
/// methods (which mirror the thrift interface) to allow it to be mocked out in
/// UT.
class Client
{
public:
  virtual ~Client() {}
  virtual bool is_connected() = 0;
  virtual void connect() = 0;
  virtual void set_keyspace(const std::string& keyspace) = 0;
  virtual void batch_mutate(const std::map<std::string, std::map<std::string, std::vector<cass::Mutation> > >& mutation_map,
                            const cass::ConsistencyLevel::type consistency_level) = 0;
  virtual void get_slice(std::vector<cass::ColumnOrSuperColumn>& _return,
                         const std::string& key,
                         const cass::ColumnParent& column_parent,
                         const cass::SlicePredicate& predicate,
                         const cass::ConsistencyLevel::type consistency_level) = 0;
  virtual void multiget_slice(std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& _return,
                              const std::vector<std::string>& key,
                              const cass::ColumnParent& column_parent,
                              const cass::SlicePredicate& predicate,
                              const cass::ConsistencyLevel::type consistency_level) = 0;
  virtual void remove(const std::string& key,
                      const cass::ColumnPath& column_path,
                      const int64_t timestamp,
                      const cass::ConsistencyLevel::type consistency_level) = 0;
  virtual void get_range_slices(std::vector<cass::KeySlice> & _return,
                                const cass::ColumnParent& column_parent,
                                const cass::SlicePredicate& predicate,
                                const cass::KeyRange& range,
                                const cass::ConsistencyLevel::type consistency_level) = 0;

  //
  // Utility methods for interacting with cassandra. These abstract away the
  // thrift interface and make it easier to deal with.
  //
  // High-availability Gets
  // ----------------------
  //
  // After growing a cluster, Cassandra does not pro-actively populate the
  // new nodes with their data (the nodes are expected to use `nodetool
  // repair` if they need to get their data).  Combining this with
  // the fact that we generally use consistency ONE when reading data, the
  // behaviour on new nodes is to return NotFoundException or empty result
  // sets to queries, even though the other nodes have a copy of the data.
  //
  // To resolve this issue, we define ha_ versions of various get methods.
  // These attempt a QUORUM read in the event that a ONE read returns
  // no data.  If the QUORUM read fails due to unreachable nodes, the
  // original result will be used.
  //
  // To implement this, the non-HA versions must take the consistency level as
  // their last parameter.
  //

  /// HA get an entire row.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key.
  /// @param columns        - (out) Columns in the row.
  void ha_get_row(const std::string& column_family,
                  const std::string& key,
                  std::vector<cass::ColumnOrSuperColumn>& columns,
                  SAS::TrailId trail);

  /// HA get specific columns in a row.
  ///
  /// Note that if a requested row does not exist in cassandra, this method
  /// will return only the rows that do exist. It will not throw an exception
  /// in this case.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param names          - The names of the columns to retrieve
  /// @param columns        - (out) The retrieved columns
  void ha_get_columns(const std::string& column_family,
                      const std::string& key,
                      const std::vector<std::string>& names,
                      std::vector<cass::ColumnOrSuperColumn>& columns,
                      SAS::TrailId trail);

  /// HA get all columns in a row
  /// This is useful when working with dynamic columns.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param columns        - (out) The retrieved columns.
  void ha_get_all_columns(const std::string& column_family,
                          const std::string& key,
                          std::vector<cass::ColumnOrSuperColumn>& columns,
                          SAS::TrailId trail);

  /// HA get all columns in a row that have a particular prefix to their name.
  /// This is useful when working with dynamic columns.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param prefix         - The prefix
  /// @param columns        - (out) the retrieved columns. NOTE: the column
  ///                         names have their prefix removed.
  void ha_get_columns_with_prefix(const std::string& column_family,
                                  const std::string& key,
                                  const std::string& prefix,
                                  std::vector<cass::ColumnOrSuperColumn>& columns,
                                  SAS::TrailId trail);

  /// HA get all columns in multiple rows that have a particular prefix to their
  /// name.  This is useful when working with dynamic columns.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param prefix         - The prefix
  /// @param columns        - (out) the retrieved columns.  Returned as a map
  ///                         where the keys are the requested row keys and
  ///                         each value is a vector of columns. NOTE: the
  ///                         column names have their prefix removed.
  void ha_multiget_columns_with_prefix(const std::string& column_family,
                                       const std::vector<std::string>& keys,
                                       const std::string& prefix,
                                       std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& columns,
                                       SAS::TrailId trail);

  /// Get an entire row (non-HA).
  /// @param consistency_level cassandra consistency level.
  void get_row(const std::string& column_family,
               const std::string& key,
               std::vector<cass::ColumnOrSuperColumn>& columns,
               cass::ConsistencyLevel::type consistency_level);

  /// Get specific columns in a row (non-HA).
  /// @param consistency_level cassandra consistency level.
  void get_columns(const std::string& column_family,
                   const std::string& key,
                   const std::vector<std::string>& names,
                   std::vector<cass::ColumnOrSuperColumn>& columns,
                   cass::ConsistencyLevel::type consistency_level);

  /// Get columns whose names begin with the specified prefix. (non-HA).
  ///
  /// @param consistency_level cassandra consistency level.
  void get_columns_with_prefix(const std::string& column_family,
                               const std::string& key,
                               const std::string& prefix,
                               std::vector<cass::ColumnOrSuperColumn>& columns,
                               cass::ConsistencyLevel::type consistency_level);

  /// Get all columns in multiple rows that have a particular prefix to their
  /// name.
  ///
  /// @param consistency_level cassandra consistency level.
  void multiget_columns_with_prefix(const std::string& column_family,
                                    const std::vector<std::string>& key,
                                    const std::string& prefix,
                                    std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& columns,
                                    cass::ConsistencyLevel::type consistency_level);

  /// Utility method to issue a get request for a particular key.
  ///
  /// @param column_family     - The column family to operate on.
  /// @param key               - Row key
  /// @param predicate         - Slice predicate specifying what columns to get.
  /// @param columns           - (out) The retrieved columns.
  /// @param consistency_level - Cassandra consistency level.
  void issue_get_for_key(const std::string& column_family,
                         const std::string& key,
                         const cass::SlicePredicate& predicate,
                         std::vector<cass::ColumnOrSuperColumn>& columns,
                         cass::ConsistencyLevel::type consistency_level);

  /// Utility method to issue a get request for multiple keys.
  ///
  /// @param column_family     - The column family to operate on.
  /// @param keys              - Row keys
  /// @param predicate         - Slice predicate specifying what columns to get.
  /// @param columns           - (out) The retrieved columns. Returned as a map
  ///                            of row keys => vectors of columns.
  /// @param consistency_level - Cassandra consistency level.
  void issue_multiget_for_key(const std::string& column_family,
                              const std::vector<std::string>& keys,
                              const cass::SlicePredicate& predicate,
                              std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& columns,
                              cass::ConsistencyLevel::type consistency_level);

  /// Write columns to a row/rows. If multiple rows are specified the same
  /// columns are written to all rows.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param columns        - The columns to write. Specified as a map
  ///                         {name => value}
  /// @param timestamp      - The timestamp to write the columns with.
  /// @param ttl            - The TTL to write the columns with.
  /// @param consistency_level - Cassandra consistency level.
  void put_columns(const std::string& column_family,
                   const std::vector<std::string>& keys,
                   const std::map<std::string, std::string>& columns,
                   int64_t timestamp,
                   int32_t ttl,
                   cass::ConsistencyLevel::type consistency_level = cass::ConsistencyLevel::ONE);

  /// Write columns to the database.  This allows for complex writing of
  /// different columns to different column families and/or keys.
  ///
  /// @param columns        - A vector where each entry describes the columns
  ///                         to put to a particular row.
  /// @param timestamp      - The timestamp to write the columns with.
  /// @param ttl            - The TTL to write the columns with.
  void put_columns(const std::vector<RowColumns>& columns,
                   int64_t timestamp,
                   int32_t ttl);

  /// Delete a row from the database.
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param timestamp      - The timestamp to put on the deletion operation.
  void delete_row(const std::string& column_family,
                  const std::string& key,
                  int64_t timestamp);

  /// Delete an arbitrary selection of columns from the database.
  ///
  /// @param columns        - A vector where each entry describes the columns
  ///                         to put to a particular row.
  /// @param timestamp      - The timestamp to put on the deletion operation.
  void delete_columns(const std::vector<RowColumns>& columns,
                      int64_t timestamp);

  /// Delete a slice of columns from a row where the slice is from start
  /// (inclusive) to end (exclusive).
  ///
  /// @param column_family  - The column family to operate on.
  /// @param key            - Row key
  /// @param start          - The start of the range.
  /// @param finish         - The end of the range.
  /// @param timestamp      - The timestamp to put on the deletion operation.
  void delete_slice(const std::string& column_family,
                    const std::string& key,
                    const std::string& start,
                    const std::string& finish,
                    const int64_t timestamp);
};

/// Implementation of a store client.
///
/// This wraps a normal cassandra client, and automatically deletes its
/// transport when it is destroyed.
class RealThriftClient : public Client
{
public:
  RealThriftClient(boost::shared_ptr<apache::thrift::protocol::TProtocol> prot,
         boost::shared_ptr<apache::thrift::transport::TFramedTransport> transport);
  ~RealThriftClient();

  bool is_connected();
  void connect();
  void set_keyspace(const std::string& keyspace);
  void batch_mutate(const std::map<std::string,
                    std::map<std::string,
                    std::vector<cass::Mutation> > >& mutation_map,
                    const cass::ConsistencyLevel::type consistency_level);
  void get_slice(std::vector<cass::ColumnOrSuperColumn>& _return,
                 const std::string& key,
                 const cass::ColumnParent& column_parent,
                 const cass::SlicePredicate& predicate,
                 const cass::ConsistencyLevel::type consistency_level);
  void multiget_slice(std::map<std::string, std::vector<cass::ColumnOrSuperColumn> >& _return,
                      const std::vector<std::string>& keys,
                      const cass::ColumnParent& column_parent,
                      const cass::SlicePredicate& predicate,
                      const cass::ConsistencyLevel::type consistency_level);
  void remove(const std::string& key,
              const cass::ColumnPath& column_path,
              const int64_t timestamp,
              const cass::ConsistencyLevel::type consistency_level);
  void get_range_slices(std::vector<cass::KeySlice> & _return,
                        const cass::ColumnParent& column_parent,
                        const cass::SlicePredicate& predicate,
                        const cass::KeyRange& range,
                        const cass::ConsistencyLevel::type consistency_level);

private:
  cass::CassandraClient _cass_client;
  boost::shared_ptr<apache::thrift::transport::TFramedTransport> _transport;
  bool _connected;
};

/// The possible outcomes of a cassandra interaction.
///
/// These values are logged to SAS so:
/// -  Each element must have an explicit value.
/// -  If you change the enum you must also update the resource bundle.
enum ResultCode
{
  OK = 0,
  INVALID_REQUEST = 1,
  NOT_FOUND = 2,
  CONNECTION_ERROR = 3,
  RESOURCE_ERROR = 4,
  UNKNOWN_ERROR = 5,
  UNAVAILABLE = 6
};

/// The byte sequences that represent True and False in cassandra.
const std::string BOOLEAN_FALSE = std::string("\x00", 1);
const std::string BOOLEAN_TRUE = std::string("\x01", 1);

class Store
{
public:

  // Constructor
  //
  // @param keyspace         - The cassandra keyspace that the store uses.
  Store(const std::string& keyspace);

  /// Virtual destructor
  virtual ~Store();

  /// Generate a timestamp suitable for supplying on cache modification
  /// requests.
  ///
  /// @return                - The current time (in micro-seconds).
  static int64_t generate_timestamp();

  /// Configure the connection to Cassandra.
  ///
  /// @param cass_hostname     - The hostname for the cassandra database.
  /// @param cass_port         - The port to connect to cassandra on.
  /// @param comm_monitor      - A monitor to track communication with the local
  ///                            Cassandra instance, and set/clear alarms based
  ///                            on recent activity.
  /// @param resolver          - The DNS resolver to use.
  virtual void configure_connection(std::string cass_hostname,
                                    uint16_t cass_port,
                                    BaseCommunicationMonitor* comm_monitor = NULL,
                                    CassandraResolver* resolver = NULL);

  /// Tests the store.
  ///
  /// Checks that the store can connect to Cassandra.  This method can be called
  /// before or after starting the store.
  ///
  /// @return                - The status of the store connection.
  virtual ResultCode connection_test();

  /// Perform an operation synchronously.  This blocks the current thread
  /// until the operation is complete.  The result of the operation is stored
  /// on the operation object.
  ///
  /// This method calls into perform_op(), which in turn runs perform() on the
  /// underlying operation. It also provides two additional features:
  ///
  /// -  If the store cannot connect to cassandra, it will try to
  ///    re-establish it's connection and retry the operation (by calling
  ///    perform() for a second time).
  ///
  /// -  It catches all possible thrift exceptions and converts them to
  ///    appropriate error codes. This means that the operation does not need
  ///    to catch them itself.
  ///
  /// @param op     - The operation to run.
  /// @param trail  - SAS trail ID.
  ///
  /// @return       - Whether the operation succeeded.
  virtual bool do_sync(Operation* op, SAS::TrailId);

  ///
  /// Asynchronous interface to the store.
  ///

  /// Configure the worker pool.
  ///
  /// @param exception_handler - The exception handler
  /// @param num_threads       - The number of worker threads to use for
  ///                            processing cassandra requests asynchronously.
  /// @param max_queue         - The maximum number of requests that can be
  ///                            queued waiting for a worker thread when running
  ///                            requests asynchronously.  If more requests are
  ///                            added the call to do_async() will block until
  ///                            some existing requests have been processed.
  ///                            0 => no limit.
  virtual void configure_workers(ExceptionHandler* exception_handler,
                                 unsigned int num_threads,
                                 unsigned int max_queue = 0);

  /// Start the store.
  ///
  /// Start any necessary worker threads.
  ///
  /// @return                - The result of starting the store.
  virtual ResultCode start();

  /// Stop the store.
  ///
  /// This discards any queued requests and terminates the worker threads once
  /// their current request has completed.
  virtual void stop();

  /// Wait until the store has completely stopped.  This method may block.
  virtual void wait_stopped();

  /// Perform an operation asynchronously.  The calling thread does not block.
  /// Instead the operation is performed on a worker thread owned by the store.
  ///
  /// The user must supply a transaction object in addition to the operation to
  /// run. When the operation is complete the worker thread calls back to the
  /// user via a method on the transaction: either on_success() (if the
  /// operation succeeded) or on_failure() (if it failed).
  ///
  /// The store takes ownership of the transaction and will destroy it once it
  /// has resolved.
  ///
  /// @param op              - The operation to perform. The store takes
  ///                          ownership of the object while the operation is
  ///                          executed, and deletes it once it is complete.
  /// @param trx             - The transaction that is used to call back into
  ///                          the user of the store.  The store takes
  ///                          ownership of the object while the operation is
  ///                          executed, and deletes it once it is complete.
  virtual void do_async(Operation*& op, Transaction*& trx);

private:
  /// The thread pool used by the store.  This is a simple subclass of
  /// ThreadPool that also stores a pointer back to the store.
  class Pool : public ThreadPool<std::pair<Operation*, Transaction*> >
  {
  public:
    Pool(Store* store,
         unsigned int num_threads,
         ExceptionHandler* exception_handler,
         unsigned int max_queue = 0);
    virtual ~Pool();

  private:
    Store* _store;

    void process_work(std::pair<Operation*, Transaction*>& params);

    static void exception_callback(std::pair<Operation*, Transaction*> work)
    {
      // No recovery behaviour as this is asynchronos, so we can't sensibly
      // respond
    }
  };

  // Private method that is used by do_sync() and connection_test()
  bool perform_op(Operation* op,
                  SAS::TrailId trail,
                  ResultCode& cass_result,
                  std::string& cass_error_text);

  // DNS resolver
  CassandraResolver* _resolver;

  // The keyspace that the store connects to.
  const std::string _keyspace;

  // Cassandra connection information.
  std::string _cass_hostname;
  uint16_t _cass_port;

  // Exception handler
  ExceptionHandler* _exception_handler;

  // Thread pool management.
  //
  // _num_threads and _max_queue are set up by the call to configure().  These
  // are used when creating the thread pool in the call to start().
  unsigned int _num_threads;
  unsigned int _max_queue;
  Pool* _thread_pool;

  // Helper used to track local communication state, and issue/clear alarms
  // based upon recent activity.
  BaseCommunicationMonitor* _comm_monitor;

  // Cassandra connection management.
  //
  // The CassandraConnectionPool manages the actual connections. Each thread
  // requests a connection from the pool when it is needed, and returns it
  // when it is finished.
  CassandraConnectionPool* _conn_pool;
};

/// Base class for transactions used to perform asynchronous operations.
///
/// This also times the length of the transaction for statistics / throttling
/// purposes.
class Transaction
{
public:
  /// Constructor
  ///
  /// @param trail      - The SAS trail ID that is in scope when the
  ///                     operation was initiated and should be used when
  ///                     resuming processing after the operation completes.
  Transaction(SAS::TrailId trail_param) : trail(trail_param) {}

  /// Virtual destructor.
  virtual ~Transaction() {}

  /// Callback that is called by the store when an operation succeeds.
  ///
  /// @param op           - The operation that was performed.
  virtual void on_success(Operation* op) = 0;

  /// Callback that is called by the store when an operation fails.
  ///
  /// @param op           - The operation that was performed.
  virtual void on_failure(Operation* op) = 0;

  /// How long the transaction took to complete.
  ///
  /// @param duration_us  - (out) The duration in microseconds.
  ///
  /// @return             - Whether the duration was recorded successfully.
  ///                       duration_us is only valid if the return value is
  ///                       true.
  inline bool get_duration(unsigned long& duration_us)
  {
    return _stopwatch.read(duration_us);
  }

  const SAS::TrailId trail;

private:
  friend class Store;

  /// Start the transaction timer.
  inline void start_timer() { _stopwatch.start(); }

  /// Stop the transaction timer.
  inline void stop_timer()  { _stopwatch.stop(); }

  Utils::StopWatch _stopwatch;
};

// Each operation involving the store is represented by an operation object.
// This is the abstract base class for all such objects.
class Operation
{
public:
  /// Default constructor
  Operation();

  /// Virtual destructor.
  virtual ~Operation() {};

  /// @return       - The result code for this operation.
  virtual ResultCode get_result_code();

  /// @return       - The error text describing why this operation failed,
  ///                 or an empty string if the operation succeeded.
  virtual std::string get_error_text();

protected:
  friend class Store;

  /// Method that contains the business logic of the operation.  This is called
  /// automatically after the operation is passed to the store.
  ///
  /// This method may be called multiple times by the store (if a thrift call
  /// fails because the cassandra connection is down).  The operation must be
  /// resistant to being called multiple times in this situation.
  ///
  /// The thrift API throws exceptions when it encounters an error.  This
  /// method does *not* need to cope with these errors itself (unless it wishes
  /// to task some corrective action). Instead it should let them propagate -
  /// the store will catch them and convert them to appropriate error codes.
  ///
  /// @param client  - A client to use to interact with cassandra.
  /// @param trail   - SAS trail ID.
  ///
  /// @return        - Whether the operation succeeded.
  virtual bool perform(Client* client, SAS::TrailId trail) = 0;

  /// Called automatically by the store if it catches an unhandled exception.
  /// The store converts these to a result code and a textual description.
  ///
  /// The default implementation of this method stores the parameters on the
  /// operation object.
  ///
  /// @param status       - The result code corresponding to the underlying
  ///                       exception.
  /// @param description  - A textual description of the exception.
  /// @param trail        - The SAS trail ID in context.
  virtual void unhandled_exception(ResultCode status,
                                   std::string& description,
                                   SAS::TrailId trail);

  /// The cassandra status of the operation.
  ResultCode _cass_status;

  /// If the operation hit an exception doing a cassandra operation, the error
  /// text describing the exception.
  std::string _cass_error_text;
};

}; // namespace CassandraStore

#endif
