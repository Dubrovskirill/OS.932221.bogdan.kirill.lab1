#define _GNU_SOURCE
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>

#define PORT "31337"
#define MAX_CLIENTS 256

volatile sig_atomic_t wasSigHup = 0;


void sigHupHandler(int sig) {
    (void)sig; // Подавляем предупреждение о неиспользуемом параметре
    wasSigHup = 1;
}

int setup_server_socket() {
    struct addrinfo hints, *addr_info, *iter;
    int server_socket;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &addr_info) != 0) {
        perror("getaddrinfo failed");
        return -1;
    }

    for (iter = addr_info; iter != NULL; iter = iter->ai_next) {
        server_socket = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol);
        if (server_socket < 0) {
            continue;
        }

        int yes = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt SO_REUSEADDR failed");
            close(server_socket);
            freeaddrinfo(addr_info);
            return -1;
        }

        if (bind(server_socket, iter->ai_addr, iter->ai_addrlen) == 0) {
            break;
        }

        close(server_socket);
    }

    freeaddrinfo(addr_info);

    if (iter == NULL) {
        fprintf(stderr, "Failed to bind server socket\n");
        return -1;
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("listen failed");
        close(server_socket);
        return -1;
    }

    printf("Server socket created and listening on port %s\n", PORT);
    return server_socket;
}

void register_signal_handler() {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sa.sa_handler = sigHupHandler;

    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction for SIGHUP failed");
        exit(EXIT_FAILURE);
    }

    printf("SIGHUP handler registered.\n");
}

void *get_socket_address(struct sockaddr *sockaddr) {
    if (sockaddr->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sockaddr)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sockaddr)->sin6_addr);
}

int main() {

    int server_socket = setup_server_socket();
     if (server_socket == -1) {
        return 1;
    }
    int client_sockets[MAX_CLIENTS];
    int active_clients = 0;

    register_signal_handler();

    sigset_t blockedMask, origMask;
    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, SIGHUP);

     if (sigprocmask(SIG_BLOCK, &blockedMask, &origMask) == -1) {
        perror("sigprocmask error");
        close(server_socket);
        return 1;
    }

    printf("Server listening on port %s. Waiting for connections and signals...\n", PORT);
    
    fd_set read_fds;
    FD_ZERO(&read_fds); 
    FD_SET(server_socket, &read_fds); 
    int max_fd = server_socket; 
    while (!wasSigHup) {
        fd_set temp_fds = read_fds; 
        int pselect_result = pselect(max_fd + 1, &temp_fds, NULL, NULL, NULL, &origMask);
        if (pselect_result == -1) {
           if (errno != EINTR) {
                perror("pselect error");
                break; 
            }
             continue;
        }
        if (wasSigHup) {
            printf("Signal received, shutting down server gracefully...\n");
            break; 
        }
        if (FD_ISSET(server_socket, &temp_fds)) {
            
            struct sockaddr_storage client_addr;
            socklen_t addr_size = sizeof(client_addr);
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
             if (new_socket == -1) {
                perror("accept error");
            } else {
                 char client_ip[INET6_ADDRSTRLEN];
                inet_ntop(client_addr.ss_family, get_socket_address((struct sockaddr *)&client_addr), client_ip, INET6_ADDRSTRLEN);
                printf("New connection attempt from %s on socket %d\n", client_ip, new_socket);

                if (active_clients >= 1) {
                   
                    printf("Server already has an active connection. Closing new socket %d\n", new_socket);
                    close(new_socket);
                } else {
                    
                    FD_SET(new_socket, &read_fds); 
                    client_sockets[active_clients] = new_socket;
                    active_clients++;
                    if (new_socket > max_fd) max_fd = new_socket; 
                    printf("Keeping socket %d as the active connection.\n", new_socket);
                }
            }
            if (--pselect_result <= 0) {
                continue;
            }
        }
        for (int i = 0; i < active_clients && pselect_result > 0; i++) {
            int client_socket = client_sockets[i];
            if (FD_ISSET(client_socket, &temp_fds)) {
                char buffer[1024];
                int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

                if (bytes_received <= 0) {
                     if (bytes_received == 0) {
                        printf("Client on socket %d disconnected.\n", client_socket);
                    } else {
                        perror("recv error on client socket");
                    }

                    FD_CLR(client_socket, &read_fds);
                    close(client_socket);

                    for (int j = i; j < active_clients - 1; j++) {
                        client_sockets[j] = client_sockets[j + 1];
                    }
                    active_clients--;
                    i--;
                    pselect_result--;

                } else {
                    // Данные успешно получены
                    printf("Received from socket %d: %d bytes\n", client_socket, bytes_received);
                }

            }

        }
    }
    for (int i = 0; i < active_clients; i++) {
        printf("Closing active client socket %d\n", client_sockets[i]);
        close(client_sockets[i]);
    }

    close(server_socket);
    printf("Server shutdown complete.\n"); 
    return 0;
}