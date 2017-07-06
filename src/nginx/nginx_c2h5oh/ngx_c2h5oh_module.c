#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>
#include <jsmn.h>

#include "ngx_c2h5oh_module.h"

//-----------------------------------------------------------------------------
#define NGX_C2H5OH_DEFAULT_TIMEOUT 1000   // ms
#define NGX_C2H5OH_JSMN_TOKENS     128

const u_char k_ngx_c2h5oh_select[]        = "select web.";
const u_char ngx_c2h5oh_content_type[]    = "application/json; charset=utf-8";

static jsmntok_t  *ngx_c2h5oh_js_tokens = 0;
static int         ngx_c2h5oh_js_tokens_count = 0;
static jsmn_parser ngx_c2h5oh_jsmn_parser = {};

//-----------------------------------------------------------------------------
static ngx_http_module_t  ngx_c2h5oh_module_ctx = {
  NULL,                            /* preconfiguration */
  NULL,                            /* postconfiguration */

  NULL,                            /* create main configuration */
  NULL,                            /* init main configuration */

  NULL,                            /* create server configuration */
  NULL,                            /* merge server configuration */

  ngx_c2h5oh_create_loc_conf,      /* create location configuration */
  ngx_c2h5oh_merge_loc_conf        /* merge location configuration */
};

//-----------------------------------------------------------------------------
static ngx_command_t  ngx_c2h5oh_commands[] = {
  { ngx_string("c2h5oh_pass"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
    ngx_c2h5oh,
    NGX_HTTP_LOC_CONF_OFFSET,
    0,
    NULL },
  { ngx_string("c2h5oh_timeout"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_c2h5oh_loc_conf_t, timeout),
    NULL },
  { ngx_string("c2h5oh_root"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_c2h5oh_loc_conf_t, root),
    NULL },
  { ngx_string("c2h5oh_route"),
    NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_c2h5oh_loc_conf_t, route),
    NULL },
  ngx_null_command
};

//-----------------------------------------------------------------------------
ngx_int_t ngx_c2h5oh_init_process(ngx_cycle_t * c) 
{
  ngx_c2h5oh_js_tokens_count = NGX_C2H5OH_JSMN_TOKENS;
  ngx_c2h5oh_js_tokens = malloc(sizeof(jsmntok_t) * ngx_c2h5oh_js_tokens_count);

  return NGX_OK;
}

//-----------------------------------------------------------------------------
void ngx_c2h5oh_exit_process(ngx_cycle_t * c) 
{
  free(ngx_c2h5oh_js_tokens);

  c2h5oh_module_cleanup();
}

//-----------------------------------------------------------------------------
ngx_module_t  ngx_c2h5oh_module = {
  NGX_MODULE_V1,
  &ngx_c2h5oh_module_ctx, /* module context */
  ngx_c2h5oh_commands,    /* module directives */
  NGX_HTTP_MODULE,                 /* module type */
  NULL,                            /* init master */
  NULL,                            /* init module */
  ngx_c2h5oh_init_process,     /* init process */
  NULL,                            /* init thread */
  NULL,                            /* exit thread */
  ngx_c2h5oh_exit_process,     /* exit process */
  NULL,                            /* exit master */
  NGX_MODULE_V1_PADDING
};

//-----------------------------------------------------------------------------
static void * ngx_c2h5oh_create_loc_conf(ngx_conf_t *cf)
{
  ngx_c2h5oh_loc_conf_t  *conf;
  conf = ngx_pcalloc(cf->pool, sizeof(ngx_c2h5oh_loc_conf_t));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }
  conf->enabled = 0;
  conf->pool_size = NGX_CONF_UNSET_SIZE;
  conf->timeout   = NGX_CONF_UNSET_MSEC;
  conf->db_path.len  = NGX_CONF_UNSET_UINT;
  conf->db_path.data = NGX_CONF_UNSET_PTR;
  return conf;
}

//-----------------------------------------------------------------------------
static char * 
ngx_c2h5oh_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_c2h5oh_loc_conf_t *prev = parent;
  ngx_c2h5oh_loc_conf_t *conf = child;
  ngx_conf_merge_msec_value(conf->timeout, prev->timeout, NGX_C2H5OH_DEFAULT_TIMEOUT);
  ngx_conf_merge_str_value(conf->db_path, prev->db_path, "");
  ngx_conf_merge_str_value(conf->root, prev->root, "");
  ngx_conf_merge_size_value(conf->pool_size, prev->pool_size, NGX_CONF_UNSET_SIZE);
  conf->enabled = prev->enabled;
  if (conf->enabled) {
    if (conf->pool_size <= 0) {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "c2h5oh pool_size must be more than 1");
      return NGX_CONF_ERROR;
    }
    if (conf->db_path.len == 0) 
    {
      ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "c2h5oh db_path is not specified");
      return NGX_CONF_ERROR;
    }
  }
  return NGX_CONF_OK;
}

//-----------------------------------------------------------------------------
static void
ngx_c2h5oh_cleanup(void * data) {
  ngx_c2h5oh_ctx_t * ctx = data;

  if (ctx->conn != NULL) {
    c2h5oh_free(ctx->conn);
    ctx->conn = NULL;
  }

  if (ctx->timer.timer_set) {
    ngx_del_timer(&ctx->timer);
  }
}

//-----------------------------------------------------------------------------
static int 
ngx_c2h5oh_parse_args(ngx_http_request_t * r, ngx_str_t * res, u_char * start, 
                      u_char * end, u_char * args_start) 
{
  u_char * p = res->data + res->len;
  u_char c;
  while(start < end) {
    if (p != args_start) {
      *p++ = ',';
    }
    *p++ = '"';
    while((*start == '=' || *start == '&') && start < end) start++;
    while(*start != '=' && *start != '&' && start < end) {
      if (*start == '\'') *p++ = '\'';
      else if (*start == '"') *p++ = '\\';
      *p++ = *start++;
    }
    *p++ = '"'; *p++ = ':'; *p++ = '"';
    if (*start != '&') {
      start++;
      while (*start != '&' && *start != '=' && start < end) {
        if (*start == '%') {
          start++;
          c = *start++;
          if (c >= 0x30) c -= 0x30;
          if (c >= 0x10) c -= 0x07;
          if (c >= 0x10 || start > end) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "[c2h5oh] wrong escape sequence");
            return -1;
          }
          *p = (c << 4);
          c = *start++;
          if (c >= 0x30) c -= 0x30;
          if (c >= 0x10) c -= 0x07;
          if (c >= 0x10 || start > end) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "[c2h5oh] wrong escape sequence");
            return -1;
          }
          *p += c;
          c = *p;
        } else if (*start == '+') {
          start++;
          c = ' ';
        } else {
          c = *start++;
        }
        if (c == '\'') *p++ = '\'';
        else if (c == '"') *p++ = '\\';
        *p++ = c;
      }
    } else {
      start++;
    }
    *p++ = '"';
  }
  res->len = p - res->data;
  return 0;
}

//-----------------------------------------------------------------------------
static void 
ngx_c2h5oh_parse_cookies(ngx_str_t * res, ngx_str_t * s, u_char * cookies_start) 
{
  u_char * start = s->data;
  u_char * end   = s->data + s->len;
  u_char * p     = res->data + res->len;
  u_char * name  = p;
  while(start < end) {
    while(*start == ' ' && start < end) ++start; 
    name = p;
    if (p != cookies_start) {
      *p++ = ',';
    }
    *p++ = '"';
    while(*start != ';' && *start != '=' && start < end) {
      if (*start == '\'') *p++ = '\'';
      else if (*start == '"') *p++ = '\\';
      *p++ = *start++;
    }
    if (*start == ';') {
      p = name;
      start++;
    } else if (start >= end) {
      p = name;
    } else {
      start++;
      *p++ = '"'; *p++ = ':'; *p++ = '"';
      while(*start == ' ' && start < end) ++start;
      while(*start != ';' && *start != '=' && start < end) {
        if (*start == '\'') *p++ = '\'';
        else if (*start == '"') *p++ = '\\';
        *p++ = *start++;
      }
      *p++ = '"';
      start++;
      if (*(name + (*name == ',' ? 1 : 0)) == '"' && *(name + (*name == ',' ? 2 : 1)) == '"') {
        p = name;
      }
    }
  }
  res->len = p - res->data;
}

//-----------------------------------------------------------------------------
static void
ngx_c2h5oh_query_data_set_len(ngx_http_request_t *r, ngx_c2h5oh_ctx_t * ctx) 
{
  ctx->query.len = sizeof(k_ngx_c2h5oh_select) + sizeof("','{}','{}',null);\0");
  ngx_uint_t i;
  u_char * start; 
  u_char * end;

  ngx_c2h5oh_loc_conf_t * alcf = ngx_http_get_module_loc_conf(r, ngx_c2h5oh_module);
  ctx->query.len += alcf->route.len + sizeof("'',") - 1;

  ctx->query.len += r->uri.len == 1 ? sizeof("index") - 1 : r->uri.len;
  for(start = end = r->uri.data, end += r->uri.len; start < end; start++) {
    if (*start == '\'') {
      ctx->query.len++;
    }
  }

  // args
  if (r->args.len) {
    ctx->query.len += sizeof("index") - 1;
  }
  for(start = end = r->args.data, end += r->args.len; start < end; start++) {
    ctx->query.len++;
    if (*start == '\'' || *start == '"') {
      ctx->query.len++;
    } else if (*start == '&') {
      ctx->query.len += sizeof("\"\":\"\" ") - 1;
    }
  }

  if (r->request_body && r->request_body->buf && r->headers_in.content_type 
      && r->request_body_in_single_buf)
  {
    if (ngx_memcmp(r->headers_in.content_type->value.data, "application/x-www-form-urlencoded", sizeof("application/x-www-form-urlencoded") - 1) == 0) {
      ctx->query.len += sizeof("\"\":{}") - 1;
      for(start = r->request_body->buf->pos, end = r->request_body->buf->last; start < end; start++) {
        ctx->query.len++;
        if (*start == '\'' || *start == '"') {
          ctx->query.len++;
        } else if (*start == '&') {
          ctx->query.len += sizeof("\"\":\"\" ") - 1;
        }
      }
    } else {
      ctx->query.len += sizeof("\"\":{}") - 1;
      for(start = r->request_body->buf->pos, end = r->request_body->buf->last; start < end; start++) {
        ctx->query.len++;
        if (*start == '\'' || *start == '"') {
          ctx->query.len++;
        }
      }
    }
  }

  ctx->query.len += sizeof("\"\":{}") - 1;
  ngx_table_elt_t  **h;
  h = r->headers_in.cookies.elts;
  for(i = 0; i < r->headers_in.cookies.nelts; i++) {
    for(start = end = h[i]->value.data, end += h[i]->value.len; start < end; start++) {
      ctx->query.len++;
      if (*start == '\'' || *start == '"') {
        ctx->query.len++;
      } else if (*start == ';') {
        ctx->query.len += sizeof("\"\":\"\" ") - 1; 
      }
    }
  }
}

//-----------------------------------------------------------------------------
static int
ngx_c2h5oh_init_query_data(ngx_http_request_t *r, ngx_c2h5oh_ctx_t * ctx) {
  ngx_uint_t i;
  ngx_table_elt_t  **h;
  ngx_c2h5oh_loc_conf_t * alcf = ngx_http_get_module_loc_conf(r, ngx_c2h5oh_module);
  h = r->headers_in.cookies.elts;

  ngx_c2h5oh_query_data_set_len(r, ctx);
  ctx->query.data = ngx_palloc(r->pool, ctx->query.len);
  ctx->query.len = 0;
  ngx_memcpy(ctx->query.data, k_ngx_c2h5oh_select, sizeof(k_ngx_c2h5oh_select));//}', '{}')");
  ctx->query.len = sizeof(k_ngx_c2h5oh_select) - 1; 
  if (alcf->route.len == 0) {
    u_char * p = r->uri.data + 1 + alcf->root.len;
    if (r->uri.len == 1) {
      ngx_memcpy(ctx->query.data + ctx->query.len, "index", sizeof("index") - 1);
    } else {
      while(p < r->uri.data + r->uri.len) {
        if ((*p >= 0x30 && *p <= 0x39) || (*p >= 0x61 && *p <= 0x7a)) {
          *(ctx->query.data + ctx->query.len++) = *p;
        } else if (*p >= 0x41 && *p <= 0x5a) {
          *(ctx->query.data + ctx->query.len++) = *p + 0x20;
        } else if (*p == '/') {
          *(ctx->query.data + ctx->query.len++) = '_';
        }
        p++;
      }
      if (*(ctx->query.data + ctx->query.len - 1) == '_') {
        ctx->query.len--; 
      }
    }
    strcpy((char *)ctx->query.data + ctx->query.len, "('{");
    ctx->query.len += sizeof("('{") - 1; 

  } else {
    ngx_memcpy(ctx->query.data + ctx->query.len, alcf->route.data, alcf->route.len);
    ctx->query.len += alcf->route.len;

    strcpy((char *)ctx->query.data + ctx->query.len, "('");
    ctx->query.len += sizeof("('") - 1; 
    u_char * p = r->uri.data + alcf->root.len;
    while(p < r->uri.data + r->uri.len) {
      *(ctx->query.data + ctx->query.len++) = *p;
      if (*p == '\'') {
        *(ctx->query.data + ctx->query.len++) = *p;
      }
      p++;
    }
    *(ctx->query.data + ctx->query.len++) = '\'';
    *(ctx->query.data + ctx->query.len++) = ',';
    *(ctx->query.data + ctx->query.len++) = '\'';
    *(ctx->query.data + ctx->query.len++) = '{';
  }

  u_char * cookies_start = ctx->query.data + ctx->query.len;
  for(i = 0; i < r->headers_in.cookies.nelts; i++) {
    ngx_c2h5oh_parse_cookies(&ctx->query, &h[i]->value, cookies_start);
  }
  ngx_memcpy(ctx->query.data + ctx->query.len, "}','{", 5);
  ctx->query.len += sizeof("}','{") - 1; 
  u_char * args_start = ctx->query.data + ctx->query.len;
  if (ngx_http_arg(r, (u_char*)"callback", sizeof("callback") - 1, &ctx->callback) != NGX_OK) {
    ctx->callback.len = 0;
  }
  if (ngx_c2h5oh_parse_args(r, &ctx->query, r->args.data, r->args.data + r->args.len, args_start) != 0) {
    return -1;
  }
  if (r->request_body && r->request_body->buf && r->headers_in.content_type && r->request_body_in_single_buf) {
    if (r->request_body_in_single_buf) {
      if (ngx_memcmp(r->headers_in.content_type->value.data, "application/x-www-form-urlencoded", sizeof("application/x-www-form-urlencoded") - 1) == 0) {
        if (r->request_body->buf) {
          if (ngx_c2h5oh_parse_args(r, &ctx->query, r->request_body->buf->pos, r->request_body->buf->last, args_start) != 0) {
            return -1;
          }
        } else {
          ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "[c2h5oh] request_body is null");
        }
      } else {
        // TODO: pass whole body as parameter
      }
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] request_body not in single buf");
    }
  }
  // TODO
  ngx_memcpy(ctx->query.data + ctx->query.len, "}');", sizeof("}');"));
  ctx->query.len += sizeof("}');"); 

  return 0;
}

//-----------------------------------------------------------------------------
ngx_int_t 
ngx_c2h5oh_init_request(ngx_http_request_t * r, ngx_c2h5oh_ctx_t * ctx) 
{
  ngx_http_cleanup_t*         cln;
  if (ngx_c2h5oh_init_query_data(r, ctx) != 0) {
    return NGX_HTTP_BAD_REQUEST;
  }

  // init c2h5oh connection -------------------------------------------------
  if (ctx->conn == NULL) {
    cln = ngx_http_cleanup_add(r, 0);
    if (cln == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] cannot add cleanup");
      return NGX_ERROR;
    }
    cln->handler = ngx_c2h5oh_cleanup;
    cln->data    = ctx;

    ctx->conn = c2h5oh_create();
    if (ctx->conn == NULL) {
      ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
      r->main->count++;
      return NGX_DONE;
    }

    if (c2h5oh_query(ctx->conn, (const char *)ctx->query.data) != 0) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] error create query");
      return NGX_ERROR;
    }
  }

  if (c2h5oh_poll(ctx->conn) == 0) {
    ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
    r->main->count++;
    return NGX_DONE;
  }

  ngx_c2h5oh_post_response(r, ctx);
  return NGX_DONE;
}

//-----------------------------------------------------------------------------
static void
ngx_c2h5oh_post_handler(ngx_http_request_t *r) 
{
  ngx_c2h5oh_ctx_t* ctx;
  ngx_chain_t*      cl;

  r->main->count--;

  ctx = ngx_http_get_module_ctx(r, ngx_c2h5oh_module);

  if (r->request_body != NULL && ctx != NULL) {
    cl  = r->request_body->bufs;
    if (cl != NULL) {
      while(cl != NULL) {
        if (cl == r->request_body->bufs) {
          r->request_body->buf = cl->buf;
        } else {
          size_t old_len  = r->request_body->buf->last - r->request_body->buf->pos;
          size_t next_len = cl->buf->last - cl->buf->pos;
          ngx_buf_t * buf = ngx_create_temp_buf(r->pool, old_len + next_len);
          ngx_memcpy(buf->pos, r->request_body->buf->pos, old_len);
          ngx_memcpy(buf->pos + old_len, cl->buf->pos, next_len);
          buf->last = buf->pos + old_len + next_len;
          r->request_body->buf = buf;
        }
        cl = cl->next;
      }
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] empty request body");
    }
  } else {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "[c2h5oh] null request body");
  }
  ngx_int_t res = ngx_c2h5oh_init_request(r, ctx);
  if (res != NGX_DONE) {
    ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  }
}

//-----------------------------------------------------------------------------
static void
ngx_c2h5oh_event_handler(ngx_event_t * ev)
{
  ngx_time_t * tp;
  ngx_http_request_t * r; 
  ngx_c2h5oh_ctx_t*  ctx; 

  r   = ev->data;
  ctx = ngx_http_get_module_ctx(r, ngx_c2h5oh_module);

  ctx->timer.timedout = 0;
  if (ctx->timer.timer_set) {
    ngx_del_timer(&ctx->timer);
  }

  tp = ngx_timeofday();
  if (tp->sec > ctx->timeout.sec || (tp->sec == ctx->timeout.sec &&
      tp->msec >= ctx->timeout.msec)) 
  {
    if (ctx->conn != NULL) {
      if (c2h5oh_is_error(ctx->conn)) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "[c2h5oh] error after timeout %s", c2h5oh_result(ctx->conn));
      }
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] no free connections available");
    }
    return ngx_http_finalize_request(r, NGX_HTTP_GATEWAY_TIME_OUT);
  }

  if (ctx->conn == NULL) {

    ctx->conn = c2h5oh_create();
    if (ctx->conn == NULL) {
      ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
      return;
    }

    if (c2h5oh_query(ctx->conn, (const char *)ctx->query.data) != 0) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] error create query");
      return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
  }

  if (c2h5oh_poll(ctx->conn) == 0) {
    ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
  } else {
    ngx_c2h5oh_post_response(r, ctx);
  }
}


//-----------------------------------------------------------------------------
static ngx_int_t
ngx_c2h5oh_handler(ngx_http_request_t *r)
{
  ngx_int_t               rc;
  ngx_c2h5oh_ctx_t*       ctx;
  ngx_http_cleanup_t*     cln;
  ngx_c2h5oh_loc_conf_t * alcf;

  ctx = ngx_http_get_module_ctx(r, ngx_c2h5oh_module);

  if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD|NGX_HTTP_POST))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  if (ctx == NULL) {
    ctx = ngx_palloc(r->pool, sizeof(ngx_c2h5oh_ctx_t));
    if (ctx == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] cannot allocate memory for c2h5oh context");
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    bzero(ctx, sizeof(ngx_c2h5oh_ctx_t));

    alcf = ngx_http_get_module_loc_conf(r, ngx_c2h5oh_module);
    ctx->timer.handler = ngx_c2h5oh_event_handler;
    ctx->timer.data    = r;
    ctx->timer.log     = r->connection->log;
    ctx->timeout.msec = (r->start_msec + alcf->timeout) % 1000;
    ctx->timeout.sec  = r->start_sec + (r->start_msec + alcf->timeout) / 1000;

    ngx_http_set_ctx(r, ctx, ngx_c2h5oh_module);
  }

  if (r->method & NGX_HTTP_POST) {
    rc = ngx_http_read_client_request_body(r, ngx_c2h5oh_post_handler);
    if (rc == NGX_AGAIN) {
      r->main->count++;
      return rc;
    }
    if (rc != NGX_OK) {
      r->main->count--;
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] error reading client request body - %d", rc);
      return rc;
    }
    r->main->count++;
    return rc;
  } else {
    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
      return rc;
    }
  }

  if (ngx_c2h5oh_init_query_data(r, ctx) != 0) {
    return NGX_HTTP_BAD_REQUEST;
  }

  if (ctx->conn == NULL) {
    cln = ngx_http_cleanup_add(r, 0);
    if (cln == NULL) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] cannot add cleanup");
      return NGX_ERROR;
    }
    cln->handler = ngx_c2h5oh_cleanup;
    cln->data    = ctx;

    ctx->conn = c2h5oh_create();
    if (ctx->conn == NULL) {
      ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
      r->main->count++;
      return NGX_DONE;
    }

    if (c2h5oh_query(ctx->conn, (const char *)ctx->query.data) != 0) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] error create query");
      return NGX_ERROR;
    }
  }

  if (c2h5oh_poll(ctx->conn) == 0) {
    ngx_add_timer(&ctx->timer, (ngx_msec_t)1);
    r->main->count++;
    return NGX_DONE;
  }

  ngx_c2h5oh_post_response(r, ctx);
  
  return NGX_DONE;
}

//-----------------------------------------------------------------------------
static void 
ngx_c2h5oh_post_response(ngx_http_request_t * r, ngx_c2h5oh_ctx_t * ctx) 
{
  ngx_buf_t    *b;
  ngx_chain_t   out;
  ngx_int_t     rc;

  if (r->method & NGX_HTTP_POST) {
    r->main->count--;
  }

  const char * result_src = c2h5oh_result(ctx->conn);
  int result_len = c2h5oh_result_len(ctx->conn);

  if (result_len <= 0 || result_src == NULL) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "[c2h5oh] empty response: %d", result_len);
    c2h5oh_free(ctx->conn); ctx->conn = NULL;
    return ngx_http_finalize_request(r, NGX_HTTP_NO_CONTENT);
  }

  if (c2h5oh_is_error(ctx->conn)) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "[c2h5oh] query error %s", result_src);
    c2h5oh_free(ctx->conn); ctx->conn = NULL;
    if (result_len > 6 && ngx_memcmp(result_src, "42883_", sizeof("42883_") - 1) == 0) {
      return ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    } else {
      return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
  }

  b = ngx_create_temp_buf(r->pool, result_len + ctx->callback.len + sizeof("();"));
  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "[c2h5oh] allocation error");
    return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  }

  b->pos = b->start + ctx->callback.len + 1; 
  memcpy(b->pos, result_src, result_len);
  b->last = b->pos + result_len;

  jsmn_init(&ngx_c2h5oh_jsmn_parser);

  int i, j;
  int js = JSMN_ERROR_NOMEM;
  u_char * content = NULL;
  int content_length = 0;

  while(js == JSMN_ERROR_NOMEM) {
    js = jsmn_parse(&ngx_c2h5oh_jsmn_parser, (char *)b->pos, result_len, 
                    ngx_c2h5oh_js_tokens, ngx_c2h5oh_js_tokens_count);
    if (js < -1) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] json parse error: invalid json string");
      return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    } else if (js == JSMN_ERROR_NOMEM) {
      ngx_c2h5oh_js_tokens_count *= 3; ngx_c2h5oh_js_tokens_count /= 2;
      void * p = realloc(ngx_c2h5oh_js_tokens, 
                         sizeof(jsmntok_t) * ngx_c2h5oh_js_tokens_count);
      if (p == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "[c2h5oh] error allocating ngx_c2h5oh_js_tokens");
        return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
      } else {
        ngx_c2h5oh_js_tokens = p;
      }
    } else if (js == 0) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] json parse error: empty json");
      return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
  }

  jsmntok_t * t = ngx_c2h5oh_js_tokens;
  if (t->type != JSMN_OBJECT) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "[c2h5oh] json parse error: root has to be an object");
    return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
  }
  int size = t->size;
  for(i = 0; i < size; i++) {
    t++;
    if (ngx_memcmp(b->pos + t->start, "headers", sizeof("headers") - 1) == 0) {
      t++;
      if (t->type == JSMN_ARRAY || t->type == JSMN_STRING) {
        if (t->type == JSMN_STRING) {
          t--;
        }
        int headers_size = t->size;
        for(j = 0; j <  headers_size; j++) {
          t++;
          if (t->type != JSMN_STRING) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "[c2h5oh] json parse error: header has to be a string");
            return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
          }
          if (ngx_memcmp(b->pos + t->start, "Content-Type: ", sizeof("Content-Type:")) == 0) {
            r->headers_out.content_type.len = t->end - t->start - sizeof("Content-Type:");
            r->headers_out.content_type.data = b->pos + t->start + sizeof("Content-Type:");
          } else {
            ngx_table_elt_t * set_header = ngx_list_push(&r->headers_out.headers);
            if (set_header == NULL) {
              ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                            "[c2h5oh] error allocating header");
              return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            }
            set_header->hash = 1;
            set_header->key.data = b->pos + t->start;
            set_header->value.len = t->end - t->start - 2;
            set_header->value.data = b->pos + t->start + 1;
            set_header->key.len = 0;
            for(set_header->key.len = 0; set_header->value.len > 0; set_header->value.len--, set_header->key.len++) {
              set_header->value.data++;
              if (*(set_header->key.data + set_header->key.len) == ':') {
                break;
              }
            }
          }
        }
      } else {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "[c2h5oh] json error: headers has to be array or string");
        return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
      }
    } else if (ngx_memcmp(b->pos + t->start, "content", sizeof("content") - 1) == 0) {
      t++;
      content = b->pos + t->start;
      content_length = t->end - t->start;
      // skip tokens
      int content_size = t->size;
      while(content_size > 0) {
        t++;
        content_size += t->size - 1;
      }
    } else if (ngx_memcmp(b->pos + t->start, "status", sizeof("status") - 1) == 0) {
      t++;
      r->headers_out.status = ngx_atoi(b->pos + t->start, t->end - t->start);
      if (r->headers_out.status <= 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "[c2h5oh] wrong status: [%.*s]", t->end - t->start, b->pos + t->start);
        return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
      }
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
        "[c2h5oh] unexpected token in json: [%*.s]", t->end - t->start, b->pos + t->start);
      return ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
    }
  }

  if (content_length == 0) {
    if (r->headers_out.status == 404) {
      return ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    } else if (r->headers_out.status == 403) {
      return ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
    } else {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "[c2h5oh] empty content");
      return ngx_http_finalize_request(r, NGX_HTTP_NO_CONTENT);
    }
  }

  b->last_buf = 1;
  b->pos = content;
  b->last  = content + content_length;
  if (ctx->callback.len) {
    b->pos -= ctx->callback.len + 1;
    ngx_memcpy(b->pos, ctx->callback.data, ctx->callback.len);
    *(b->pos + ctx->callback.len) = '(';
    *b->last++ = ')';
    *b->last++ = ';';
  }

  out.buf = b;
  out.next = NULL;

  c2h5oh_free(ctx->conn); ctx->conn = NULL;

  if (r->headers_out.content_type.len == 0) {
    r->headers_out.content_type.len = sizeof(ngx_c2h5oh_content_type) - 1;
    r->headers_out.content_type.data = (u_char *)ngx_c2h5oh_content_type;
  }
  if (ctx->callback.len) {
    content_length += ctx->callback.len + sizeof("();") - 1;
  }
  r->headers_out.content_length_n = content_length;
  r->headers_out.last_modified_time = -1;
  if (r->headers_out.status == 0) {
    r->headers_out.status = 200;
  }

  rc = ngx_http_send_header(r);

  if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
    return ngx_http_finalize_request(r, rc);
  }

  rc = ngx_http_output_filter(r, &out);

  ngx_http_finalize_request(r, NGX_HTTP_OK);
}

//-----------------------------------------------------------------------------
static char *
ngx_c2h5oh(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_str_t             *value;
  ngx_c2h5oh_loc_conf_t *alcf = conf;

  if (alcf->enabled) {
    return "is duplicate";
  }
  alcf->enabled = 1;

  value = cf->args->elts;

  if (value[1].data == NULL || value[1].len == 0) {
    return "db_path is not specified";
  }

  alcf->db_path.data = value[1].data;
  alcf->db_path.len  = value[1].len;

  if (value[2].data == NULL || value[2].len == 0) {
    return "pool_size is not specified";
  }

  ngx_int_t pool_size = ngx_atoi(value[2].data, value[2].len);
  if (pool_size <= 0) {
    return "pool size is invalid";
  }

  alcf->pool_size = pool_size;

  if (c2h5oh_module_init((const char *)alcf->db_path.data, 
                                alcf->db_path.len, alcf->pool_size) != 0) 
  {
    ngx_log_error(NGX_LOG_ERR, cf->log, 0, "[c2h5oh] error init c2h5oh");
    return NGX_CONF_ERROR;
  }
    
  ngx_http_core_loc_conf_t  *clcf;

  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_c2h5oh_handler;

  return NGX_CONF_OK;
}

