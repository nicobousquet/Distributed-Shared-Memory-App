#define _GNU_SOURCE
#include "common.h"
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
    char *dsmexec_hostname = argv[1];
    char *dsmexec_port = argv[2];
    struct client dsmwrap_client = client_init(dsmexec_hostname, dsmexec_port);
    printf("*** Connexion processus distant (%s:%i) avec dsmexec (%s:%s) ==> OK ***\n", dsmwrap_client.ip_addr, dsmwrap_client.port, dsmexec_hostname, dsmexec_port);
    //on définit la variable d'environnement DSMEXEC_FD
    char DSMEXEC_FD[MAX_STR];
    sprintf(DSMEXEC_FD, "DSMEXEC_FD=%i", dsmwrap_client.fd);

    /* Envoi de l'adresse ip de la machine au lanceur */
    char name[MAX_STR];
    gethostname(name, MAX_STR);
    dsm_send(dsmwrap_client.fd, dsmwrap_client.ip_addr, MAX_STR, 0);

    //on récupère le pid du processus
    pid_t pid = getpid();
    //envoi du pid au lanceur
    dsm_send(dsmwrap_client.fd, &pid, sizeof(pid_t), 0);
    //le processus met dans le tube son pid pour que le lanceur puisse lier ses tubes avec son processus
    write(STDOUT_FILENO, &pid, sizeof(pid_t));
    /* Creation de la socket d'ecoute pour les */
    /* connexions avec les autres processus dsm */
    struct server dsmwrap_server = server_init(); //connfd;
    printf("*** Processus distant écoute sur %s:%i ***\n", dsmwrap_server.ip_addr, dsmwrap_server.port);
    //on définit la variable d'environnement MASTER_FD
    char MASTER_FD[MAX_STR];
    sprintf(MASTER_FD, "MASTER_FD=%i", dsmwrap_server.fd);
    /* Envoi du numero de port au lanceur */
    /* pour qu'il le propage à tous les autres */
    /* processus dsm */
    dsm_send(dsmwrap_client.fd, &dsmwrap_server.port, sizeof(dsmwrap_server.port), 0);
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
