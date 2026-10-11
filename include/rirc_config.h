/*
 * rirc.vxp - compile-time limits and defaults.
 *
 * Everything is fixed-size so the client never allocates at run time.
 */
#ifndef RIRC_CONFIG_H
#define RIRC_CONFIG_H

/* Plain IRC over TCP only. There is deliberately no TLS and no port setting. */
#define RIRC_IRC_PORT 6667

/* Defaults shown the first time the app starts (editable in the Menu) */
#define RIRC_DEFAULT_SERVER  "irc.libera.chat"
#define RIRC_DEFAULT_NICK    "rircvxp"
#define RIRC_DEFAULT_CHANNEL ""

/* Buffer sizes (bytes, including the terminating NUL where applicable) */
#define IRC_SERVER_MAX   64
#define IRC_NICK_MAX     17    /* 16 chars + NUL */
#define IRC_CHAN_MAX     33
#define IRC_LINE_MAX     512   /* RFC 1459 line limit incl. CRLF */
#define IRC_TX_MAX       2048  /* outbound queue */
#define IRC_INPUT_MAX    300   /* longest message typed on the keypad */

/* Scrollback: fixed ring, oldest lines are overwritten */
#define IRC_LOG_LINES    100
#define IRC_LOG_LINE_MAX 256

/* Seconds to wait for the TCP connection before giving up */
#define RIRC_CONNECT_TIMEOUT_S 30

#endif
