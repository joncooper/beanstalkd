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


static sasl_callback_t callbacks[] = {
  // see sasl.h for examples
};

void
sasl_init()
{
  int r;
  r = sasl_server_init(callbacks, progname);
}

sasl_conn_t* 
sasl_conn_new()
{
  int r;
  sasl_conn_t *conn;
  
  // Make a new SASL context
  r = sasl_server_new(service_name,
                      NULL,         // FQDN; NULL means use gethostname()
                      NULL,         // user realm used for password lookups
                      NULL, NULL,   // IP address information strings
                      NULL,         // callbacks supported only for this conn
                      0,            // security flags
                      &conn);

  if (r == -1) {
    twarn("sasl_server_new");
    exit(1);
  }
  
  return conn;
}

const char *
sasl_available_mechanisms(sasl_conn_t *conn)
{
  int r;
  const char *result_string;
  unsigned string_length;
  int number_of_mechanisms;
  
  r = sasl_listmech(conn,                   // SASL contxt
                    NULL,                   // not supported
                    "{", ", ", "}",         // format the output
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
sasl_auth_start(sasl_conn_t *conn, const char *mechanism, const char *clientin)
{
  int r;
  const char *out;
  unsigned outlen;
  
  r = sasl_server_start(conn,
                        mechanism,
                        clientin,
                        sizeof(clientin),
                        &out,         // library output; may not be NULL-terminated
                        &outlen);
  
  if ((r != SASL_OK) && (r != SASL_CONTINUE)) {
    // failure. send protocol specific message saying authentication failed.
  } else if (result == SASL_OK) {
    // authentication succeeded. send client the protocol specific message saying that authentication is complete.
  } else {
    // should be SASL_CONTINUE
    // send data 'out' with length 'outlen' over the network in protocol specific format
  }
}

void
sasl_auth_step(sasl_conn_t *conn, const char *clientin)
{
  int r;
  const char *out;
  unsigned outlen;
  
  r = sasl_server_step(conn,
                       clientin,
                       sizeof(clientin),
                       &out,         // library output; may not be NULL-terminated
                       &outlen);
  
  if ((r != SASL_OK) && (r != SASL_CONTINUE)) {
    // failure. send protocol specific message saying authentication failed.
  } else if (result == SASL_OK) {
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