/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DATA61_GPL)
 */
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <utils/util.h>
#include <networkconsole/networkconsole.h>
#include <sos/gen_config.h>
#undef PACKED /* picotcp redefines this */
#include <pico_socket.h>
#include <pico_ipv4.h>

#define AOS_BASEPORT (26700)
#define MAX_PAYLOAD_SIZE  1024

struct network_console {
    struct pico_ip4 inaddr_any;
    struct pico_socket *pico_socket;
    void (*handler)(struct network_console *network_console, char c);
    uint32_t peer;
    uint16_t port;
};

/* Incoming data is read into this buffer */
static char buf[MAX_PAYLOAD_SIZE];
static struct network_console network_console = {};

/*
 * This function will be called from the network stack on any networking event, such as receiving data.
 * If you have a registered handler to receive incoming data, it gets called from here. 
 */
static void network_console_recv_handler(uint16_t ev, struct pico_socket *s)
{
    if (ev == PICO_SOCK_EV_RD && network_console.handler) {
        int read = 0;
        do {
            read = pico_socket_recvfrom(network_console.pico_socket, buf, MAX_PAYLOAD_SIZE, &network_console.peer, &network_console.port);
            for (int i = 0; i < read; i++) {
                network_console.handler(&network_console, buf[i]);
            }
        } while (read > 0);
    } else if (ev == PICO_SOCK_EV_ERR) {
        ZF_LOGE("Pico recv error");
    }
}

struct network_console *network_console_init(void)
{
    if (network_console.pico_socket != NULL) {
        ZF_LOGE("Network console already initialised!");
        return NULL;
    }

    network_console.pico_socket = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &network_console_recv_handler);
    if (!network_console.pico_socket) {
        ZF_LOGE("Network console connection failed");
        return NULL;
    }

    struct pico_ip4 gateway;
    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &gateway.addr);

    struct pico_ip4 *src = pico_ipv4_source_find(&gateway);
    unsigned char *octects = (unsigned char *) &src->addr;
    printf("libnetworkconsole using udp port %d\n", AOS_BASEPORT + octects[3]);

    /* Configure peer/port for sendto */
    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &network_console.peer);
    uint16_t port_be = short_be(AOS_BASEPORT + octects[3]);
    network_console.port = port_be;

    int err = pico_socket_bind(network_console.pico_socket, &network_console.inaddr_any, &port_be);
    if (err) {
        return NULL;
    }

    err = pico_socket_connect(network_console.pico_socket, &network_console.peer, network_console.port);
    if (err < 0) {
        ZF_LOGE("Network console failed to connect to UDP server");
        return NULL;
    }

    return &network_console;
}

int network_console_send(struct network_console *network_console, char *data, int len)
{
    assert(network_console->pico_socket != NULL);
    int total_sent = 0;
    while (total_sent < len) {
        int sent = pico_socket_sendto(network_console->pico_socket, data, len, &network_console->peer, network_console->port);
        if (sent == -1) {
            ZF_LOGE("Pico send failed");
            return -1;
        }
        if (sent == 0) {
            return total_sent;
        }
        total_sent += sent;
    }
    return len;
}

int network_console_register_handler(struct network_console *network_console,
                            void (*handler)(struct network_console *network_console, char c))
{
    network_console->handler = handler;
    return 0;
}
