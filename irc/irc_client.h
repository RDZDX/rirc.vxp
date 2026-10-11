/*
 * Event driven IRC client state machine.
 *
 * It performs no I/O of its own: the host (see main/app.c) feeds received
 * bytes in, and drains the outbound queue. This replaces rirc's blocking
 * socket/select loop. Message handling is modelled on rirc
 * (https://github.com/rcr/rirc, MIT, (C) Richard Robbins).
 */
#ifndef IRC_CLIENT_H
#define IRC_CLIENT_H

#include <stddef.h>

#include "irc_log.h"
#include "rirc_config.h"

typedef enum {
	IRC_ST_DISCONNECTED,
	IRC_ST_CONNECTING,  /* TCP connect in progress */
	IRC_ST_REGISTERING, /* NICK/USER sent, waiting for 001 */
	IRC_ST_ONLINE
} irc_state_t;

typedef struct {
	char server[IRC_SERVER_MAX];
	char nick[IRC_NICK_MAX];
	char channel[IRC_CHAN_MAX]; /* optional, "" = none */
} irc_config_t;

typedef struct {
	irc_config_t cfg;
	irc_state_t state;
	char nick[IRC_NICK_MAX];   /* nick currently in use */
	char target[IRC_CHAN_MAX]; /* channel messages are sent to */
	int nick_retries;

	char rx[IRC_LINE_MAX];
	unsigned rx_len;
	int rx_overflow;

	char tx[IRC_TX_MAX];
	unsigned tx_len;

	irc_log_t log;
} irc_client_t;

void irc_client_init(irc_client_t *c);

/* Configuration setters; return 0 on success, -1 (value unchanged) if invalid. */
int irc_client_set_server(irc_client_t *c, const char *server);
int irc_client_set_nick(irc_client_t *c, const char *nick);
/* "" clears; a missing leading '#' is added. */
int irc_client_set_channel(irc_client_t *c, const char *channel);

/* Lifecycle, driven by the network adapter. */
int  irc_client_begin_connect(irc_client_t *c); /* -1 if already active or no server */
void irc_client_on_connected(irc_client_t *c);  /* TCP is up: sends NICK/USER */
void irc_client_on_closed(irc_client_t *c, const char *reason);
void irc_client_request_quit(irc_client_t *c);  /* queues QUIT */

/* Incoming bytes from the socket (any chunking). */
void irc_client_feed(irc_client_t *c, const char *data, size_t len);

/* A line typed by the user: plain text or a /command (see /help). */
void irc_client_input(irc_client_t *c, const char *text);

/* Outbound queue. Peek, send what the socket accepts, then consume. */
const char *irc_client_tx_peek(const irc_client_t *c, size_t *len);
void        irc_client_tx_consume(irc_client_t *c, size_t n);

const char *irc_state_name(irc_state_t st);

#endif
