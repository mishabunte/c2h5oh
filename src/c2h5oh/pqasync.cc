#include <libpq-fe.h>
#include <cassert>
#include <stdexcept>
#include <string.h>

#include "pqasync.h"

namespace Pq {

//-----------------------------------------------------------------------------
struct Pg {
  Pg() : conn(nullptr), cancel(nullptr) {}
  PGconn * conn;
  PGcancel * cancel;
};

//-----------------------------------------------------------------------------
PqAsync::PqAsync() 
  : pg(new Pg())
  , conn_string_(nullptr)
  , query_(nullptr)
  , state(PqState::START)
{}

//-----------------------------------------------------------------------------
PqAsync::~PqAsync() 
{
  disconnect();
}

//-----------------------------------------------------------------------------
void PqAsync::disconnect()
{
  if (pg->conn) {
    if (state == PqState::QUERY) {
      cancel_query(false);
    }
    if (pg->cancel) {
      PQfreeCancel(pg->cancel);
      pg->cancel = nullptr;
    }
    state = PqState::START;
    PQfinish(pg->conn);
    pg->conn = nullptr;
    pg->cancel = nullptr;
  }
}

//-----------------------------------------------------------------------------
void PqAsync::clear_result()
{
  result_.clear();
  result_is_null_ = false;
  result_is_error_ = false;
  has_result_ = false;
}
//-----------------------------------------------------------------------------
bool PqAsync::connect(const char * conn_string, bool non_blocking)
{
  assert(conn_string);

  disconnect();
  clear_result();
  if (non_blocking) {
    conn_string_ = conn_string;
    start_connect();
    return true;
  } else {
    conn_string_ = conn_string;
    pg->conn = PQconnectdb(conn_string_);
    if (PQstatus(pg->conn) != CONNECTION_OK) {
      last_error = PQerrorMessage(pg->conn);
      disconnect();
      return false;
    } else {
      PQsetnonblocking(pg->conn, 1);
      state = PqState::CONNECTED;
      return true;
    }
  }
}

//-----------------------------------------------------------------------------
bool PqAsync::do_query(const char * query)
{
  assert(query);

  if (state == PqState::RESULT) {
    state = PqState::CONNECTED;
  }

  if (state == PqState::QUERY) {
    cancel_query();
    query_ = query;
  } else {
    query_ = query;
  }

  return true;
}

//-----------------------------------------------------------------------------
void PqAsync::abort()
{
  if (state == PqState::QUERY) {
    cancel_query(false);
  }
}

//-----------------------------------------------------------------------------
void PqAsync::cancel_query(bool reconnect)
{
  // TODO use PQcancel();
  assert(state == PqState::QUERY);

  pg->cancel = PQgetCancel(pg->conn);
  if (pg->cancel == NULL) {
    if (reconnect) {
      disconnect();
      start_connect();
    }
  } else {
    if (PQcancel(pg->cancel, NULL, 0) == 0) {
      PQfreeCancel(pg->cancel);
      if (reconnect) {
        disconnect();
        start_connect();
      }
    } else {
      state = PqState::CANCEL;
    }
  }
}

//-----------------------------------------------------------------------------
bool PqAsync::poll()
{
  switch(state) {
    case PqState::START :
      start_connect();
      return true;
    case PqState::CONNECTING :
      wait_connected();
      return true;
    case PqState::CONNECTED :
      return !send_query(); 
    case PqState::QUERY :
      return !wait_result();
    case PqState::RESULT :
      return false;
    case PqState::CANCEL :
      wait_result();
      return true;
    default:
      throw std::runtime_error("wrong state in poll");
  }
}

//-----------------------------------------------------------------------------
void PqAsync::start_connect()
{
  assert(state == PqState::START);
  assert(conn_string_);
  
  pg->conn = PQconnectStart(conn_string_);
  state = PqState::CONNECTING;
  if (pg->conn == NULL) {
    throw std::runtime_error("no pq connections available");
  }
}

//-----------------------------------------------------------------------------
void PqAsync::wait_connected()
{
  assert(state == PqState::CONNECTING);

  auto s = PQconnectPoll(pg->conn);
  PQsetnonblocking(pg->conn, 1);

  if (PGRES_POLLING_OK == s) {
    state = PqState::CONNECTED;
    send_query();
  } else if (PGRES_POLLING_FAILED == s) {
    result_is_error_ = true;
    const char * err =  PQerrorMessage(pg->conn);
    if (err != NULL) {
      int len = strlen(err);
      result_.assign(err, len); 
    } else {
      result_.clear();
    }
    disconnect();
    start_connect();
  }
}

//-----------------------------------------------------------------------------
bool PqAsync::check_connected()
{
  auto s = PQstatus(pg->conn);
  if (CONNECTION_BAD == s) {
    result_ = PQerrorMessage(pg->conn);
    result_is_error_ = true;
    disconnect();
    start_connect();
    return false;
  } else if (CONNECTION_OK != s) {
    state = PqState::CONNECTING;
    return false;
  } else {
    return true;
  }
}

//-----------------------------------------------------------------------------
bool PqAsync::send_query()
{
  assert(state == PqState::CONNECTED);
  assert(query_);

  clear_result();

  if (check_connected()) {
    if (PQsendQuery(pg->conn, query_) == 0) {
      state = PqState::CONNECTED;
      return true;
    } else {
      state = PqState::QUERY;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------
bool PqAsync::wait_result()
{
  assert(state == PqState::QUERY || state == PqState::CANCEL);

  if (check_connected()) {
    if (PQisBusy(pg->conn) == 1) {
      if (PQconsumeInput(pg->conn) == 0) {
        disconnect();
        start_connect();
        return false;
      }
    }
    while (PQisBusy(pg->conn) == 0) {
      auto result = PQgetResult(pg->conn);
      if (nullptr == result) {
        if (state == PqState::CANCEL) {
          PQfreeCancel(pg->cancel);
          pg->cancel = NULL;
          state = PqState::CONNECTED;
          return false;
        } else {
          state = PqState::RESULT;
          return true;
        }
      } else {
        if (PQresultStatus(result) == PGRES_FATAL_ERROR) {
          has_result_ = true;
          result_is_error_ = true;
          const char * err =  PQresultErrorField(result, PG_DIAG_SQLSTATE);
          if (err != NULL) {
            int len = strlen(err);
            result_.assign(err, len); 
            result_ += "_";
            result_ += PQresultErrorMessage(result);
          } else {
            result_.clear();
          }
        } else if (PQntuples(result) > 0 && PQnfields(result) > 0) {
          result_ = PQgetvalue(result, 0, 0);
          has_result_   = true;
          result_is_null_ = 1 == PQgetisnull(result, 0, 0);
        } else {
          has_result_ = false;
          result_is_null_ = false;
        }
        PQclear(result);
      }
    }
    return false;
  } else {
    return false;
  }
}

//-----------------------------------------------------------------------------
const std::string & PqAsync::get_result() 
{
  return result_;
}

} // namespace Pq
