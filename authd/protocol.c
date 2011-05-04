#define CMD_AUTH_LIST_MECHANISMS "auth-list-mechanisms"
#define CMD_AUTH_START "auth-start "
#define CMD_AUTH_STEP "auth-step "

#define CONSTSTRLEN(m) (sizeof(m) - 1)
#define CMD_AUTH_LIST_MECHANISMS_LEN CONSTSTRLEN(CMD_AUTH_LIST_MECHANISMS)
#define CMD_AUTH_START_LEN CONSTSTRLEN(CMD_AUTH_START)
#define CMD_AUTH_STEP_LEN CONSTSTRLEN(CMD_AUTH_STEP)

#define MSG_AUTH_OK "AUTH_OK\r\n"
#define MSG_AUTH_UNAUTHORIZED "AUTH_UNAUTHORIZED\r\n"
#define MSG_AUTH_CONTINUE "AUTH_CONTINUE\r\n"
#define MSG_UNKNOWN_COMMAND "UNKNOWN_COMMAND\r\n"
#define MSG_INTERNAL_ERROR "INTERNAL_ERROR\r\n"
#define MSG_BAD_FORMAT "BAD_FORMAT\r\n"

#define STATE_WANTCOMMAND 0
#define STATE_WANTDATA 1
#define STATE_SENDWORD 2
#define STATE_SENDDATA 3

#define OP_UNKNOWN 0
#define OP_AUTH_LIST_MECHANISMS 1
#define OP_AUTH_START 2
#define OP_AUTH_STEP 3
#define TOTAL_OPS 4

#define MECHANISM_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
                        "0123456789-_"

#ifdef DEBUG
static const char * op_names[] = {
  "<unknown>",
  CMD_AUTH_LIST_MECHANISMS,
  CMD_AUTH_START,
  CMD_AUTH_STEP
};
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "ds.h"

// Doubly linked list of dirty connections

static struct Connection dirty = {&dirty, &dirty};

// TODO: consider connection pooling to avoid malloc()/free() overhead.

static void
update_conns()
{
  int r;
  Connection c;
  
  while ((c = conn_remove(dirty.next))) {
    r = sockwant(&c->socket, c->rw);
    if (r == -1) {
      twarn("sockwant");
      conn_close(c);
    }
  }
}

static void
check_err(Connection c, const char *s)
{
  if (errno == EAGAIN) return;
  if (errno == EINTR) return;
  if (errno == EWOULDBLOCK) return;

  twarn("%s", s);
  conn_close(c);
  return;
}

/* Scan the given string for the sequence "\r\n" and return the line length.
 * Always returns at least 2 if a match is found. Returns 0 if no match. */
static int
scan_line_end(const char *s, int size)
{
    char *match;

    match = memchr(s, '\r', size - 1);
    if (!match) return 0;

    /* this is safe because we only scan size - 1 chars above */
    if (match[1] == '\n') return match - s + 2;

    return 0;
}

static int
cmd_len(Connection c)
{
    return scan_line_end(c->cmd, c->cmd_read);
}

static void
reply(Connection c, const char *line, int len, int state)
{
    if (!c) return;

    connwant(c, 'w', &dirty);
    c->reply = line;
    c->reply_len = len;
    c->reply_sent = 0;
    c->state = state;
    dbgprintf("sending reply: %.*s", len, line);
}

#define reply_msg(c,m) reply((c),(m),CONSTSTRLEN(m),STATE_SENDWORD)

#define reply_serr(c,e) (twarnx("server error: %s",(e)), reply_msg((c),(e)))

static void
reply_line(Connection c, int state, const char *fmt, ...)
{
  int r;
  va_list ap;
  
  va_start(ap, fmt);
  r = vsnprintf(c->reply_buf, LINE_BUF_SIZE, fmt, ap);
  va_end(ap);
  
  /* Make sure the buffer was big enough. If not, we have a bug. */
  if (r >= LINE_BUF_SIZE) return reply_serr(c, MSG_INTERNAL_ERROR);
  
  return reply(c, c->reply_buf, r, state);
}

static void
reset_conn(Connection c)
{
  connwant(c, 'r', &dirty);
  c->reply_sent = 0; /* now that we're done, reset this */
  c->state = STATE_WANTCOMMAND;
  
  dbgprintf("reset_conn()\n");
}

static int
which_cmd(Connection c)
{
#define TEST_CMD(s,c,o) if (strncmp((s), (c), CONSTSTRLEN(c)) == 0) return (o);
  TEST_CMD(c->cmd, CMD_AUTH_LIST_MECHANISMS, OP_AUTH_LIST_MECHANISMS);
  TEST_CMD(c->cmd, CMD_AUTH_START, OP_AUTH_START);
  TEST_CMD(c->cmd, CMD_AUTH_STEP, OP_AUTH_STEP);
  return OP_UNKNOWN;
}

static int
read_uint_param(uint *param, const char *buf, char **end)
{
  char *tend;
  uint tparam;
  
  errno = 0;
  while (buf[0] == ' ') buf++;
  if (!isdigit(buf[0])) return -1;
  tparam = strtoul(buf, &tend, 10);
  if (tend == buf) return -1;
  if (errno && errno != ERANGE) return -1;
  if (!end && tend[0] != '\0') return -1;
  
  if (param) *param = tparam;
  if (end) *end = tend;
  return 0;
}

// Grab the mechanism out of buf and deposit into *mechanism.
// Point *end at the address after the last character consumed
static int
read_mechanism(char **mechanism, char *buf, char **end)
{
  size_t len;
  while (buf[0] == ' ') buf++;
  len = strspn(buf, MECHANISM_CHARS);
  if (len == 0) return -1;
  
  char *ret = malloc(sizeof(char) * len);
  strncpy(ret, buf, len);
  
  if (mechanism) *mechanism = ret;
  if (end) *end = buf + len;
  return 0;
}

static void 
dispatch_cmd(Connection c)
{
  int r;
  byte type;
  const char *available_mechanisms;
  char *size_buf, *end_buf;
  char *mechanism;
  uint data_size;
  
  /* NUL-terminate this string so we can use strtol and friends */
  c->cmd[c->cmd_len - 2] = '\0';

  /* check for possible maliciousness */
  if (strlen(c->cmd) != c->cmd_len - 2) {
      return reply_msg(c, MSG_BAD_FORMAT);
  }

  type = which_cmd(c);
  dbgprintf("got %s command: \"%s\"\n", op_names[(int) type], c->cmd);

  switch(type) {
  case OP_AUTH_LIST_MECHANISMS:
    /* disallow trailing garbage */
    if (c->cmd_len != CMD_AUTH_LIST_MECHANISMS_LEN + 2) {
      return reply_msg(c, MSG_BAD_FORMAT);
    }
    c->sasl_conn = sasl_conn_new();
    available_mechanisms = sasl_available_mechanisms(c->sasl_conn);
    reply_line(c, STATE_SENDWORD, "AUTH %s\r\n", available_mechanisms);
    break;
  case OP_AUTH_START:
    r = read_mechanism(&mechanism, c->cmd + CMD_AUTH_START_LEN, &size_buf);
    if (r) return reply_msg(c, MSG_BAD_FORMAT);
    dbgprintf("OP_AUTH_START: mechanism %s\n", mechanism);
    
    // *size_buf now points to the location of the <bytes> parameter
    r = read_uint_param(&data_size, size_buf, &end_buf);
    if (r) return reply_msg(c, MSG_BAD_FORMAT);
    
    // *end_buf now points to the null terminator
    if (end_buf[0] != '\0') {
      dbgprintf("dispatch_cmd: bad *end_buf.");
      return reply_msg(c, MSG_BAD_FORMAT);
    }
    break;
  case OP_AUTH_STEP:
    break;
  default:
    return reply_msg(c, MSG_UNKNOWN_COMMAND);
    break;
  }
}

static void
fill_extra_data(Connection c)
{
  int extra_bytes;
  
  if (!c->socket.fd) return; /* the connection was closed */
  if (!c->cmd_len) return; /* we don't have a complete command */
  
  /* how many extra bytes did we read? */
  extra_bytes = c->cmd_read - c->cmd_len;
  
  /* see prot.c - copy data somewhere the SASL auth can find it */
  
  c->cmd_read = 0;
  c->cmd_len = 0;
}

static void
do_cmd(Connection c)
{
  dispatch_cmd(c);
  fill_extra_data(c); 
}

static void
conn_data(Connection c)
{
  int r;
  
  switch (c->state) {
  case STATE_WANTCOMMAND:
    r = read(c->socket.fd, c->cmd + c->cmd_read, LINE_BUF_SIZE - c->cmd_read);
    if (r == -1) return check_err(c, "read()");
    if (r == 0) return conn_close(c); /* the client hung up */
    
    c->cmd_read += r; /* we got some bytes */

    c->cmd_len = cmd_len(c); /* find the EOL */

    /* yay, complete command line */
    if (c->cmd_len) return do_cmd(c);

    /* c->cmd_read > LINE_BUF_SIZE can't happen */

    /* command line too long? */
    if (c->cmd_read == LINE_BUF_SIZE) {
        c->cmd_read = 0; /* discard the input so far */
        return reply_msg(c, MSG_BAD_FORMAT);
    }

    /* otherwise we have an incomplete line, so just keep waiting */
    break;
  case STATE_SENDWORD:
    r= write(c->socket.fd, c->reply + c->reply_sent, c->reply_len - c->reply_sent);
    if (r == -1) return check_err(c, "write()");
    if (r == 0) return conn_close(c); /* the client hung up */

    c->reply_sent += r; /* we got some bytes */

    /* (c->reply_sent > c->reply_len) can't happen */

    if (c->reply_sent == c->reply_len) return reset_conn(c);

    /* otherwise we sent an incomplete reply, so just keep waiting */
    break;
  }
}

#define want_command(c) ((c)->socket.fd && ((c)->state == STATE_WANTCOMMAND))
#define cmd_data_ready(c) (want_command(c) && (c)->cmd_read)

static void
h_conn(const int fd, const short which, Connection c)
{
  if (fd != c->socket.fd) {
    twarnx("Argh! event fd doesn't match conn fd.");
    close(fd);
    conn_close(c);
    update_conns();
    return;
  }
  
  conn_data(c);
  while (cmd_data_ready(c) && (c->cmd_len = cmd_len(c))) do_cmd(c);
  update_conns();
}

static void
prothandle(Connection c, int ev)
{
  h_conn(c->socket.fd, ev, c);
}

void
h_accept(const int fd, const short which, Server *s)
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

  c = make_conn(cfd, STATE_WANTCOMMAND);
  if (!c) {
      twarnx("make_conn() failed");
      close(cfd);
      update_conns();
      return;
  }
  
  c->socket.x = c;
  c->socket.f = (Handle)prothandle;
  c->socket.fd = cfd;
  
  dbgprintf("accepted conn, fd=%d\n", cfd);
  r = sockwant(&c->socket, 'r');
  if (r == -1) {
      twarn("sockwant");
      close(cfd);
      update_conns();
      return;
  }
  update_conns();
}