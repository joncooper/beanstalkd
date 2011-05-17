#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <task.h>
#include <sasl/sasl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "ds.h"

#define CMD_AUTH_LIST_MECHANISMS "auth-list-mechanisms"
#define CMD_AUTH_START "auth-start "
#define CMD_AUTH_STEP "auth-step "

#define CONSTSTRLEN(m) (sizeof(m) - 1)

char *address = "surgingurgency.com";
int port = 11301;

char remote[16];
int rport;

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
  dbgprintf("GFY\n");
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
