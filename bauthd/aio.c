#include "aio.h"

void
aio_read_line(AIO_Operation aio_operation)
{
    
}

// for ref: read(int fildes, void *buf, size_t nbyte);
void
aio_read(AIO_Operation aio_operation)
{
  
}

void
aio_write(AIO_Operation aio_operation)
{
  
}

void
aio_read_line_task(void *v)
{
  int r;
  int line_len;
  
  AIO_Operation op = (AIO_Operation)v;
  
  Connection conn = op->connection;
  AIO_Buffer buf = op->connection->aio_buffer_in;
  
  op->desired = LINE_BUF_SIZE;
  
  r = read(conn->fd, buf->data + buf->loc, op->desired - buf->len);
  if (r == -1) check_err(conn, "aio_read_task()");
  if (r == 0) conn_close(conn); // they hung up
  
  buf->loc += r;
  
  line_len = scan_line_end(buf->data, buf->loc);
  
  /* NUL-terminate this string so we can use strtol and friends */
  buf->data[line_len - 2] = '\0';
  
  /* check for possible maliciousness */
  if (strlen(buf->data) != buf->loc - 2) {
    assert(-1); // error handle here... 
  }
  
  op->aio_dispatcher(op->aio_handler, op);
}

void
aio_read_task(void *v)
{
  int r;
  AIO_Operation op = (AIO_Operation)v;
  
  Connection conn = op->connection;
  AIO_Buffer buf = op->connection->aio_buffer_in;

  r = read(conn->fd, buf->data + buf->loc, op->desired - buf->len);
  if (r == -1) check_err(conn, "aio_read_task()");
  if (r == 0) conn_close(conn); // they hung up

  buf->loc += r;
  assert(buf->loc <= op->desired);
  
  if (buf->loc == op->desired) {
    op->aio_dispatcher(op->aio_handler, op);
  }

  // otherwise, we're not full, yield and keep waiting
}