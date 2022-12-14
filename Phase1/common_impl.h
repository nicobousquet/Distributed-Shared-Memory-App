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
struct dsm_proc_conn {
    int rank;
    maxstr_t machine;
    int port_num;
    int fd;
    int fd_for_exit; /* special */
};

struct socket {
    int fd;
    char ip_addr[16];
    int port;
};

typedef struct dsm_proc_conn dsm_proc_conn_t;

int dsm_send(int dest, void *buf, size_t size, int flag);

int dsm_recv(int from, void *buf, size_t size, int flag);

struct socket create_server_socket(struct socket server_socket);

struct socket socket_connect(char *hostname, struct socket client_socket, char *connecting_port);
/**************************************************************/
/******************* FIN DE PARTIE NON MODIFIABLE *************/
/**************************************************************/

/* definition du type des infos */
/* d'identification des processus dsm */
struct dsm_proc {
    pid_t pid;
    int pipe_fd_out; //file descriptor du tube associé à SDTOUT
    int pipe_fd_err; //descriptor du tube associé à SDTERR
    dsm_proc_conn_t connect_info;
};
typedef struct dsm_proc dsm_proc_t;
