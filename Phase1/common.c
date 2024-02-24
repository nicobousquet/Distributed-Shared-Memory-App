#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

ssize_t dsm_send(int dest, void *buf, size_t size, int flag) {
    ssize_t num_send = 0;
    if ((num_send = send(dest, buf, size, flag)) <= 0) {
        // si BROKEN PIPE c'est normal pour truc
        if (errno == EPIPE) {
            return 0;
        }
        perror("dsm_send()");
    }
    return num_send;
}

ssize_t dsm_recv(int from, void *buf, size_t size, int flag) {
    ssize_t num_rec = 0;
    if ((num_rec = recv(from, buf, size, flag)) < 0) {
        perror("dsm_recv");
    }

    return num_rec;
}

struct server server_init() {
    struct server server;
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    //port d'écoute dynamique
    char port[MAX_STR] = "0";
    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server.fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server.fd == -1) {
            continue;
        }
        if (bind(server.fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(server.fd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in my_addr;
    socklen_t len = sizeof(my_addr);
    getsockname(server.fd, (struct sockaddr *) &my_addr, &len);
    inet_ntop(AF_INET, &my_addr.sin_addr, server.ip_addr, sizeof(server.ip_addr));
    //on récupère le port d'écoute effectif
    server.port = ntohs(my_addr.sin_port);
    freeaddrinfo(result);

    return server;
}

struct client client_init(char *hostname, char *connection_port) {
    struct client dsmwrap_client;
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, connection_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        dsmwrap_client.fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (dsmwrap_client.fd == -1) {
            continue;
        }
        if (connect(dsmwrap_client.fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            struct sockaddr_in *sockAddrInPtr = (struct sockaddr_in *) rp->ai_addr;
            socklen_t len = sizeof(struct sockaddr_in);
            getsockname(dsmwrap_client.fd, (struct sockaddr *) sockAddrInPtr, &len);
            dsmwrap_client.port = ntohs(sockAddrInPtr->sin_port);
            strcpy(dsmwrap_client.ip_addr, inet_ntoa(sockAddrInPtr->sin_addr));
            break;
        }
        close(dsmwrap_client.fd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return dsmwrap_client;
}

