#include <cassert> 
#include <stack>

#include "c2h5oh.h"
#include "object_pool.h"
#include "pqasync.h"

//-----------------------------------------------------------------------------
struct c2h5oh {
  Pq::PqAsync pq;
};

namespace {

object_pool<c2h5oh> pool;
std::string conn_str;

} // namespace

//-----------------------------------------------------------------------------
int c2h5oh_module_init(const char * conn_string, size_t str_len, 
                       uint16_t connections_count)
{
  assert(conn_string != NULL);
  assert(str_len > 0);
  assert(connections_count > 0);

  conn_str.assign(conn_string, str_len);

  pool.set_max_size(connections_count);
  
  std::stack<c2h5oh *> v;
  c2h5oh * c;
  while((c = pool.object_new()) != nullptr) {
    c->pq.connect(conn_str.c_str(), true);
    v.push(c);
  }
  while(v.size()) {
    pool.object_delete(v.top());
    v.pop();
  }

  return 0;
}

//-----------------------------------------------------------------------------
void c2h5oh_module_cleanup()
{
  c2h5oh * c;
  while((c = pool.object_new()) != nullptr) {
    c->pq.disconnect();
  }
}

//-----------------------------------------------------------------------------
c2h5oh_t * c2h5oh_create()
{
  return pool.object_new();
}

//-----------------------------------------------------------------------------
void c2h5oh_free(c2h5oh_t * c)
{
  //fprintf(stderr, "try to free");
  assert(c != nullptr);
  c->pq.abort();
  //fprintf(stderr, "connection freed");
  pool.object_delete(c);
}

//-----------------------------------------------------------------------------
int c2h5oh_query(c2h5oh_t * c, const char * query)
{
  assert(c != nullptr);
  return c->pq.do_query(query) ? 0 : -1;
}

//-----------------------------------------------------------------------------
int c2h5oh_poll(c2h5oh_t * c)
{
  assert(c != nullptr);
  return c->pq.poll() ? 0 : 1;
}

//-----------------------------------------------------------------------------
const char * c2h5oh_result(c2h5oh_t * c)
{
  assert(c != nullptr);
  return c->pq.has_result() || c->pq.result_is_error() ? 
    c->pq.get_result().c_str() : NULL;
}

//-----------------------------------------------------------------------------
size_t c2h5oh_result_len(c2h5oh_t * c)
{
  assert(c != nullptr);
  return c->pq.has_result() ? c->pq.get_result().size() : 0;
}

//-----------------------------------------------------------------------------
int c2h5oh_is_error(c2h5oh_t * c)
{
  assert(c != nullptr);
  return c->pq.result_is_error() ? 1 : 0;
}
