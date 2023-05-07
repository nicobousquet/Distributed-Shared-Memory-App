#include <sys/types.h>
#include <sys/socket.h>


/* autres includes (eventuellement) */

#define ERROR_EXIT(str) {perror(str);exit(EXIT_FAILURE);}

/**************************************************************/
/****************** DEBUT DE PARTIE NON MODIFIABLE ************/
/**************************************************************/

#define MAX_STR  (1024)
typedef char maxstr_t[MAX_STR];

/* definition du type des infos */
/* de connexion des processus dsm */
typedef struct dsm_proc_conn {
    int rank;
    maxstr_t machine;
    int port_num;
    int fd;
} dsm_proc_conn_t;

struct server {
    int fd;
    char ip_addr[16];
    int port;
};

struct client {
    int fd;
    char ip_addr[16];
    int port;
};

int dsm_send(int dest, void *buf, size_t size, int flag);

int dsm_recv(int from, void *buf, size_t size, int flag);

struct server server_init();

struct client client_init(char *hostname, char *connecting_port);
/**************************************************************/
/******************* FIN DE PARTIE NON MODIFIABLE *************/
/**************************************************************/

/* definition du type des infos */
/* d'identification des processus dsm */
typedef struct dsm_proc {
    pid_t pid;
    int pipe_fd_stdout; //file descriptor du tube associé à SDTOUT
    int pipe_fd_stderr; //descriptor du tube associé à SDTERR
    dsm_proc_conn_t connect_info;
} dsm_proc_t;