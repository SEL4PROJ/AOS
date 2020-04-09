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
#include <serial/serial.h>
#include <sos/gen_config.h>
#undef PACKED /* picotcp redefines this */
#include <pico_socket.h>
#include <pico_ipv4.h>

#define AOS_BASEPORT (26700)
#define MAX_PAYLOAD_SIZE  1024

struct serial {
    struct pico_ip4 inaddr_any;
    struct pico_socket *pico_socket;
    void (*handler)(struct serial *serial, char c);
    uint32_t peer;
    uint16_t port;
};

char buf[MAX_PAYLOAD_SIZE];
static struct serial serial = {};

static void serial_recv_handler(uint16_t ev, struct pico_socket *s)
{
    if (ev == PICO_SOCK_EV_RD) {
        int read = 0;
        do {
            read = pico_socket_recvfrom(serial.pico_socket, buf, MAX_PAYLOAD_SIZE, &serial.peer, &serial.port);
            for (int i = 0; i < read; i++) {
                serial.handler(&serial, buf[i]);
            }
        } while (read > 0);
    } else if (ev == PICO_SOCK_EV_ERR) {
        ZF_LOGE("Pico recv error");
    }
}

struct serial *serial_init(void)
{
    if (serial.pico_socket != NULL) {
        ZF_LOGE("Serial already initialised!");
        return NULL;
    }

    serial.pico_socket = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &serial_recv_handler);
    if (!serial.pico_socket) {
        ZF_LOGE("serial connection failed");
        return NULL;
    }

    struct pico_ip4 gateway;
    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &gateway.addr);

    struct pico_ip4 *src = pico_ipv4_source_find(&gateway);
    unsigned char *octects = (unsigned char *) &src->addr;
    printf("libserial using udp port %d\n", AOS_BASEPORT + octects[3]);

    /* Configure peer/port for sendto */
    pico_string_to_ipv4(CONFIG_SOS_GATEWAY, &serial.peer);
    uint16_t port_be = short_be(AOS_BASEPORT + octects[3]);
    serial.port = port_be;

    int err = pico_socket_bind(serial.pico_socket, &serial.inaddr_any, &port_be);
    if (err) {
        return NULL;
    }

    err = pico_socket_connect(serial.pico_socket, &serial.peer, serial.port);
    if (err < 0) {
        ZF_LOGE("serial failed to connect to UDP server");
        return NULL;
    }

    return &serial;
}

int serial_send(struct serial *serial, char *data, int len)
{
    assert(serial->pico_socket != NULL);
    int total_sent = 0;
    while (total_sent < len) {
        int sent = pico_socket_sendto(serial->pico_socket, data, len, &serial->peer, serial->port);
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

int serial_register_handler(struct serial *serial,
                            void (*handler)(struct serial *serial, char c))
{
    serial->handler = handler;
    return 0;
}
