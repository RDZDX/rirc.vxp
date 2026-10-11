/*
 * MRE TCP adapter: one non-blocking TCP stream to port 6667.
 *
 * Adapted from the TCP pattern in IPoverObexVxp (tcp/tcp.c): vm_tcp_connect
 * with an event callback, then vm_tcp_read / vm_tcp_write when the platform
 * says the socket is readable / writable. No TLS, no Bluetooth/OBEX, no
 * POSIX sockets.
 */
#ifndef NET_TCP_H
#define NET_TCP_H

typedef enum {
	NET_EV_CONNECTED,
	NET_EV_READABLE,
	NET_EV_WRITABLE,
	NET_EV_HOST_NOT_FOUND,
	NET_EV_CLOSED  /* peer closed or connection broken */
} net_event_t;

typedef void (*net_event_fn)(net_event_t ev);

/* Start connecting to host on RIRC_IRC_PORT (fixed). Events go to cb.
 * Returns 0 if the attempt was started, -1 otherwise. */
int net_connect(const char *host, net_event_fn cb);

/* Close the stream (no event is delivered for a local close). */
void net_close(void);

/* 1 while a connection attempt or an established stream exists. */
int net_active(void);

/* Returns bytes accepted by the socket (0 = would block / nothing). */
int net_send(const char *buf, int len);

/* Returns bytes read (0 = nothing available). */
int net_recv(char *buf, int len);

#endif
