/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <lwip/netif.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>

#include <serial/serial.h>

// To remember checkout a mobile keypad for AOS06
#define AOS06_PORT (26706)

/* Limit the MTU to give client a chance to respond */
#define MAX_PAYLOAD_SIZE  500

/* Attempt to obtain UDP reliability by providing some delay to flush network queues
 * This should not be a problem when using a high throughput connection
 */
//#define TX_DELAY_US  1000UL
#define TX_DELAY_US  0UL

struct serial {
    void (*fHandler) (struct serial *serial, char c);
    struct udp_pcb *fUpcb;
};

static void 
serial_recv_handler(void *vSerial, struct udp_pcb *unused0, 
                    struct pbuf *p, struct ip_addr *unused1, u16_t unused2)
{
    struct serial *serial = (struct serial *) vSerial;
    if (serial && serial->fHandler) {
        struct pbuf *q;
        for(q = p; q != NULL; q = q->next){
            char *data = q->payload;
            int i;
            for(i = 0; i < q->len; i++){
                serial->fHandler(serial, *data++);
            }
        }
    }
    pbuf_free(p);
}

struct serial *
serial_init(void)
{
    static struct serial serial = {.fUpcb = NULL, .fHandler = NULL};
    if(serial.fUpcb != NULL){
        return &serial;
    }
    serial.fUpcb = udp_new();

    u16_t port = AOS06_PORT;
    if (udp_bind(serial.fUpcb, &netif_default->ip_addr, port)){
        udp_remove(serial.fUpcb);
        serial.fUpcb = NULL;
        return NULL;
    }

    if (udp_connect(serial.fUpcb, &netif_default->gw, port)){
        udp_remove(serial.fUpcb);
        serial.fUpcb = NULL;
        return NULL;
    }
    udp_recv(serial.fUpcb, &serial_recv_handler, &serial);

    return &serial;
}

static void
__delay(int us)
{
    while(us--){
        /* Loosely assume 1GHZ clock */
        volatile int i = 1000;
        while(i--);
    }
}

int
serial_send(struct serial *serial, char *data, int len)
{
    int to_send = len;
    while(to_send > 0){
        int plen;
        struct pbuf *p;
        /* Generate the packet */
        plen = (to_send > MAX_PAYLOAD_SIZE)? MAX_PAYLOAD_SIZE : to_send;
        p = pbuf_alloc(PBUF_TRANSPORT, plen, PBUF_RAM);
        if(p == NULL){
            return len - to_send;
        }

        if(pbuf_take(p, data, plen)){
            pbuf_free(p);
            return len - to_send;
        }

        __delay(TX_DELAY_US);

        if (udp_send(serial->fUpcb, p)){
            pbuf_free(p);
            return len - to_send;
        }

        pbuf_free(p);
        to_send -= plen;
        data += plen;
    }
    return len;
}

int
serial_register_handler(struct serial *serial,
                        void (*handler)(struct serial *serial, char c))
{
    serial->fHandler = handler;
    return 0;
}
