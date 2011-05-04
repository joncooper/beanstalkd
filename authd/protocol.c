#define STATE_START 0
#define STATE_SENDWORD 1

#define MSG_BAD_FORMAT "BAD_FORMAT\r\n"

#define CONSTSTRLEN(m) (sizeof(m) - 1)

#include <errno.h>
#include <fcntl.h>
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

// Doubly linked list of all live connections

static struct Connection connections = {&connections, &connections};

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

static void
reset_conn(Connection c)
{
  connwant(c, 'r', &dirty);
  c->reply_sent = 0; /* now that we're done, reset this */
  c->state = STATE_START;
  
  dbgprintf("reset_conn()");
}

static void
do_cmd(Connection c)
{
  /* NUL-terminate this string so we can use strtol and friends */
  c->cmd[c->cmd_len - 2] = '\0';

  /* check for possible maliciousness */
  if (strlen(c->cmd) != c->cmd_len - 2) {
      return reply_msg(c, MSG_BAD_FORMAT);
  }
  
  dbgprintf("got %s\n", c->cmd);
}

static void
conn_data(Connection c)
{
  int r;
  
  switch (c->state) {
  case STATE_START:
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

  c = make_conn(cfd, STATE_START);
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