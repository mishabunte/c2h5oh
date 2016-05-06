#ifndef __ngx_c2h5oh_module_h_included__
#define __ngx_c2h5oh_module_h_included__ 

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>

#include "c2h5oh.h"

//-----------------------------------------------------------------------------
typedef struct {
  ngx_event_t timer; 
  c2h5oh_t * conn;
  ngx_str_t  query;
  ngx_time_t timeout;
  ngx_str_t  callback;
} ngx_c2h5oh_ctx_t;

typedef struct {
  ngx_int_t  enabled;
  ngx_str_t  db_path;
  ngx_msec_t timeout;
  size_t     pool_size;
  ngx_str_t  root;
  ngx_str_t  route;
} ngx_c2h5oh_loc_conf_t;

//-----------------------------------------------------------------------------
// nginx module handlers
static ngx_int_t ngx_c2h5oh_handler(ngx_http_request_t *r);
static void ngx_c2h5oh_cleanup(void * data);
static void ngx_c2h5oh_post_response(ngx_http_request_t * r, 
                                     ngx_c2h5oh_ctx_t * ctx);
//-----------------------------------------------------------------------------
// nginx module config handlers
static void * ngx_c2h5oh_create_loc_conf(ngx_conf_t *cf);
static char * ngx_c2h5oh_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static char * ngx_c2h5oh(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
//-----------------------------------------------------------------------------
#endif //__ngx_c2h5oh_module_h_included__
// eof
