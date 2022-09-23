#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <signal.h>


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

typedef struct dsm_proc_conn dsm_proc_conn_t;

int dsm_send(int dest, void *buf, size_t size, int flag);

int dsm_recv(int from, void *buf, size_t size, int flag);

int create_server_socket();

int socket_connect(char *hostname, char *connecting_port, int sfd);

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
