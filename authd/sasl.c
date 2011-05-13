#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <sasl/sasl.h>
#include "ds.h"

// Plan to support only shared-secret login schemes such that saslpasswd2
// can be used to administer users.

static const char *service_name = "beanstalk";

// Lifecycle:
//   first, call sasl_server_init
//   then, on each new connection, call sasl_server_new at accept() time
//   get a list of supported mechanisms with sasl_listmech
//   when the client requests to authenticate, sasl_server_start, then
//      repeatedly hit sasl_server_step
//      you'll get SASL_INTERACT, SASL_OK or SASL_CONTINUE; 
//         anything else means an error.
//   when the connection is concluded, make a call to sasl_dispose
//   finish by calling sasl_done (on global shutdown)

// See SASL commits to memcached:
//   https://github.com/memcached/memcached/commit/f1307c4d9cadb94076a99cc2f88a00f7e0b4161f

static sasl_callback_t callbacks[] = {
  { SASL_CB_LIST_END, NULL, NULL }
};

void
sasl_init()
{
  int r;
  r = sasl_server_init(callbacks, progname);
}


  
  return conn;
sasl_conn_new()
{
  
}

const char *
sasl_available_mechanisms(sasl_conn_t *conn)
{
  int r;
  const char *result_string;
  unsigned int string_length;
  int number_of_mechanisms;
  
  r = sasl_listmech(conn,                   // SASL contxt
                    NULL,                   // not supported
                    "", " ", "",            // format the output
                    &result_string,         // list of mechanisms as string
                    &string_length,         // etc..
                    &number_of_mechanisms);
  
  if (r == -1) {
    twarn("sasl_listmech");
    exit(1);
  }
  
  return result_string;
}

// TODO: auth_{start, step} need to return out & outlen 'coz they might not be NULL-terminated. 
// TODO: similarly they should not use sizeof() as the input might not be NULL-terminated either.

void
sasl_auth_start(sasl_conn_t *conn, const char *mechanism, const char *clientin, unsigned int clientinlen, const char *serverout, unsigned int serveroutlen)
{
  int r;
  
  r = sasl_server_start(conn,
                        mechanism,
                        clientin,
                        sizeof(clientin),
                        &serverout,         // library output; may not be NULL-terminated
                        &serveroutlen);
  
  if ((r != SASL_OK) && (r != SASL_CONTINUE)) {
    // failure. send protocol specific message saying authentication failed.
  } else if (r == SASL_OK) {
    // authentication succeeded. send client the protocol specific message saying that authentication is complete.
  } else {
    // should be SASL_CONTINUE
    // send data 'out' with length 'outlen' over the network in protocol specific format
  }
}

void
sasl_auth_step(sasl_conn_t *conn, const char *clientin, unsigned int clientinlen, const char *serverout, unsigned int serveroutlen)
{
  int r;
  
  r = sasl_server_step(conn,
                       clientin,
                       sizeof(clientin),
                       &serverout,         // library output; may not be NULL-terminated
                       &serveroutlen);
  
  if ((r != SASL_OK) && (r != SASL_CONTINUE)) {
    // failure. send protocol specific message saying authentication failed.
  } else if (r == SASL_OK) {
    // authentication succeeded. send client the protocol specific message saying that authentication is complete.
  } else {
    // should be SASL_CONTINUE
    // send data 'out' with length 'outlen' over the network in protocol specific format
  }
}

void
sasl_conn_free(sasl_conn_t *conn)
{
  sasl_dispose(&conn);
}