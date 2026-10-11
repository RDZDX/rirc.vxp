/* See irc_parse.h. rirc.vxp, MIT licensed (see LICENSE). */
#include "irc_parse.h"

#include <string.h>

int
irc_parse(char *line, irc_msg_t *msg)
{
	char *p = line;

	memset(msg, 0, sizeof(*msg));

	if (!p)
		return -1;

	while (*p == ' ')
		p++;

	/* IRCv3 message tags are not used; skip them */
	if (*p == '@') {
		while (*p && *p != ' ')
			p++;
		while (*p == ' ')
			p++;
	}

	if (*p == ':') {
		msg->prefix = ++p;
		while (*p && *p != ' ')
			p++;
		if (*p)
			*p++ = '\0';
		while (*p == ' ')
			p++;
		if (!*msg->prefix)
			return -1;
	}

	if (!*p)
		return -1;

	msg->command = p;
	while (*p && *p != ' ')
		p++;
	if (*p)
		*p++ = '\0';

	while (*p) {
		while (*p == ' ')
			p++;
		if (!*p)
			break;
		if (msg->nparams == IRC_MAX_PARAMS)
			break;
		if (*p == ':') {
			msg->params[msg->nparams++] = p + 1;
			break;
		}
		msg->params[msg->nparams++] = p;
		while (*p && *p != ' ')
			p++;
		if (*p)
			*p++ = '\0';
	}

	return 0;
}

void
irc_prefix_nick(const char *prefix, char *out, size_t outsz)
{
	size_t n = 0;

	if (!outsz)
		return;

	if (prefix) {
		while (prefix[n] && prefix[n] != '!' && prefix[n] != '@' && n + 1 < outsz) {
			out[n] = prefix[n];
			n++;
		}
	}
	out[n] = '\0';
}

static char
lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

int
irc_strcasecmp(const char *a, const char *b)
{
	while (*a && lower(*a) == lower(*b)) {
		a++;
		b++;
	}
	return (unsigned char)lower(*a) - (unsigned char)lower(*b);
}

static int
is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int
is_digit(char c)
{
	return c >= '0' && c <= '9';
}

int
irc_valid_nick(const char *s)
{
	size_t n = 0;

	if (!s || !*s)
		return 0;

	for (; *s; s++, n++) {
		if (is_alpha(*s) || strchr("[]\\`_^{|}", *s))
			continue;
		if (n > 0 && (is_digit(*s) || *s == '-'))
			continue;
		return 0;
	}

	return n < 17;
}

int
irc_valid_channel(const char *s)
{
	size_t n;

	if (!s || !strchr("#&+!", s[0]) || !s[0])
		return 0;

	for (n = 1; s[n]; n++) {
		if ((unsigned char)s[n] <= ' ' || (unsigned char)s[n] >= 0x7f || s[n] == ',' || s[n] == ':')
			return 0;
	}

	return n > 1 && n < 33;
}

int
irc_valid_server(const char *s)
{
	size_t n = 0;

	if (!s || !*s)
		return 0;

	for (; *s; s++, n++) {
		if (is_alpha(*s) || is_digit(*s) || *s == '.' || *s == '-' || *s == '_')
			continue;
		return 0;
	}

	return n < 64;
}
