// Okay.
// 
// Structure is going to look like this:
//
// Server
//   Socket
//   Connections
//
// ds.h
//   data structures
//
// main.c
//   initialize the Server data structure
//   initialize the Connections data structure
//   start the Server
//
// server.c
//   set up kqueue
//   set up the event loop with 10ms intervalse
//   set the event loop tick function
//   set the 
//
// connections.c
//   define a connection
//   track which connections are dirty
//   keep track of all our connections
// 
// protocol.c
//   update connections with dirty callbacks whenever it makes sense

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sasl/sasl.h>
#include "ds.h"

static char *host_addr = "localhost";
static char *port = "11301";

int
main(int argc, char** argv)
{
  int r;
  Server s = {};
  
  progname = argv[0];
  
  sasl_init();
  sasl_conn_t *sasl_context = sasl_conn_new();
  const char *available_mechanisms = sasl_available_mechanisms(sasl_context);
  
  dbgprintf("sasl_available_mechanisms:\n%s\n", available_mechanisms);
  
  r = make_server_socket(host_addr, port);
  if (r == -1) twarnx("make_server_socket()"), exit(111);
  s.socket.fd = r;
  
  // ms_init(&connections, NULL, NULL);
  
  serve(&s);
  return 0;
}