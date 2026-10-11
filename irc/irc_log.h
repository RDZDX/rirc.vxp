/* Fixed-size scrollback ring buffer. rirc.vxp, MIT licensed (see LICENSE). */
#ifndef IRC_LOG_H
#define IRC_LOG_H

#include "rirc_config.h"

typedef enum {
	IRC_LOG_INFO,  /* client / server status */
	IRC_LOG_ERROR,
	IRC_LOG_CHAT,  /* messages from others */
	IRC_LOG_SELF,  /* messages typed by the user */
	IRC_LOG_EVENT  /* joins, parts, nick changes... */
} irc_log_kind_t;

typedef struct {
	char text[IRC_LOG_LINE_MAX];
	unsigned char kind;
} irc_log_line_t;

typedef struct {
	irc_log_line_t lines[IRC_LOG_LINES];
	unsigned head;  /* index of the oldest line */
	unsigned count;
	unsigned serial; /* incremented on every change (for redraw checks) */
} irc_log_t;

void irc_log_init(irc_log_t *log);
void irc_log_clear(irc_log_t *log);

/* Append a line; text is truncated to IRC_LOG_LINE_MAX-1 and made printable ASCII. */
void irc_log_add(irc_log_t *log, irc_log_kind_t kind, const char *text);

/* idx 0 is the oldest line. Returns NULL if out of range. */
const irc_log_line_t *irc_log_get(const irc_log_t *log, unsigned idx);

#endif
