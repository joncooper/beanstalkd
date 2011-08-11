#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "libtask/task.h"
#include <sasl/sasl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include "ds.h"

#define CMD_AUTH_LIST_MECHANISMS "auth-list-mechanisms"
#define CMD_AUTH_START "auth-start "
#define CMD_AUTH_STEP "auth-step "

#define CONSTSTRLEN(m) (sizeof(m) - 1)

/* extern const char *progname;*/
const char *progname = "bauthd";

static char *address = "localhost";
static int port = 11301;
static char *socket_path = "/tmp/beanstalkd.sock";

static void
usage(char *msg, char *arg)
{
  if (arg) warnx("%s: %s", msg, arg);
  fprintf(stderr, "Use: %s [OPTIONS]\n"
      "\n"
      "Options:\n"
      " -A PATH  listen on socket bound to PATH"
      " -p PORT  listen on port (default is 11301"
      " -l ADDR  listen on address (default is localhost)\n"
      " -h",
      progname);
  exit(arg ? 5 : 0);
}

static char *
require_arg(char *opt, char *arg)
{
  if (!arg) usage("option requires an argument", opt);
  return arg;
}

static int
parse_port(char *str)
{
  int r, intvalue;
  r = sscanf(str, "%i", &intvalue);
  if (1 != r) usage("invalid port", str);
  return intvalue;
}


static void
opts(int argc, char **argv)
{
  int i;
  for (i = 1; i < argc; ++i) {
    if (argv[i][0] != '-') usage("unknown option", argv[i]);
    if (argv[i][1] == 0 || argv[i][2] != 0) usage("unknown option", argv[i]);
    switch (argv[i][1]) {
      case 'A':
        socket_path = require_arg("-A", argv[++i]);
        break;
      case 'p':
        port = parse_port(require_arg("-p", argv[++i]));
        break;
      case 'l':
        address = require_arg("-l", argv[++i]);
        break;
      case 'h':
        usage(NULL, NULL);
      default:
        usage("unknown option", argv[i]);
    }
  }
}

// With thanks to "vpj" --
// http://vpj.posterous.com/passing-file-descriptors-between-processes-us

int
send_fd(int socket, int fd_to_send)
{
	struct msghdr message;
	struct iovec iov[1];
	struct cmsghdr *control_message = NULL;
    char ctrl_buf[CMSG_SPACE(sizeof(int))];
	char data[1];
	
	memset(&message, 0, sizeof(struct msghdr));
    memset(ctrl_buf, 0, CMSG_SPACE(sizeof(int)));
	
	data[0] = ' ';
	iov[0].iov_base = data;
	iov[0].iov_len = sizeof(data);

	message.msg_name = NULL;
	message.msg_namelen = 0;
	message.msg_iov = iov;
	message.msg_iovlen = 1;
    message.msg_controllen = CMSG_SPACE(sizeof(int));
    message.msg_control = ctrl_buf;

    control_message = CMSG_FIRSTHDR(&message);
	control_message->cmsg_level = SOL_SOCKET;
	control_message->cmsg_type = SCM_RIGHTS;
    control_message->cmsg_len = CMSG_LEN(sizeof(int));

    *((int *) CMSG_DATA(control_message)) = fd_to_send;

	return sendmsg(socket, &message, 0);
}

int
make_local_client_socket(char *socket_path)
{
  int fd = -1, r, flags;
  struct sockaddr_un address;
  
  fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if (fd == -1)
    return twarn("socket()"), -1;

  memset(&address, 0, sizeof(struct sockaddr_un));
  address.sun_family = AF_UNIX;

  if (sizeof(socket_path) <= sizeof(address.sun_path)) {
    memmove(address.sun_path, socket_path, sizeof(socket_path));
  } else {
    twarn("bad socket path");
    return -1;
  }

	r = connect(fd, (struct sockaddr *) &address, sizeof(address));
	if (r == -1)
		return twarn("connect()"), -1;

	// See http://www.tin.org/bin/man.cgi?section=7&topic=AF_LOCAL
  // for more info on socket options (or the lack thereof)

  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    twarn("getting flags");
    close(fd);
    return -1;
  }

  r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) {
    twarn("setting O_NONBLOCK");
    close(fd);
    return -1;
  }

	return fd;
}

void
handoff_fd(int fd)
{
    int r, beanstalkd_sock;
    beanstalkd_sock = make_local_client_socket(socket_path);
    r = send_fd(beanstalkd_sock, fd);
    if (r == -1) {
        twarn("handoff_fd()");
    }
}

#define TEST_CMD(s,c,o) if (strncmp((s), (c), CONSTSTRLEN(c)) == 0) return (o);
static int
which_event(char *line)
{
  TEST_CMD(line, CMD_AUTH_LIST_MECHANISMS, EV_SASL_LIST_MECHANISMS);
  TEST_CMD(line, CMD_AUTH_START, EV_SASL_START);
  TEST_CMD(line, CMD_AUTH_STEP, EV_SASL_STEP);
  return EV_ERROR;
}

void
bauthdtask(void *v)
{
  int cfd = (int)v;
  int event;
  int r;
  Connection conn;

  conn = conn_init();
  conn->fd = cfd;

  for (;;) {
    r = conn_read_command(conn);
    if (r == -1) {
      dbgprintf("could not conn_read_command()\n");
      taskexit(-1);
    }
    dbgprintf("command: %s\n", conn->command);

    event = which_event(conn->command);
    sasl_fsm_dispatch(conn, event);
    if (conn->state == ST_SASL_OK) {
      dbgprintf("authentication succeeded\n");
      handoff_fd(conn->fd);
    } else if (conn->state == ST_SASL_FAIL) {
      dbgprintf("authentication failed.\n");
    }
    conn_close(conn);
    taskexit(0);
  }
}

static void
exit_cleanly()
{
  dbgprintf("bauthd: exiting\n");
  sasl_free();
  taskexitall(0);
}

static void
set_sig_handlers()
{
  int r;
  struct sigaction sa;

  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  r = sigemptyset(&sa.sa_mask);
  if (r == -1) twarn("sigemptyset()"), exit(111);
  
  r = sigaction(SIGPIPE, &sa, 0);
  if (r == -1) twarn("sigaction(SIGPIPE)"), exit(111);
  
  sa.sa_handler = exit_cleanly;
  r = sigaction(SIGINT, &sa, 0);
  if (r == -1) twarn("sigaction(SIGINT)"), exit(111);
  
  sa.sa_handler = exit_cleanly;
  r = sigaction (SIGTERM, &sa, 0);
  if (r == -1) twarn("sigaction(SIGTERM)"), exit(111);
}

void
taskmain(int argc, char **argv)
{
  int cfd, fd;
  char remote[16];
  int rport;

  set_sig_handlers();

  fd = netannounce(TCP, address, port);
  if (fd < 0) {
    fprintf(stderr, "Could not bind to %s:%i. Exiting.\n", address, port);
    taskexitall(1);
  }
  
  fdnoblock(fd);
  
  sasl_init();
    
  while (1) {
    cfd = netaccept(fd, remote, &rport);
    fdnoblock(cfd);
    if (cfd < 0) {
      exit(1);
    }
    taskcreate(bauthdtask, (void*)cfd, STACK_SIZE);
    // no need to yield given blocking semantics of netaccept().
  }
}
