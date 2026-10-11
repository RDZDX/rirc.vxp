/*
 * Small IRC message parser (RFC 1459 / 2812 style).
 *
 * Written for rirc.vxp; the message grammar and naming follow the upstream
 * rirc project (https://github.com/rcr/rirc, MIT, (C) Richard Robbins).
 * No MRE, POSIX or terminal dependencies.
 */
#ifndef IRC_PARSE_H
#define IRC_PARSE_H

#include <stddef.h>

#define IRC_MAX_PARAMS 15

typedef struct {
	const char *prefix;  /* "nick!user@host" or server name, NULL if absent */
	const char *command; /* "PRIVMSG", "001", ... */
	const char *params[IRC_MAX_PARAMS];
	int nparams;         /* the trailing parameter is the last entry */
} irc_msg_t;

/* Parse one line (no CRLF) in place. Returns 0 on success, -1 if invalid. */
int irc_parse(char *line, irc_msg_t *msg);

/* Copy the nick part of a prefix ("nick!user@host") into out. */
void irc_prefix_nick(const char *prefix, char *out, size_t outsz);

/* ASCII case-insensitive comparison (IRC casemapping, simplified). */
int irc_strcasecmp(const char *a, const char *b);

/* Validation helpers for user supplied configuration. */
int irc_valid_nick(const char *s);
int irc_valid_channel(const char *s);
int irc_valid_server(const char *s);

#endif
