#include "esp_common.h"
#include "espconn.h"
#include "c_types.h"

#define UDP_SERVER_LOCAL_PORT 8182


static void (*recv_callback)(char *buf, int len);
static struct espconn udp_client;
const uint8 udp_server_ip[4] = { 255, 255, 255, 255 };

#define DBG_LINES printf
#define DBG_PRINT printf

void UdpRecvCb(void *arg, char *pdata, unsigned short len)
{
    struct espconn* udp_server_local = arg;
    DBG_LINES("UDP_RECV_CB");
    DBG_PRINT("UDP_RECV_CB len:%d ip:%d.%d.%d.%d port:%d\n", len, udp_server_local->proto.tcp->remote_ip[0],
            udp_server_local->proto.tcp->remote_ip[1], udp_server_local->proto.tcp->remote_ip[2],
            udp_server_local->proto.tcp->remote_ip[3], udp_server_local->proto.tcp->remote_port);
    espconn_send(udp_server_local, pdata, len);
    if(recv_callback != 0)
        recv_callback(pdata, len);
}

void UdpSendCb(void* arg)
{
    struct espconn* udp_server_local = arg;
    DBG_LINES("UDP_SEND_CB");
    DBG_PRINT("UDP_SEND_CB ip:%d.%d.%d.%d port:%d\n", udp_server_local->proto.tcp->remote_ip[0],
            udp_server_local->proto.tcp->remote_ip[1], udp_server_local->proto.tcp->remote_ip[2],
            udp_server_local->proto.tcp->remote_ip[3], udp_server_local->proto.tcp->remote_port);
    
}

os_timer_t time1;
#define UDP_Client_GREETING "drape\n"
void t1Callback(void* arg)
{
    os_printf("t1 callback\n");
    espconn_send(&udp_client, UDP_Client_GREETING, strlen(UDP_Client_GREETING));

}

void udp_client_start(void (*cb)(char *buf, int len))
{

    static esp_udp udp;

    recv_callback = cb;

    udp_client.type = ESPCONN_UDP;
    udp_client.proto.udp = &udp;
    udp.remote_port = UDP_SERVER_LOCAL_PORT;

    memcpy(udp.remote_ip, udp_server_ip, sizeof(udp_server_ip));
    uint8 i = 0;
    os_printf("serve ip:\n");
    for (i = 0; i <= 3; i++) {
        os_printf("%u.", udp_server_ip[i]);
    }
    os_printf("\n remote ip\n");
    for (i = 0; i <= 3; i++) {
        os_printf("%u.", udp.remote_ip[i]);
    }
    os_printf("\n");
    espconn_regist_recvcb(&udp_client, UdpRecvCb);
    espconn_regist_sentcb(&udp_client, UdpSendCb);
    int8 res = 0;
    res = espconn_create(&udp_client);

    if (res != 0) {
        DBG_PRINT("UDP CLIENT CREAT ERR ret:%d\n", res);
    }

    os_timer_disarm(&time1);
    os_timer_setfn(&time1, t1Callback, NULL);
    os_timer_arm(&time1, 5000, 1);

    vTaskDelete(NULL);

}

