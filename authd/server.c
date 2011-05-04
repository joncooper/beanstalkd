#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "ds.h"

void
srvtick(Server *s, int ev)
{
  // prottick(s);
}

void
srvaccept(Server *s, int ev)
{
  h_accept(s->socket.fd, ev, s);
}

void
serve(Server *s)
{
  int r;

  // Set up kqueue;
  // Event loop will run with 10ms timeouts
  // Tick function is srvtick()
  // Tick function input is s, the Srv instance

  sockinit((Handle)srvtick, s, 10*1000000);
  
  // Set up the function to be executed when this Srv's socket has data
  // Function is srvaccept()
  // Function input is s, the Srv instance
  
  s->socket.x = s;
  s->socket.f = (Handle)srvaccept;
  
  // Flag the socket as accepting inbound connections
  
  r = listen(s->socket.fd, 1024);
  if (r == -1) {
    twarn("listen");
    return;
  }
  
  // Register for events upon the presence of readable data
  
  r = sockwant(&s->socket, 'r');
  if (r == -1) {
    twarn("sockwant");
    exit(2);
  }
  
  // Kick off main event loop
  
  sockmain();
  twarnx("sockmain");
  exit(1);
}