#define _GNU_SOURCE
#include <pthread.h>

/* fin des includes */

#define TOP_ADDR    (0x40000000)
#define PAGE_NUMBER (100)
#define PAGE_SIZE   (sysconf(_SC_PAGE_SIZE))
#define BASE_ADDR   (TOP_ADDR - (PAGE_NUMBER * PAGE_SIZE))
#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];

typedef enum {
    NO_ACCESS,
    READ_ACCESS,
    WRITE_ACCESS,
    UNKNOWN_ACCESS
} dsm_page_access_t;

typedef enum {
    INVALID,
    READ_ONLY,
    WRITE,
    NO_CHANGE
} dsm_page_state_t;

typedef int dsm_page_owner_t;

typedef struct {
    dsm_page_state_t status;
    dsm_page_owner_t owner;
} dsm_page_info_t;

struct client {
    int fd;
    char ip_addr[16];
    int port;
};

typedef struct dsm_proc_conn dsm_proc_conn_t;

extern int DSM_NODE_ID;
extern int DSM_NODE_NUM;

char *dsm_init(int argc, char *argv[]);

void dsm_finalize(void);

struct dsm_proc_conn {
    int rank;
    maxstr_t machine;
    int port_num;
    int fd;
};

typedef enum {
    DSM_NO_TYPE = -1,
    DSM_REQ,
    DSM_PAGE,
    DSM_NREQ,
    DSM_FINALIZE,
    NEW_OWNER,
} dsm_req_type_t;

typedef struct {
    int source;
    int page_num;
    dsm_req_type_t type;
} dsm_req_t;
