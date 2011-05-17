#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <task.h>
#include "ds.h"

Connection conn_init()
{
  Connection conn;
  conn = malloc(sizeof(struct Connection));

  conn->sasl_conn = sasl_conn_new();
  conn->state = ST_SASL_READY;

  conn->data_in = calloc(MAX_DATA_SIZE, sizeof(char));
  conn->data_out = calloc(MAX_DATA_SIZE, sizeof(char));

  return conn;
}

void
  conn_close(Connection conn)
{
  free(conn->data_in);
  free(conn->data_out);
  sasl_conn_free(conn->sasl_conn);
  free(conn);
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

void
fdwrite_n(int fd, char *buf, int n)
{
  int written = 0;
  while (written < n) {
    written += fdwrite(fd, buf + written, n - written);
  }
}

void
fdread_n(int fd, char *buf, int n)
{
  int read = 0;
  while (read < n) {
    read += fdread(fd, buf + read, n - read);
  }
}

void
conn_write(Connection conn)
{
  fdwrite_n(conn->fd, conn->data_out + conn->data_out_loc, conn->data_out_len - conn->data_out_loc);
}

void
conn_read(Connection conn)
{
  fdread_n(conn->fd, conn->data_in + conn->data_in_loc, conn->data_in_len - conn->data_in_loc);
}

// This is not remotely threadsafe.
int
conn_read_command(Connection conn)
{
  int r;
  int line_read = 0;
  int line_end = 0;
  do {
    r = fdread(conn->fd, conn->command, LINE_BUF_SIZE - line_read);
    if (r == 0) {
      dbgprintf("read_command(): client hung up.\n");
      return -1;
    }
    if (r == -1) {
      twarn("read_command(): fdread");
      return -1;
    }
    line_read += r;
    line_end = scan_line_end(conn->command, line_read);
  } while((line_read < LINE_BUF_SIZE) && (line_end == 0));

  if (line_end == 0) {
    dbgprintf("read_command(): could not find EOL\n");
    return -1;
  }

  // NULL terminate the command string 
  conn->command[line_end - 2] = '\0';

  // Something strange (and malicious) is afoot at the Circle K.
  assert(strlen(conn->command) == (line_end - 2));
  
  // copy any trailing bytes into the data buffer
  memcpy(conn->data_in, conn->command + line_end, LINE_BUF_SIZE - line_end);
  conn->data_in_loc = LINE_BUF_SIZE - line_end;
  
  return 1;
}
