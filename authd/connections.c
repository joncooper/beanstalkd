// connections.c
//   define a connection
//   track which connections are dirty
//   keep track of all our connections

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ds.h"

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

// Is this Connection in any linked list?
static int
conn_list_any_p(Connection head)
{
  return head->next != head || head->prev != head;
}

// Insert a Connection into a linked list
void
conn_insert(Connection head, Connection c)
{
  if (conn_list_any_p(c)) return; // It is already in another linked list.
  
  c->prev = head->prev;
  c->next = head;
  head->prev->next = c;
  head->prev = c;
}

// Remove a Connection from a linked list
Connection
conn_remove(Connection c)
{
  if (!conn_list_any_p(c)) return NULL; // It is not in a doubly-linked list!
  
  c->next->prev = c->prev;
  c->prev->next = c->next;
  
  c->prev = c->next = c;
  return c;
}

Connection
make_conn(int fd, char start_state)
{
  Connection c;
  c = conn_alloc();
  if (!c) return twarn("OOM"), (Connection) 0;
  
  // Initialize
  
  c->socket.fd = fd;
  c->state = start_state;
  c->cmd_read = 0;
  c->prev = c->next = c;
  
  return c;
}

void
connwant(Connection c, int rw, Connection list)
{
  c->rw = rw;
  conn_insert(list, c);
  // scheduling went here via connsched->conntickat->srvschedconn
}

void
conn_close(Connection c)
{
  // deregister event notifications on this connection
  sockwant(&c->socket, 0);
  close(c->socket.fd);
  
  conn_remove(c);
  conn_free(c);
}
