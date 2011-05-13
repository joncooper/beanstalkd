#define LINE_BUF_SIZE 208

typedef struct AIO_Buffer *AIO_Buffer;
typedef struct AIO_Operation *AIO_Operation;
typedef void(*AIO_Handler)(AIO_Operation);
typedef void(*AIO_Dispatcher)(AIO_Handler, AIO_Operation);

/*
 * The Asynchronous Operation Processor that handles the IO will fill (drain)
 * the aio_buffer, then hit the Completion Dispatcher to tell the Proactive Initiator
 * that the work unit is done. (via a Completion Handler).
 */
struct AIO_Operation {
  int desired;
  int complete;
  
  Connection connection;
  
  AIO_Handler aio_handler;
  AIO_Dispatcher aio_dispatcher;
  AIO_Buffer aio_buffer;
};

/*
 * This buffer's current length is in len; the last offset read to (written from)
 * is in loc.
 */
struct AIO_Buffer {
  int len;
  int loc;
  char data[];
};

void aio_read_line(AIO_Operation aio_operation);
void aio_read(AIO_Operation aio_operation);
void aio_write(AIO_Operation aio_operation);