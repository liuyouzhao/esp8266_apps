#include <nopoll/nopoll.h>
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"

#include "udp_socket.h"

#define BUFSIZE 32
#define BROADCAST_ADDR "255.255.255.255"

static int s_socket_fd;
static int s_socket_fd_send;
static int s_port = 8182;
static int s_broad_intied = 0;


static int init()
{
    struct sockaddr_in addr;

    s_socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(s_socket_fd < 0)
    {
        printf("socket create error %d\n", s_socket_fd);
        return -1;
    }
    
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(s_socket_fd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) == -1)
    {
        printf("bind fail\n");
        return -1;
    }
    return 0;
}

static int udp_broadcast_recv(char *data, char *addr, int *len) {
    struct sockaddr_in addr_from;
    int addr_len = sizeof(struct sockaddr_in);
    char *addr_get;
    int n = -1;

    if(s_socket_fd == -1 || data == 0)
    {
        return -1;
    }

    n = recvfrom(s_socket_fd, data, BUFSIZE, 0, (struct sockaddr *)&addr_from, (socklen_t*)&addr_len);
    *len = n;

    addr_get = inet_ntoa(addr_from.sin_addr);
    memcpy(addr, addr_get, strlen(addr_get));

    return n;
}

int udp_broadcast(char *data, int len, int port)
{
    int optval = 1;
    int n = 0;
    struct sockaddr_in addr;
    s_socket_fd_send = socket(AF_INET, SOCK_STREAM, 0);

    if(s_socket_fd_send == -1)
    {
        return -1;
    }

    if(s_broad_intied == 0)
    {
        struct sockaddr_in addr;

        setsockopt(s_socket_fd_send, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int));
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
        addr.sin_port = htons(port);
    }
    s_broad_intied = 1;
    
    n = sendto(s_socket_fd_send, data, len, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr));
    return n;
}

static int deinit()
{
    close(s_socket_fd);
    s_socket_fd = -1;
    return 0;
}

user_udp_socket_t g_udp_socket = 
{
    .init = init,
    .recv = udp_broadcast_recv,
    .broad = udp_broadcast,
    .deinit = deinit
};
