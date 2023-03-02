#include "common_impl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <signal.h>

/* variables globales */

/* un tableau gerant les infos d'identification */
/* des processus dsm */
dsm_proc_t *proc_array = NULL;

/* le nombre de processus effectivement crees */
volatile int num_procs_creat = 0;

void usage(void) {
    fprintf(stdout, "Usage : dsmexec machinefile executable arg1 arg2 ...\n");
    fflush(stdout);
    exit(EXIT_FAILURE);
}

void sigchld_handler(int sig) {
    /* on traite les fils qui se terminent */
    /* pour eviter les zombies */
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Processus fils %d terminé\n", pid);
    }
}

int read_machine_file(char *filename) {
    FILE *file = fopen(filename, "r");
    int c;
    int last;
    int nLignes = 0;
    while ((c = fgetc(file)) != EOF) {
        if (c == '\n' && last != '\n')
            nLignes++;
        last = c;
    }
    /* Ici, last est égal au caractère juste avant le EOF. */
    if (last != '\n') {//si on n'a pas mis un \n
        nLignes++; /* Dernière ligne non finie */
    }
    rewind(file);
    int num_procs = nLignes;
    proc_array = malloc(num_procs * sizeof(dsm_proc_t));
    memset(proc_array, 0, num_procs * sizeof(dsm_proc_t));
    fclose(file);
    file = fopen(filename, "r");
    if (file == NULL)
        exit(EXIT_FAILURE);

    char *line = NULL;
    size_t len = 0;
    ssize_t num_read;
    int i = 0;
    while ((num_read = getline(&line, &len, file)) != -1) {
        if (num_read != 1) {
            //on récupère le nom des machines
            if (line[strlen(line) - 1] == '\n') {
                strncpy(proc_array[i].connect_info.machine, line, strlen(line) - 1);
            } else {
                strcpy(proc_array[i].connect_info.machine, line);
            }
            //on initialise les valeurs des fd à -1
            proc_array[i].pipe_fd_err = -1;
            proc_array[i].pipe_fd_out = -1;
            i++;
        }
    }
    if (line) {
        free(line);
    }
    fclose(file);
    return num_procs;
}

void listen_and_close_pipes(struct pollfd *pollfds, int num_procs, FILE *stream) {
    char buff[MAX_STR];
    memset(buff, 0, MAX_STR);
    int inactive_pipes = 0; //tubes fermés
    while (1) {
        //si tous les tubes stdout sont fermés
        if (inactive_pipes == num_procs) {
            break;
        }
        //le processus écoute sur stdout
        poll(pollfds, num_procs, -1);
        for (int i = 0; i < num_procs; i++) {
            memset(buff, 0, MAX_STR);
            if (pollfds[i].revents & POLLIN) {
                int numRead = 0;
                int offset = 0;
                while ((numRead = read(pollfds[i].fd, buff + offset, 1)) != 0 && offset < MAX_STR) {
                    if (numRead == -1) {
                        perror("read");
                        exit(EXIT_FAILURE);
                    }
                    offset += numRead;
                    if (buff[offset - 1] == '\n') {
                        for (int j = 0; j < num_procs; j++) {
                            if (stream == stdout) {
                                if (pollfds[i].fd == proc_array[j].pipe_fd_out) {
                                    printf("[Proc# %i][%i <= %s:%i <= stdout]: ", proc_array[j].connect_info.rank,
                                           proc_array[j].connect_info.port_num, proc_array[j].connect_info.machine,
                                           proc_array[j].pid);
                                    printf("%s", buff);
                                }
                            } else if (stream == stderr) {
                                if (pollfds[i].fd == proc_array[j].pipe_fd_err) {
                                    printf("[Proc# %i][%i <= %s:%i <= stderr]: ", proc_array[j].connect_info.rank,
                                           proc_array[j].connect_info.port_num, proc_array[j].connect_info.machine,
                                           proc_array[j].pid);
                                    printf("%s", buff);
                                }
                            }
                        }
                        offset = 0;
                        memset(buff, 0, MAX_STR);
                    }
                }
                pollfds[i].revents = 0;
                if (numRead == 0) {
                    inactive_pipes++; //on incrémente le nombre de tubes stdout fermés
                    close(pollfds[i].fd); //on ferme le pipe de ce côté aussi
                    pollfds[i].revents = 0;
                    pollfds[i].fd = -1;
                }
            } else if (pollfds[i].revents & POLLHUP) { //si pipe fermé de l'autre côté
                inactive_pipes++; //on incrémente le nombre de tubes stdout fermés
                close(pollfds[i].fd); //on ferme le pipe de ce côté aussi
                pollfds[i].revents = 0;
                pollfds[i].fd = -1;
            }
        }
    }
}

char **fill_new_argv(char **newargv, char **args, int argc, char *argv[]) {
    for (int i = 0; i < 5; i++) {
        newargv[i] = malloc(MAX_STR);
        strcpy(newargv[i], args[i]);
    }
    for (int i = 5; i < argc + 3; i++) {
        newargv[i] = malloc(MAX_STR);
        strcpy(newargv[i], argv[i - 3]);
    }
    newargv[argc + 3] = NULL;
    return newargv;
}

/*******************************************************/
/*********** ATTENTION : BIEN LIRE LA STRUCTURE DU *****/
/*********** MAIN AFIN DE NE PAS AVOIR A REFAIRE *******/
/*********** PLUS TARD LE MEME TRAVAIL DEUX FOIS *******/
/*******************************************************/

int main(int argc, char *argv[]) {
    if (argc < 3) {
        usage();
    } else {
        pid_t pid;
        /* Mise en place d'un traitant pour recuperer les fils zombies*/
        /* XXX.sa_handler = sigchld_handler; */
        struct sigaction child;
        memset(&child, 0, sizeof(struct sigaction));
        child.sa_handler = sigchld_handler;
        child.sa_flags = SA_RESTART | SA_NOCLDSTOP;
        sigaction(SIGCHLD, &child, NULL);
        /* lecture du fichier de machines */
        /* 1- on recupere le nombre de processus a lancer */
        int num_procs = read_machine_file(argv[1]);

        /* creation de la socket d'ecoute */

        int connfd;
        struct socket server_socket;
        server_socket = create_server_socket(server_socket);

        if ((listen(server_socket.fd, SOMAXCONN)) != 0) {
            perror("listen()\n");
            exit(EXIT_FAILURE);
        }
        printf("Dsmexec écoute sur le port %i and l'adresse %s\n", server_socket.port, server_socket.ip_addr);

        //le processus récupère le nom de sa machine
        char hostname[MAX_STR];
        gethostname(hostname, MAX_STR);

        /* + ecoute effective */

        //tableau d'écoute des tubes de redirection de stdout
        struct pollfd pollfds_out[num_procs];
        //tableau d'écoute des tubes de redirection de stderr
        struct pollfd pollfds_err[num_procs];
        int fds_out;
        int fds_err;
        /* creation des fils */
        for (int i = 0; i < num_procs; i++) {

            /* creation du tube pour rediriger stdout */
            int fd_out[2];
            pipe(fd_out);

            /* creation du tube pour rediriger stderr */
            int fd_err[2];
            pipe(fd_err);
            pid = fork();
            if (pid == -1) ERROR_EXIT("fork")
            if (pid == 0) { /* fils */
                /* redirection stdout */
                close(STDOUT_FILENO);
                for (int j = 0; j < i; j++) {
                    close(pollfds_out[j].fd);
                    pollfds_out[i].fd = -1;
                }
                close(fd_out[0]);
                dup(fd_out[1]);
                close(fd_out[1]);
                /* redirection stderr */
                close(STDERR_FILENO);
                for (int j = 0; j < i; j++) {
                    close(pollfds_err[j].fd);
                    pollfds_err[j].fd = -1;
                }
                close(fd_err[0]);
                dup(fd_err[1]);
                close(fd_err[1]);
                char dsmwrap[MAX_STR];
                memset(dsmwrap, 0, MAX_STR);
                strncpy(dsmwrap, argv[0], strlen(argv[0]) - strlen("dsmexec"));
                strcat(dsmwrap, "dsmwrap");
                /* Creation du tableau d'arguments pour le ssh */
                char port[MAX_STR];
                sprintf(port, "%u", server_socket.port);
                char *args[5] = {"ssh", proc_array[i].connect_info.machine, dsmwrap, hostname, port};
                char **newargv = malloc((argc + 4) * sizeof(char *));
                newargv = fill_new_argv(newargv, args, argc, argv);
                /* jump to new prog : */
                if (execvp("ssh", newargv) == -1) {
                    perror("execvp");
                }
            } else if (pid > 0) { /* pere */
                /* fermeture des extremites des tubes non utiles */
                close(fd_out[1]);
                close(fd_err[1]);
                fds_out = fd_out[0];
                fds_err = fd_err[0];
                num_procs_creat++;
                //on remplit le tableau d'écoute des tubes de redirection de stdout
                pollfds_out[i].fd = fds_out;
                pollfds_out[i].events = POLLIN;
                pollfds_out[i].revents = 0;
                //on remplit le tableau d'écoute des tubes de redirection de stderr
                pollfds_err[i].fd = fds_err;
                pollfds_err[i].events = POLLIN;
                pollfds_err[i].revents = 0;
            }
        }


        for (int i = 0; i < num_procs; i++) {

            /* on accepte les connexions des processus dsm */
            struct sockaddr_in server_addr;
            socklen_t len = sizeof(server_addr);
            if ((connfd = accept(server_socket.fd, (struct sockaddr *) &server_addr, &len)) < 0) {
                perror("accept()\n");
                exit(EXIT_FAILURE);
            }
            /*On recupere le fd*/
            proc_array[i].connect_info.fd = connfd;

            /*  On recupere le nom de la machine distante */
            dsm_recv(connfd, proc_array[i].connect_info.machine, sizeof(proc_array[i].connect_info.machine),
                     MSG_WAITALL);
            /* On recupere le pid du processus distant  (optionnel)*/
            pid_t dist_pid;
            dsm_recv(connfd, &dist_pid, sizeof(pid_t), MSG_WAITALL);
            proc_array[i].pid = dist_pid;
            /* On recupere le numero de port de la socket */
            /* d'ecoute des processus distants */
            unsigned int dist_listening_port;
            dsm_recv(connfd, &dist_listening_port, sizeof(int), MSG_WAITALL);
            proc_array[i].connect_info.port_num = dist_listening_port;
            //on affecte un rang à chaque processus distant
            proc_array[i].connect_info.rank = i;
        }

        /***********************************************************/
        /********** ATTENTION : LE PROTOCOLE D'ECHANGE *************/
        /********** DECRIT CI-DESSOUS NE DOIT PAS ETRE *************/
        /********** MODIFIE, NI DEPLACE DANS LE CODE   *************/
        /***********************************************************/

        for (int i = 0; i < num_procs; i++) {
            /* 1- envoi du nombre de processus aux processus dsm*/
            /* On envoie cette information sous la forme d'un ENTIER */
            /* (IE PAS UNE CHAINE DE CARACTERES */
            dsm_send(proc_array[i].connect_info.fd, &num_procs, sizeof(int), MSG_NOSIGNAL);

            /* 2- envoi des rangs aux processus dsm */
            /* chaque processus distant ne reçoit QUE SON numéro de rang */
            /* On envoie cette information sous la forme d'un ENTIER */
            /* (IE PAS UNE CHAINE DE CARACTERES */
            dsm_send(proc_array[i].connect_info.fd, &proc_array[i].connect_info.rank, sizeof(int), MSG_NOSIGNAL);

            /* 3- envoi des infos de connexion aux processus */
            /* Chaque processus distant doit recevoir un nombre de */
            /* structures de type dsm_proc_conn_t égal au nombre TOTAL de */
            /* processus distants, ce qui signifie qu'un processus */
            /* distant recevra ses propres infos de connexion */
            /* (qu'il n'utilisera pas, nous sommes bien d'accords). */
            for (int j = 0; j < num_procs; j++) {
                dsm_send(proc_array[i].connect_info.fd, &(proc_array[j].connect_info), sizeof(dsm_proc_conn_t),
                         MSG_NOSIGNAL);
            }
        }


        /***********************************************************/
        /********** FIN DU PROTOCOLE D'ECHANGE DES DONNEES *********/
        /********** ENTRE DSMEXEC ET LES PROCESSUS DISTANTS ********/
        /***********************************************************/

        /* gestion des E/S : on recupere les caracteres */
        /* sur les tubes de redirection de stdout/stderr */
        pid_t pid_in_pipe = 0;
        for (int i = 0; i < num_procs; i++) {
            //on récupère le pid de chaque processus distant envoyé dans les tubes stdout
            read(pollfds_out[i].fd, &pid_in_pipe, sizeof(pid_t));
            for (int j = 0; j < num_procs; j++) {
                //on lie chaque tube à chaque processus distant
                if (proc_array[j].pid == pid_in_pipe) {
                    proc_array[j].pipe_fd_out = pollfds_out[i].fd;
                    proc_array[j].pipe_fd_err = pollfds_err[i].fd;
                }
            }
        }
        listen_and_close_pipes(pollfds_out, num_procs, stdout);
        listen_and_close_pipes(pollfds_err, num_procs, stderr);

        /* on ferme les descripteurs proprement */
        for (int i = 0; i < num_procs; i++) {
            if (proc_array[i].connect_info.fd != -1) {
                close(proc_array[i].connect_info.fd);
            }
        }
        free(proc_array);
        /* on ferme la socket d'ecoute */
        close(server_socket.fd);
    }
    exit(EXIT_SUCCESS);
}

