#include <sasl/sasl.h>

#define twarn(fmt, args...) warn("%s:%d in %s: " fmt, \
                                 __FILE__, __LINE__, __func__, ##args)

#define DEBUG 1

#ifdef DEBUG
#define dbgprintf(fmt, args...) ((void) fprintf(stderr, fmt, ##args))
#else
#define dbgprintf(fmt, ...) ((void) 0)
#endif

#define LINE_BUF_SIZE 208
#define MAX_DATA_SIZE 8192
#define STACK_SIZE 32768

typedef struct Connection *Connection;

struct Connection {
  int fd;
  
  sasl_conn_t *sasl_conn;
  int state;
  
  char command[LINE_BUF_SIZE];
  
  char *data_in;
  int data_in_len;
  int data_in_loc;
  
  char *data_out;
  int data_out_len;
  int data_out_loc;
};

Connection conn_init();
void conn_close(Connection conn);
void conn_write(Connection conn);
void conn_read(Connection conn);
int conn_read_command(Connection conn);

void sasl_init();
void sasl_free();
sasl_conn_t* sasl_conn_new();
void sasl_conn_free(sasl_conn_t* sasl_conn);

void sasl_fsm_dispatch(Connection conn, int event);
int sasl_list_mechanisms(Connection conn);
int sasl_start(Connection conn);
int sasl_step(Connection conn);
int fsm_error(Connection conn);

/*
 * State machine setup - see great thread at http://stackoverflow.com/questions/1647631/c-state-machine-design
 */

typedef struct {
  int state;
  int event;
  int (*transition_function)(Connection);
} tTransition;

#define ST_ANY           -1
#define ST_START          0
#define ST_SASL_READY     1 // Global SASL has been initialized already with sasl_server_init(),
                            // There exists a sasl_conn_t for this connection via sasl_server_new(),
                            // But authentication has not been attempted.
#define ST_SASL_CONTINUE  2 // Authentication has begun, more steps required
#define ST_SASL_OK        3 // Completed successfully 
#define ST_SASL_FAIL      4 // Authentication failed

#define EV_ANY                  -1
#define EV_SASL_LIST_MECHANISMS  1000
#define EV_SASL_START            1001
#define EV_SASL_STEP             1002
#define EV_ERROR                 2000
