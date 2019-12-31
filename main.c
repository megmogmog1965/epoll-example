#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#define SERVER_PORT 12001
#define MAX_EVENTS 100
#define BACKLOG 10


static void stream_write(int fd, const char *buf)
{
    write(fd, buf, strlen(buf));
}

static int open_server_socket()
{
    int socket_fd;
    struct sockaddr_in server_addr;

    if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof server_addr);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(socket_fd, (struct sockaddr *) &server_addr, sizeof server_addr) < 0) {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, BACKLOG) < 0) {
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

static int accept_client_connection(int socket_listening)
{
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof client_addr;

    int socket_established = accept(socket_listening, (struct sockaddr *) &client_addr, &client_addr_len);
    if (socket_established < 0) {
        return -1;
    }

    // Non Blocking.
    int flag = fcntl(socket_established, F_GETFL, 0);
    fcntl(socket_established, F_SETFL, flag | O_NONBLOCK);

    // following 2 lines are for testing purpose only.
    stream_write(socket_established, "---- SLEEP 5 SECONDS ----\n");
    sleep(5);

    return socket_established;
}

static int read_then_echo_write(int socket_established)
{
    char buffer[1024];
    int n = read(socket_established, buffer, sizeof buffer);

    if (n <= 0) {
        return n;
    }

    stream_write(socket_established, ">> ");
    write(socket_established, buffer, n);

    return n;
}

int main()
{
    struct epoll_event events[MAX_EVENTS];

    int epoll_fd;
    if ((epoll_fd = epoll_create(MAX_EVENTS)) < 0) {
        exit(EXIT_FAILURE);
    }

    const int socket_listening = open_server_socket();

    struct epoll_event ev;
    memset(&ev, 0, sizeof ev);
    ev.events = EPOLLIN;
    ev.data.fd = socket_listening;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_listening, &ev);

    for (;;) {
        int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        int i;
        for (i = 0; i < event_count; i++) {
            struct epoll_event e = events[i];

            if (e.data.fd == socket_listening) {
                int socket_established = accept_client_connection(socket_listening);
                if (socket_established < 0) {
                    continue;
                }

                struct epoll_event ev;
                memset(&ev, 0, sizeof ev);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = socket_established;

                // register client connection to epoll.
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_established, &ev);
                stream_write(socket_established, "---- EPOLL REGISTERED ----\n");

            } else {
                int socket_established = e.data.fd;
                int size = read_then_echo_write(socket_established);
                if (size > 0) {
                    continue;
                }

                // close disconnected session.
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_established, &ev);
                close(socket_established);
            }
        }
    }

    return EXIT_SUCCESS;
}
