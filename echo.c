// Needs to be linked with:
//
// ms.c
// net.c
// sock-bsd.c

#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 1

typedef unsigned char uchar;
typedef uchar         byte;
typedef unsigned int  uint;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;

typedef struct Connection *Connection;
typedef struct ms     *ms;
typedef struct job    *job;
typedef struct tube   *tube;
typedef struct conn   *conn;
typedef struct Heap   Heap;
typedef struct Socket Socket;
typedef struct Srv    Srv;

typedef void(*evh)(int, short, void *);
typedef void(*ms_event_fn)(ms a, void *item, size_t i);
typedef void(*Handle)(void*, int);

struct Socket {
    int    fd;
    Handle f;
    void   *x;
    int    added;
};

void sockinit(Handle tick, void *x, int64 ns);
int  sockwant(Socket *s, int rw);
void sockmain(); // does not return

struct Srv {
    Socket sock;
    Heap   conns;
};

struct ms {
    size_t used, cap, last;
    void **items;
    ms_event_fn oninsert, onremove;
};

struct Connection {
  Srv *srv;
  Socket sock;
  char state; // in terms of our protocol's state machine
  int rw;
}



static char *host_addr = "localhost";
static char *port = "11301";

struct ms connections;

// typedef void(*evh)(int, short, void *);
// typedef void(*ms_event_fn)(ms a, void *item, size_t i);
// typedef void(*Handle)(void*, int);

// TODO: STATE_START
// TODO: prothandle
// TODO: update_conns

static Connection
conn_alloc()
{
  Connection c;
  c = malloc(sizeof(struct Connection));
  return memset(c, 0, sizeof *c);
}

static void
conn_free(Connection c)
{
  free(c);
}

Connection make_conn(int fd, char start_state)
{
  Connection c;
  c = conn_alloc();
  if (!c) return twarn("out of memory"), (Connection) 0;
  
  if (!ms_append(&connections, c)) {
    conn_free(c);
    return twarn("out of memory"), (Connection) 0;
  }
    
  c->sock.fd = fd;
  c->state = start_state;
  return c;
}

void
h_accept(const int fd, const short which, Srv *s)
{
  Connection c;
  int cfd, flags, r;
  socklen_t addrlen;
  struct sockaddr_in6 addr;

  addrlen = sizeof addr;
  cfd = accept(fd, (struct sockaddr *)&addr, &addrlen);
  if (cfd == -1) {
      if (errno != EAGAIN && errno != EWOULDBLOCK) twarn("accept()");
      update_conns();
      return;
  }
  
  flags = fcntl(cfd, F_GETFL, 0);
  if (flags < 0) {
      twarn("getting flags");
      close(cfd);
      update_conns();
      return;
  }

  r = fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
  if (r < 0) {
      twarn("setting O_NONBLOCK");
      close(cfd);
      update_conns();
      return;
  }
  
  c = make_conn(cfd, STATE_START);
  if (!c) {
      twarnx("make_conn() failed");
      close(cfd);
      update_conns();
      return;
  }
  c->srv = s;
  c->sock.x = c;
  c->sock.f = (Handle)prothandle;
  c->sock.fd = cfd;

  dbgprintf("accepted conn, fd=%d\n", cfd);
  r = sockwant(&c->sock, 'r');
  if (r == -1) {
      twarn("sockwant");
      close(cfd);
      update_conns();
      return;
  }
  update_conns();
}

void
srvaccept(Srv *s, int ev)
{
  h_accept(s->sock.fd, ev, s);
}

void
srvtick(Srv *s, int ev)
{
  // prottick(s);
}

void
srv(Srv *s)
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
  
  s->sock.x = s;
  s->sock.f = (Handle)srvaccept;
  
  // Flag the socket as accepting inbound connections
  
  r = listen(s->sock.fd, 1024);
  if (r == -1) {
    twarn("listen");
    return;
  }
  
  // Register for events upon the presence of readable data
  
  r = sockwant(&s->sock, 'r');
  if (r == -1) {
    twarn("sockwant");
    exit(2);
  }
  
  // Kick off main event loop
  
  sockmain();
  twarnx("sockmain");
  exit(1);
}

int
main(int argc, char** argv)
{
  int r;
  Srv s = {};
  
  r = make_server_socket(host_addr, port);
  if (r == -1) twarnx("make_server_socket()"), exit(111);
  s.sock.fd = r;
  
  ms_init(&connections, NULL, NULL);
  
  srv(&s);
  return 0;
}

