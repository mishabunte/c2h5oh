#pragma once

#include <string>
#include <memory>

namespace Pq {
  
//-----------------------------------------------------------------------------
struct Pg; // UGLY an ugly way to hide libpq dependencies from header file
enum class PqState { START, CONNECTING, CONNECTED, QUERY, RESULT, CANCEL };

//-----------------------------------------------------------------------------
/** Postgresql interface */
class PqAsync {

public: 
  /** Constructor */
  PqAsync();
  /** Desctructor */
  virtual ~PqAsync();
  PqAsync(const PqAsync &) = delete;          // disallow copy
  void operator = (const PqAsync &) = delete; // disallow assign


  /** Connect to database, by default this is blocking */
  bool connect(const char * conn_string, bool non_blocking = false);
  /** Disconnect from database */
  void disconnect();
  /** Perform query */
  bool do_query(const char * query);
  /** Abort current query */
  void abort();
  /** Poll query, returns true if query completed */
  bool poll();
  /** Check for result is ready */
  bool has_result() const { return state == PqState::RESULT && has_result_; }
  /** Check for result is null */
  bool result_is_null() const { return result_is_null_; }
  /** Check for result is error */
  bool result_is_error() const { return result_is_error_; }
  /** Returns result */
  const std::string & get_result();
  /** Returns last error */
  const std::string & get_last_error() const { return last_error; }
  bool has_last_error() const { return last_error.empty(); }

private:
  std::unique_ptr<Pg> pg;       // libpq structures
  const char * conn_string_;    // connection string
  const char * query_;          // current query
  PqState state;                // sate
  std::string last_error;       // last error message
  std::string result_;          // last result
  bool        has_result_;      // has_result flag
  bool        result_is_null_;  // result is null flag
  bool        result_is_error_; // result is error flag

  void clear_result();    // clear result data

  bool check_connected(); // check database connection
  void start_connect();   // initiate connection process
  void wait_connected();  // wait while connection to pg established
  bool send_query();      // send query
  bool wait_result();     // wait query result
  void cancel_query(bool reconnect = true); // cancel current query
};

} // namespace Pq
