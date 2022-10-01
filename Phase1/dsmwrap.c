#define _GNU_SOURCE

#include "common_impl.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main(int argc, char **argv) {
    /* processus intermediaire pour "nettoyer" */
    /* la liste des arguments qu'on va passer */
    /* a la commande a executer finalement  */

    /* creation d'une socket pour se connecter au */
    /* au lanceur et envoyer/recevoir les infos */
    /* necessaires pour la phase dsm_init */
    int sfd = 0;
    char *hostname = argv[1];
    char *connecting_port = argv[2];
    sfd = socket_connect(hostname, connecting_port, sfd);
    //on définit la variable d'environnement DSMEXEC_FD
    char DSMEXEC_FD[MAX_STR];
    sprintf(DSMEXEC_FD, "DSMEXEC_FD=%i", sfd);

    /* Envoi du nom de machine au lanceur */
    char name[MAX_STR];
    gethostname(name, MAX_STR);
    int size = strlen(name);
    //envoi de la taille du nom de la machine
    dsm_send(sfd, &size, sizeof(int), 0);
    //envoi du nom de la machine
    dsm_send(sfd, name, size, 0);

    //on récupère le pid du processus
    pid_t pid = getpid();
    //envoi du pid au lanceur
    dsm_send(sfd, &pid, sizeof(pid_t), 0);
    //le processus met dans le tube son pid pour que le lanceur puisse lier ses tubes avec son processus
    write(STDOUT_FILENO, &pid, sizeof(pid_t));
    /* Creation de la socket d'ecoute pour les */
    /* connexions avec les autres processus dsm */
    int client_fd = create_server_socket(); //connfd;
    //on définit la variable d'environnement MASTER_FD
    char MASTER_FD[MAX_STR];
    sprintf(MASTER_FD, "MASTER_FD=%i", client_fd);
    struct sockaddr_in my_addr;
    socklen_t _len = sizeof(my_addr);
    getsockname(client_fd, (struct sockaddr *) &my_addr, &_len);
    char ip_addr[MAX_STR];
    inet_ntop(AF_INET, &my_addr.sin_addr, ip_addr, sizeof(hostname));
    unsigned int listening_port = ntohs(my_addr.sin_port);
    /* Envoi du numero de port au lanceur */
    /* pour qu'il le propage à tous les autres */
    /* processus dsm */
    dsm_send(sfd, &listening_port, sizeof(unsigned int), 0);
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
