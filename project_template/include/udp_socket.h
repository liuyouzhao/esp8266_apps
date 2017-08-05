typedef struct user_udp_socket_s
{
    int (*init)();
    int (*deinit)();
    int (*recv)(char *data, char *addr, int *len);
    int (*broad)(char *data, int len, int port);
} user_udp_socket_t;

extern user_udp_socket_t g_udp_socket;
