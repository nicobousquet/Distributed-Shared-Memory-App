#include "common_impl.h"
#include <errno.h>
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int dsm_send(int dest, void *buf, size_t size, int flag) {
    int num_send = 0;
    if ((num_send = send(dest, buf, size, flag)) <= 0) {
        // si BROKEN PIPE c'est normal pour truc
        if (errno == EPIPE) {
            return 0;
        }
        perror("dsm_send()");
    }
    return num_send;
}

int dsm_recv(int from, void *buf, size_t size, int flag) {
    int num_rec = 0;
    if ((num_rec = recv(from, buf, size, flag)) < 0) {
        perror("dsm_recv");
    }
    return num_rec;
}

struct socket create_server_socket(struct socket server_socket) {
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
        server_socket.fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_socket.fd == -1) {
            continue;
        }
        if (bind(server_socket.fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(server_socket.fd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in my_addr;
    socklen_t len = sizeof(my_addr);
    getsockname(server_socket.fd, (struct sockaddr *) &my_addr, &len);
    inet_ntop(AF_INET, &my_addr.sin_addr, server_socket.ip_addr, sizeof(server_socket.ip_addr));
    //on récupère le port d'écoute effectif
    server_socket.port = ntohs(my_addr.sin_port);
    freeaddrinfo(result);
    return server_socket;
}

struct socket socket_connect(char *hostname, struct socket client_socket, char *connecting_port) {
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(hostname, connecting_port, &hints, &result) != 0) {
        perror("getaddrinfo()");
        exit(EXIT_FAILURE);
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        client_socket.fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (client_socket.fd == -1) {
            continue;
        }
        if (connect(client_socket.fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            struct sockaddr_in *sockAddrInPtr = (struct sockaddr_in *) rp->ai_addr;
            socklen_t len = sizeof(struct sockaddr_in);
            getsockname(client_socket.fd, (struct sockaddr *) sockAddrInPtr, &len);
            client_socket.port = ntohs(sockAddrInPtr->sin_port);
            strcpy(client_socket.ip_addr, inet_ntoa(sockAddrInPtr->sin_addr));
            break;
        }
        close(client_socket.fd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(result);
    return client_socket;
}

