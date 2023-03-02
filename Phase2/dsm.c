#include "dsm.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <poll.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int DSM_NODE_NUM; /* nombre de processus dsm */
int DSM_NODE_ID;  /* rang (= numero) du processus */
dsm_proc_conn_t *PROC_ARRAY = NULL; //tableau des processus distants
struct pollfd *POLLFDS = NULL; //tableau des fds des processus distants pour la socket d'écoute
int PROCS_FINALIZED = 1; //variable globale du nombre de processus arrivés à la fin de exemple.c afin de les synchroniser
dsm_page_info_t table_page[PAGE_NUMBER];
pthread_t comm_daemon;
sem_t semaphore;

/* indique l'adresse de debut de la page de numero numpage */
static char *num2address(int numpage) {
    char *pointer = (char *) (BASE_ADDR + (numpage * (PAGE_SIZE)));

    if (pointer >= (char *) TOP_ADDR) {
        fprintf(stderr, "[%i] Invalid address !\n", DSM_NODE_ID);
        return NULL;
    } else {
        return pointer;
    }
}

/* cette fonction permet de recuperer un numero de page */
/* a partir  d'une adresse  quelconque */
static int address2num(char *addr) {
    return (((intptr_t)(addr - BASE_ADDR)) / (PAGE_SIZE));
}

/* cette fonction permet de recuperer l'adresse d'une page */
/* a partir d'une adresse quelconque (dans la page)        */
static char *address2pgaddr(char *addr) {
    return (char *) (((intptr_t) addr) & ~(PAGE_SIZE - 1));
}

/* fonctions pouvant etre utiles */
static void dsm_change_info(int numpage, dsm_page_state_t state, dsm_page_owner_t owner) {
    if ((numpage >= 0) && (numpage < PAGE_NUMBER)) {
        if (state != NO_CHANGE) {
            table_page[numpage].status = state;
        }
        if (owner >= 0) {
            table_page[numpage].owner = owner;
        }
        return;
    } else {
        fprintf(stderr, "[%i] Invalid page number !\n", DSM_NODE_ID);
        return;
    }
}

static dsm_page_owner_t get_owner(int numpage) {
    return table_page[numpage].owner;
}

static dsm_page_state_t get_status(int numpage) {
    return table_page[numpage].status;
}

/* Allocation d'une nouvelle page */
static void dsm_alloc_page(int numpage) {
    char *page_addr = num2address(numpage);
    mmap(page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return;
}

/* Changement de la protection d'une page */
static void dsm_protect_page(int numpage, int prot) {
    char *page_addr = num2address(numpage);
    mprotect(page_addr, PAGE_SIZE, prot);
    return;
}

static void dsm_free_page(int numpage) {
    char *page_addr = num2address(numpage);
    munmap(page_addr, PAGE_SIZE);
    return;
}

static int dsm_send(int dest, void *buf, size_t size, int flag) {
    int num_send = 0;
    if ((num_send = send(dest, buf, size, flag)) <= 0) {
        perror("dsm_send()");
        fflush(stderr);
    }
    return num_send;
}

static int dsm_recv(int from, void *buf, size_t size, int flag) {
    int num_rec = 0;
    if ((num_rec = recv(from, buf, size, flag)) < 0) {
        perror("dsm_recv");
        fflush(stderr);
    }
    return num_rec;
}

static _Noreturn void *dsm_comm_daemon(void *arg) {
    printf("Waiting for reqs...\n");
    while (1) {
        //on écoute tous les processus distants
        fflush(stdout);
        fflush(stderr);
        if (poll(POLLFDS, DSM_NODE_NUM, -1) < 0) {
            perror("poll()");
            exit(EXIT_FAILURE);
        }
        dsm_req_t req = {0, 0, 0};
        dsm_req_t msg = {0, 0, 0};
        for (int i = 0; i < DSM_NODE_NUM; i++) {
            if (POLLFDS[i].revents & POLLIN) {
                int numread = dsm_recv(POLLFDS[i].fd, &req, sizeof(dsm_req_t), MSG_WAITALL);
                //si le processus distant s'est terminé
                if (numread == 0) {
                    POLLFDS[i].fd = -1;
                    POLLFDS[i].revents = 0;
                    break;
                }
                //si le processus reçoit une demande de page
                if (req.type == DSM_REQ) {
                    if (table_page[req.page_num].owner ==
                        DSM_NODE_ID) { //si le processus est bien propriétaire de la page
                        msg.source = DSM_NODE_ID;
                        msg.type = DSM_PAGE;
                        msg.page_num = req.page_num;
                        dsm_send(PROC_ARRAY[req.source].fd, &msg, sizeof(dsm_req_t), 0);
                        char *page = num2address(req.page_num);
                        //le processus envoie la page
                        dsm_send(PROC_ARRAY[req.source].fd, (void *) page, PAGE_SIZE, 0);
                        //on rend la page inaccessible pour le processus
                        dsm_protect_page(req.page_num, PROT_NONE);
                        //on libère la page
                        dsm_free_page(req.page_num);
                        //on change le propriétaire dans la table des pages
                        dsm_change_info(req.page_num, WRITE, req.source);
                        //on envoie la page au processus demandeur
                        printf("[%i] Page numéro %i envoyée au processus %i\n", DSM_NODE_ID, msg.page_num, req.source);
                    } else { //le processus n'est plus propriétaire de la page mais le processus demandeur n'a pas encore mis à jour sa table (cas où plusieurs processus
                        //essaient d'accéder à la même page en même temps
                        msg.page_num = req.page_num;
                        //envoie le propriétaire de la page demandée
                        msg.source = table_page[req.page_num].owner;
                        msg.type = NEW_OWNER;
                        //on envoie au processus le bon propriétaire de la page
                        dsm_send(PROC_ARRAY[req.source].fd, &msg, sizeof(dsm_req_t), 0);
                    }
                } else if (req.type == DSM_PAGE) { //si le processus reçoit une page
                    printf("[%i] Page numéro %i reçue du processus %i\n", DSM_NODE_ID, req.page_num, req.source);
                    //le processus s'alloue la page
                    dsm_alloc_page(req.page_num);
                    //le processus reçoit la page
                    char *page = num2address(req.page_num);
                    dsm_recv(POLLFDS[i].fd, (void *) page, PAGE_SIZE, MSG_WAITALL);
                    //le processus se met propriétaire de la page dans la table des pages
                    dsm_change_info(req.page_num, WRITE, DSM_NODE_ID);
                    printf("[%i] Je suis propriétaire de la page %i\n", DSM_NODE_ID, req.page_num);
                    //le processus envoie à tous les autres processus distants qu'il est maintenant propriétaire de la page
                    msg.page_num = req.page_num;
                    msg.source = DSM_NODE_ID;
                    msg.type = DSM_NREQ;
                    for (int j = 0; j < DSM_NODE_NUM; j++) {
                        if (j != DSM_NODE_ID) {
                            dsm_send(PROC_ARRAY[j].fd, &msg, sizeof(dsm_req_t), 0);
                        }
                    }
                    //le processus met un jeton dans le sémaphore pour que la fonction dsm_handler() puisse se terminer
                    sem_post(&semaphore);
                } else if (req.type == DSM_NREQ) { //le processus reçoit des changements d'infos de page
                    //le processus met à jour les informations de la table des pages
                    dsm_change_info(req.page_num, WRITE, req.source);
                    printf("[%i] Le processus %i est maintenant propriétaire de la page %i\n", DSM_NODE_ID, req.source,
                           req.page_num);
                } else if (req.type ==
                           DSM_FINALIZE) { //si le processus reçoit qu'un processus est arrivé à la fin de exemple.c
                    PROCS_FINALIZED++;
                } else if (req.type == NEW_OWNER) { //le processus reçoit le bon propriétaire de la page
                    msg.type = DSM_REQ;
                    msg.source = DSM_NODE_ID;
                    msg.page_num = req.page_num;
                    //le processus demande la page au bon processus
                    dsm_send(PROC_ARRAY[req.source].fd, &msg, sizeof(dsm_req_t), 0);
                }
                POLLFDS[i].revents = 0;
            } else if (POLLFDS[i].revents & POLLHUP) {
                POLLFDS[i].fd = -1;
                POLLFDS[i].revents = 0;
                break;
            }
        }
    }
}

static void dsm_handler(void *addr) {
    //on gère les erreurs de segmentation
    printf("[%i] Page mémoire inaccessible \n", DSM_NODE_ID);
    //on récupère le numéro de la page qui a provoqué l'erreur
    int numpage = address2num(addr);
    //on récupère le propriétaire de la page
    dsm_page_owner_t owner = get_owner(numpage);
    //on envoie une demande de page au propriétaire
    dsm_req_t req = {0, 0, 0};
    req.source = DSM_NODE_ID;
    req.page_num = numpage;
    req.type = DSM_REQ;
    dsm_send(PROC_ARRAY[owner].fd, &req, sizeof(dsm_req_t), 0);
    printf("[%i] Page numéro %i demandée au processus %i\n", DSM_NODE_ID, numpage, owner);
    //le processus reste bloqué le temps que la page ait été envoyée
    sem_wait(&semaphore);
}

/* traitant de signal adequat */
static void segv_handler(int sig, siginfo_t *info, void *context) {
    /* A completer */
    /* adresse qui a provoque une erreur */
    void *addr = info->si_addr;
    /* Si ceci ne fonctionne pas, utiliser a la place :*/
    /*
     #ifdef __x86_64__
     void *addr = (void *)(context->uc_mcontext.gregs[REG_CR2]);
     #elif __i386__
     void *addr = (void *)(context->uc_mcontext.cr2);
     #else
     void  addr = info->si_addr;
     #endif
     */
    /*
    pour plus tard (question ++):
    dsm_access_t access  = (((ucontext_t *)context)->uc_mcontext.gregs[REG_ERR] & 2) ? WRITE_ACCESS : READ_ACCESS;
   */
    /* adresse de la page dont fait partie l'adresse qui a provoque la faute */
    void *page_addr = (void *) (((unsigned long) addr) & ~(PAGE_SIZE - 1));

    if ((addr >= (void *) BASE_ADDR) && (addr < (void *) TOP_ADDR)) {
        dsm_handler(page_addr);
    } else {
        /* SIGSEGV normal : ne rien faire*/
        return;
    }
    return;
}

struct socket socket_connect(struct socket client_socket, char *port, char *machine) {
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(machine, port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    int connected = 0;
    while (connected == 0) {
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            client_socket.fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (client_socket.fd == -1) {
                continue;
            }
            if (connect(client_socket.fd, rp->ai_addr, rp->ai_addrlen) !=
                -1) { //on se connecte aux autres processus distants
                struct sockaddr_in *sockAddrInPtr = (struct sockaddr_in *) rp->ai_addr;
                socklen_t len = sizeof(struct sockaddr_in);
                getsockname(client_socket.fd, (struct sockaddr *) sockAddrInPtr, &len);
                client_socket.port = ntohs(sockAddrInPtr->sin_port);
                strcpy(client_socket.ip_addr, inet_ntoa(sockAddrInPtr->sin_addr));
                connected = 1;
                break;
            }
            close(client_socket.fd);
        }
    }
    freeaddrinfo(result);
    return client_socket;
}

/* Seules ces deux dernieres fonctions sont visibles et utilisables */
/* dans les programmes utilisateurs de la DSM                       */
char *dsm_init(int argc, char *argv[]) {
    struct sigaction act = {0};
    int index;
    int err = sem_init(&semaphore, 0, 0);
    if (err != 0) {
        perror("sem_init");
    }

    /* Récupération de la valeur des variables d'environnement */
    /* DSMEXEC_FD et MASTER_FD                                 */
    char *MASTER_FD_ptr = getenv("MASTER_FD");
    int MASTER_FD = atoi(MASTER_FD_ptr);
    printf("MASTER_FD = %i\n", MASTER_FD);

    if ((listen(MASTER_FD, SOMAXCONN)) != 0) {
        perror("listen()\n");
        exit(EXIT_FAILURE);
    }

    char *DSMEXEC_FD_ptr = getenv("DSMEXEC_FD");
    int DSMEXEC_FD = atoi(DSMEXEC_FD_ptr);
    printf("DSMEXEC_FD = %i\n", DSMEXEC_FD);

    /* reception du nombre de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_NUM) */
    dsm_recv(DSMEXEC_FD, &DSM_NODE_NUM, sizeof(int), MSG_WAITALL);
    printf("DSM_NODE_NUM = %i\n", DSM_NODE_NUM);
    /* reception de mon numero de processus dsm envoye */
    /* par le lanceur de programmes (DSM_NODE_ID)      */
    dsm_recv(DSMEXEC_FD, &DSM_NODE_ID, sizeof(int), MSG_WAITALL);
    printf("DSM_NODE_ID = %i\n", DSM_NODE_ID);
    /* reception des informations de connexion des autres */
    /* processus envoyees par le lanceur :                */
    /* nom de machine, numero de port, etc.               */
    PROC_ARRAY = malloc(DSM_NODE_NUM * sizeof(dsm_proc_conn_t));
    for (int i = 0; i < DSM_NODE_NUM; i++) {
        dsm_recv(DSMEXEC_FD, &PROC_ARRAY[i], sizeof(dsm_proc_conn_t), MSG_WAITALL);
    }
    /* initialisation des connexions              */
    /* avec les autres processus : connect/accept */
    int sfd = 0;
    char port[MAX_STR];
    memset(port, 0, MAX_STR);
    for (int i = 0; i < DSM_NODE_NUM; i++) {
        PROC_ARRAY[i].fd = -1;
        if (PROC_ARRAY[i].rank != DSM_NODE_ID) {
            sprintf(port, "%i", PROC_ARRAY[i].port_num);
            struct socket client_socket;
            client_socket = socket_connect(client_socket, port, PROC_ARRAY[i].machine);
            PROC_ARRAY[i].fd = client_socket.fd; //on remplit le fd dans le proc_array pour pouvoir communiquer avec les autres dsm_procs
            printf("Connexion du processus distant numéro %i (%s:%i) avec le processus distant numéro %i (%s:%i)\n",
                   DSM_NODE_ID, client_socket.ip_addr, client_socket.port, PROC_ARRAY[i].rank, PROC_ARRAY[i].machine,
                   PROC_ARRAY[i].port_num);
        }
    }
    //création socket d'écoute
    int fd;
    struct sockaddr_in server_addr;
    socklen_t len = sizeof(server_addr);
    POLLFDS = malloc((DSM_NODE_NUM) * sizeof(struct pollfd));
    for (int i = 0; i < DSM_NODE_NUM; i++) {
        POLLFDS[i].fd = -1;
        POLLFDS[i].events = -1;
        POLLFDS[i].revents = -1;
        if (PROC_ARRAY[i].rank != DSM_NODE_ID) {
            //on accepte les connections des autres processus distants
            if ((fd = accept(MASTER_FD, (struct sockaddr *) &server_addr, &len)) < 0) {
                perror("accept()\n");
                exit(EXIT_FAILURE);
            }
            POLLFDS[i].fd = fd;
            POLLFDS[i].events = POLLIN;
            POLLFDS[i].revents = -1;
        }
    }
    /* Allocation des pages en tourniquet */
    for (index = 0; index < PAGE_NUMBER; index++) {
        if ((index % DSM_NODE_NUM) == DSM_NODE_ID)
            dsm_alloc_page(index);
        dsm_change_info(index, WRITE, index % DSM_NODE_NUM);
    }
    /* mise en place du traitant de SIGSEGV */
    act.sa_flags = SA_SIGINFO | SA_RESTART;
    act.sa_sigaction = segv_handler;
    sigaction(SIGSEGV, &act, NULL);
    /* creation du thread de communication           */
    /* ce thread va attendre et traiter les requetes */
    /* des autres processus                          */
    pthread_create(&comm_daemon, NULL, &dsm_comm_daemon, NULL);

    /* Adresse de début de la zone de mémoire partagée */
    return ((char *) BASE_ADDR);
}

void dsm_finalize(void) {
    //le processus envoie à tous les autres processus distants qu'il est arrivé à la fin du programme
    dsm_req_t req = {0, 0, 0};
    req.type = DSM_FINALIZE;

    for (int i = 0; i < DSM_NODE_NUM; i++) {
        if (PROC_ARRAY[i].rank != DSM_NODE_ID) {
            dsm_send(PROC_ARRAY[i].fd, &req, sizeof(dsm_req_t), 0);
        }
    }
    //le processus attend que tous les autres processus en soit au même stade que lui pour tuer le thread d'écoute
    int toto = 0;
    while (PROCS_FINALIZED != DSM_NODE_NUM) {
        toto++;
    }
    //on détruit le thread d'écoute
    fflush(stderr);
    fflush(stdout);
    pthread_cancel(comm_daemon);
    void *retval;
    pthread_join(comm_daemon, &retval);
    /* fermer proprement les connexions avec les autres processus */
    for (int i = 0; i < DSM_NODE_NUM; i++) {
        if (POLLFDS[i].fd != -1) {
            close(POLLFDS[i].fd);
            POLLFDS[i].revents = 0;
            POLLFDS[i].events = 0;
            POLLFDS[i].fd = -1;
        }
    }

    for (int i = 0; i < DSM_NODE_NUM; i++) {
        if (PROC_ARRAY[i].fd != -1) {
            close(PROC_ARRAY[i].fd);
            PROC_ARRAY[i].fd = -1;
        }
    }
    free(PROC_ARRAY);
    free(POLLFDS);
    return;
}

