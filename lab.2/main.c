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

int setup_server_socket();

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
    close(server_socket);

    return 0;
}