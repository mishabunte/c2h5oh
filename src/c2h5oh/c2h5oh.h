#ifndef __c2h5oh_c_h_included__
#define __c2h5oh_c_h_included__

#include <stdint.h> 
#include <stdlib.h> 

//-----------------------------------------------------------------------------
// c2h5oh C interface

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

//-----------------------------------------------------------------------------
struct c2h5oh; // c2h5oh forward declaration
typedef struct c2h5oh c2h5oh_t; // c2h5oh handle


//-----------------------------------------------------------------------------

/** 
 * init c2h5oh_module 
 * @param conn_string       Database connection string
 * @param str_len           Database connection string length
 * @param connections_count Connections pool size
 * @return 0 if succeeded, -1 otherwise
 */
int c2h5oh_module_init(const char * conn_string, size_t str_len, 
                       uint16_t connections_count);

/** 
 * Clean up c2h5oh module 
 */
void c2h5oh_module_cleanup();


//-----------------------------------------------------------------------------

/**
 * Get next available c2h5oh connection 
 * Connection has to be free by c2h5oh_free
 * @returns NULL on error, c2h5oh connection otherwise
 */
c2h5oh_t * c2h5oh_create();

/**
 * Free c2h5oh connection
 * @param c     c2h5oh connection
 */
void c2h5oh_free(c2h5oh_t * c);


//-----------------------------------------------------------------------------

/** 
 * Perform query 
 * @param c     c2h5oh connection
 * @param query query to execute, null terminated
 * @return Return 0 if succeeded, -1 on error
 */ 
int c2h5oh_query(c2h5oh_t * c, const char * query);

/**
 * Poll c2h5oh connection, client must call it while result is ready
 * @param  c c2h5oh connection
 * @return 1 if query completed and result is ready, 0 otherwise, -1 error fatal
 */
int c2h5oh_poll(c2h5oh_t * c);


//-----------------------------------------------------------------------------

/**  
 * Returns result 
 * @param  c c2h5oh connection
 */
const char * c2h5oh_result(c2h5oh_t * c);

/** 
 * Returns result len 
 * @param  c c2h5oh connection
 */
size_t       c2h5oh_result_len(c2h5oh_t * c);

/** Returns 0 if result is not error */
int c2h5oh_is_error(c2h5oh_t * c);

//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif//__cplusplus
#endif//__c2h5oh_h_included__
