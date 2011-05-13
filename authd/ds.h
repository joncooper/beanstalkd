#include <sasl/sasl.h>

typedef unsigned char uchar;
typedef uchar         byte;
typedef unsigned int  uint;
typedef int32_t       int32;
typedef uint32_t      uint32;
typedef int64_t       int64;
typedef uint64_t      uint64;

#define int32_t  do_not_use_int32_t
#define uint32_t do_not_use_uint32_t
#define int64_t  do_not_use_int64_t
#define uint64_t do_not_use_uint64_t

/* A command can be at most LINE_BUF_SIZE chars, including "\r\n". This value
 * MUST be enough to hold the longest possible command or reply line, which is
 * currently "USING a{200}\r\n". */
#define LINE_BUF_SIZE 208

#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)
#define twarnx(fmt, args...) warnx("%s:%d in %s: " fmt, \
                                   __FILE__, __LINE__, __func__, ##args)

#ifdef DEBUG
#define dbgprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dbgprintf(fmt, ...) ((void) 0)
#endif

typedef struct Server Server;
typedef struct Connection *Connection;
typedef struct Socket Socket;

typedef void(*evh)(int, short, void *);
typedef void(*Handle)(void*, int);

struct Socket {
  int fd;
  Handle f;
  void *x;
  int added;
};

struct Connection {
  Connection prev, next;
  Socket socket;
  char state; // in terms of our protocols' state machine
  int rw;
  
  char cmd[LINE_BUF_SIZE]; // this is not NULL-terminated
  int cmd_len;
  int cmd_read;
  const char *reply;
  int reply_len;
  int reply_sent;
  char reply_buf[LINE_BUF_SIZE]; // this IS NULL-terminated
  
  sasl_conn_t *sasl_conn;
  
  char *auth_data_in;
  int auth_data_in_len;
  int auth_data_in_read;
  
  char *auth_data_out_lenut;
  int auth_data_out_len;
  int auth_data_out_sent;
};

struct Server {
  Socket socket;
  Connection connections;
};

extern const char *progname;

Connection make_conn(int fd, char start_state);
void conn_close(Connection c);

Connection conn_remove(Connection c);
void conn_insert(Connection head, Connection c);

void connwant(Connection c, int rw, Connection list);

void sockinit(Handle tick, void *x, int64 ns);
int  sockwant(Socket *s, int rw);
void sockmain(); // does not return

void h_accept(const int fd, const short which, Server* s);

int make_server_socket(char *host_addr, char *port);

void serve(Server *srv);
void srvaccept(Server *s, int ev);
void srvtick(Server *s, int ev);

void warn(const char *fmt, ...);
void warnx(const char *fmt, ...);

int64 nanoseconds();

void sasl_init();
sasl_conn_t* sasl_conn_new();
const char *sasl_available_mechanisms(sasl_conn_t *sasl_context);
void sasl_auth_start(sasl_conn_t *conn, const char *mechanism, const char *clientin, unsigned int clientinlen, const char *serverout, unsigned int serveroutlen);
void sasl_auth_step(sasl_conn_t *conn, const char *clientin, unsigned int clientinlen, const char *serverout, unsigned int serveroutlen);
void sasl_conn_free();

