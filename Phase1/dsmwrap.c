#define _GNU_SOURCE

#include "common_impl.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv) {
    /* processus intermediaire pour "nettoyer" */
    /* la liste des arguments qu'on va passer */
    /* a la commande a executer finalement  */

    /* creation d'une socket pour se connecter au */
    /* au lanceur et envoyer/recevoir les infos */
    /* necessaires pour la phase dsm_init */
    char *hostname = argv[1];
    char *connecting_port = argv[2];
    struct socket client_socket;
    client_socket = socket_connect(hostname, client_socket, connecting_port);
    printf("*** connexion processus distant (%s:%i) avec dsmexec (%s:%s) ==> OK ***\n", client_socket.ip_addr,
           client_socket.port, hostname, connecting_port);
    //on définit la variable d'environnement DSMEXEC_FD
    char DSMEXEC_FD[MAX_STR];
    sprintf(DSMEXEC_FD, "DSMEXEC_FD=%i", client_socket.fd);

    /* Envoi du nom de machine au lanceur */
    char name[MAX_STR];
    gethostname(name, MAX_STR);
    //envoi du nom de la machine
    dsm_send(client_socket.fd, client_socket.ip_addr, MAX_STR, 0);

    //on récupère le pid du processus
    pid_t pid = getpid();
    //envoi du pid au lanceur
    dsm_send(client_socket.fd, &pid, sizeof(pid_t), 0);
    //le processus met dans le tube son pid pour que le lanceur puisse lier ses tubes avec son processus
    write(STDOUT_FILENO, &pid, sizeof(pid_t));
    /* Creation de la socket d'ecoute pour les */
    /* connexions avec les autres processus dsm */
    struct socket server_socket;
    server_socket = create_server_socket(server_socket); //connfd;
    printf("*** Processus distant écoute sur %s:%i ***\n", server_socket.ip_addr, server_socket.port);
    //on définit la variable d'environnement MASTER_FD
    char MASTER_FD[MAX_STR];
    sprintf(MASTER_FD, "MASTER_FD=%i", server_socket.fd);
    /* Envoi du numero de port au lanceur */
    /* pour qu'il le propage à tous les autres */
    /* processus dsm */
    dsm_send(client_socket.fd, &server_socket.port, sizeof(server_socket.port), 0);
    fflush(stdout);
    /* on execute la bonne commande */
    /* attention au chemin à utiliser ! */
    char *args[argc - 2];
    for (int i = 3; i < argc; i++) {
        args[i - 3] = argv[i];
    }
    args[argc - 3] = NULL;
    char *arge[] = {MASTER_FD, DSMEXEC_FD, NULL};
    if (execvpe(argv[3], args, arge) == -1) {
        perror("execvpe");
    }
    /************** ATTENTION **************/
    /* vous remarquerez que ce n'est pas   */
    /* ce processus qui récupère son rang, */
    /* ni le nombre de processus           */
    /* ni les informations de connexion    */
    /* (cf protocole dans dsmexec)         */
    /***************************************/

    return 0;
}
