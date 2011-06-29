#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <task.h>
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

// TODO: do something smart with the bind address
char *address = "localhost";
int port = 11301;
static char *socket_path = "/tmp/beanstalkd.sock";

char remote[16];
int rport;

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

  // TODO: this won't let someone pass a format string as socket_path,
  // but have I missed some other vulnerability? Bloody C strings!
  snprintf(address.sun_path, sizeof(address.sun_path), "%s", socket_path);

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

  set_sig_handlers();

  fd = netannounce(TCP, address, port);
  if (fd < 0) {
//TODO: tighten error message
    printf("aieeee!\n");
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
