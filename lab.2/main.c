#define _POSIX_C_SOURCE 200809L  

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define PORT 31337 

static volatile sig_atomic_t g_got_sighup = 0; 
static int g_server_socket = -1; 
static int g_client_socket = -1; 


void handle_sighup(int signo) {
    (void)signo;
    g_got_sighup = 1; 
}


void finish(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void)
{
    struct sigaction sa; 
    memset(&sa, 0, sizeof(sa)); 
    sa.sa_handler = handle_sighup; 
    sigemptyset(&sa.sa_mask); 
    sa.sa_flags = 0;

  
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        finish("sigaction"); 
    }

    sigset_t blocked_signals, orig_mask; 
    sigemptyset(&blocked_signals); 
    sigaddset(&blocked_signals, SIGHUP); 

    if (sigprocmask(SIG_BLOCK, &blocked_signals, &orig_mask) == -1) {
        finish("sigprocmask");
    }

    // Создаем серверный сокет
    g_server_socket = socket(AF_INET, SOCK_STREAM, 0); 
    if (g_server_socket == -1) {
        finish("socket");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); 
    addr.sin_family = AF_INET; 
    addr.sin_port = htons(PORT); 
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 

    if (bind(g_server_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) { 
        finish("bind");
    }
    if (listen(g_server_socket, 1) == -1) {
        finish("listen");
    }

    printf("Server started on port %d (PID %d)\n", PORT, (int)getpid());
    printf("One client is kept active, the remaining connections are closed immediately.\n");

    while(1) {
        fd_set read_fds; 
     
        FD_ZERO(&read_fds); 
        FD_SET(g_server_socket, &read_fds); 
        int maxfd = g_server_socket;

        if (g_client_socket != -1) {
            FD_SET(g_client_socket, &read_fds);
            if (g_client_socket > maxfd) {
                maxfd = g_client_socket;
            }
        }

       
        int ready = pselect(maxfd + 1, &read_fds, NULL, NULL,
            NULL, &orig_mask);

        if (ready == -1) { 
            if (errno == EINTR) {
               
                if (g_got_sighup) {
                    printf("Received signal SIGHUP (in main loop)\n");
                    g_got_sighup = 0;
                }
                continue;
            }
            else { 
                finish("pselect");
            }
        }

       
        if (FD_ISSET(g_server_socket, &read_fds)) { 
            int new_fd = accept(g_server_socket, NULL, NULL); 
            if (new_fd == -1) { 
                perror("accept");
            }
            else {
                printf("New connection accepted\n");
                if (g_client_socket == -1) { 
                    g_client_socket = new_fd; 
                    printf("This connection has become an active client\n");
                }
                else { 
                    printf("There is already an active client, close the extra connection\n");
                    close(new_fd); 
                }
            }
        }
  
        if (g_client_socket != -1 && FD_ISSET(g_client_socket, &read_fds)) {
            char buf[4096];
            ssize_t n = recv(g_client_socket, buf, sizeof(buf), 0);
            if (n > 0) {
                printf("Received %zd bytes\n", n);
            }
            else if (n == 0) {
                printf("The client closed the connection\n");
                close(g_client_socket);
                g_client_socket = -1;
            }
            else {
                if (errno == EINTR) {
                    continue;
                }
                perror("recv");
                close(g_client_socket);
                g_client_socket = -1;
            }
        }
    }
    return 0;
}