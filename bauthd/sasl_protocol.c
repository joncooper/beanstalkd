#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <task.h>
#include <string.h>
#include <sasl/sasl.h>
#include "ds.h"

tTransition transitions[] = {
  { ST_SASL_READY,    EV_SASL_LIST_MECHANISMS,  &sasl_list_mechanisms },
  { ST_SASL_READY,    EV_SASL_START,            &sasl_start           },
  { ST_SASL_CONTINUE, EV_SASL_STEP,             &sasl_step            },
  { ST_ANY,           EV_ERROR,                 &fsm_error            },
  { ST_ANY,           EV_ANY,                   &fsm_error            }  // Wildcards at the end.
};

#define TRANSITION_COUNT (sizeof(transitions) / sizeof(*transitions))

#define MSG_AUTH_OK "AUTH_OK\r\n"
#define MSG_AUTH_FAILURE "AUTH_FAILURE\r\n"
#define MSG_AUTH_ERROR "AUTH_ERROR\r\n"
#define MSG_AUTH_CONTINUE "AUTH_CONTINUE %i\r\n"
#define MSG_AUTH_LIST_MECHANISMS "AUTH_MECHANISMS: %s\r\n"
#define CONSTSTRLEN(m) (sizeof(m) - 1)

char *service_name = "bauthd";

static sasl_callback_t sasl_callbacks[] = {
  { SASL_CB_LIST_END, NULL, NULL }
};

void
sasl_init()
{
  int r;
  r = sasl_server_init(sasl_callbacks, service_name);
  
  if (r != SASL_OK) {
    twarn("sasl_init()");
    taskexitall(-1);
  }
}

void
sasl_free()
{
  sasl_done();
}

sasl_conn_t*
sasl_conn_new()
{
  int r;
  sasl_conn_t *sasl_conn;
  
  // Make a new SASL context
  r = sasl_server_new(service_name,
                      NULL,         // FQDN; NULL means use gethostname()
                      NULL,         // user realm used for password lookups
                      NULL, NULL,   // IP address information strings
                      NULL,         // callbacks supported only for this conn
                      0,            // security flags
                      &sasl_conn);

  if (r == -1) {
    twarn("sasl_conn_new()");
    taskexitall(-1);
  }
  
  return sasl_conn;
}

void
sasl_conn_free(sasl_conn_t *sasl_conn)
{
  sasl_dispose(&sasl_conn);
}

int
sasl_list_mechanisms(Connection conn)
{
  int r;
  const char *result_string;
  unsigned int string_length;
  int number_of_mechanisms;
  
r = sasl_listmech(conn->sasl_conn,        // SASL context
                  NULL,                   // not supported
                  "", " ", "",            // format the output
                  &result_string,         // list of mechanisms as string
                  &string_length,         // etc..
                  &number_of_mechanisms);
  
  if (r == -1) {
    twarn("sasl_listmech");
    fsm_error(conn);
  }
  
  dbgprintf("sasl_list_mechanisms: %s\n", result_string);

  fdwrite(conn->fd, "AUTH_MECHANISMS: ", strlen("AUTH_MECHANISMS: "));
  fdwrite(conn->fd, result_string, string_length);
  
  return ST_SASL_READY;
}

int
sasl_start(Connection conn)
{
  // parse the command string for parameters
  // we expect:
  //   auth-start <mechanism> <bytes>\r\n
  //   <auth data>\r\n
  char *loc, *mechanism, *bytes;
  int bytes_to_read, r;
 
  loc = strchr(conn->command, ' ');
  if (loc == NULL) fsm_error(conn);
  
  mechanism = loc + 1;
  loc = strchr(mechanism, ' ');
  if (loc == NULL) fsm_error(conn);
  
  *loc = '\0';
  bytes = loc + 1;
  
  dbgprintf("mechanism: %s, bytes: %s\n", mechanism, bytes);
    
  // we assume that anything excess written with the command has been put in conn->data_in and its data_in_loc has been set, but not data_in_len.
  // read the rest of the data if need be
  
  bytes_to_read = (int)strtol(bytes, (char **)NULL, 10);

  dbgprintf("bytes_to_read: %i\n", bytes_to_read);

  conn->data_in_len = bytes_to_read;
  conn_read(conn);
 
  // throw to sasl
  r = sasl_server_start(conn->sasl_conn, mechanism, conn->data_in, conn->data_in_len, &(conn->data_out), &(conn->data_out_len));

  conn->data_in[conn->data_in_len] = '\0';
  dbgprintf("data_in: %s\n", conn->data_in);

  if ((r != SASL_OK) && (r != SASL_CONTINUE)) {
    dbgprintf("ST_SASL_FAIL\n");
    // if you are debugging, split error from fail here..
    return ST_SASL_FAIL;
  } else if (r == SASL_OK) {
    dbgprintf("ST_SASL_OK\n");
    fdwrite(conn->fd, MSG_AUTH_OK, CONSTSTRLEN(MSG_AUTH_OK));    
    return ST_SASL_OK;
  } else {
    // should be SASL_CONTINUE
    fprintf(conn->fd, MSG_AUTH_CONTINUE, conn->data_out_len);
    conn_write(conn);
    return ST_SASL_CONTINUE;
  }
}

int
sasl_step(Connection conn)
{
  dbgprintf("sasl_step()\n");
  return ST_SASL_READY;
}

int
fsm_error(Connection conn)
{
  dbgprintf("fsm_error()");
  conn_close(conn);
  return ST_SASL_FAIL;
}

void
sasl_fsm_dispatch(Connection conn, int event)
{
  int i;
  for (i = 0; i < TRANSITION_COUNT; i++) {
    if ((conn->state == transitions[i].state) || (ST_ANY == transitions[i].state)) {
      if ((event == transitions[i].event) || (EV_ANY == transitions[i].event)) {
        conn->state = (transitions[i].transition_function)(conn);
        break;
      }
    }
  }
}
