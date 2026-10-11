/*
 * See irc_client.h. rirc.vxp, MIT licensed (see LICENSE).
 * Behaviour modelled on rirc, Copyright (C) 2014-2023 Richard Robbins.
 */
#include "irc_client.h"
#include "irc_parse.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define IRC_USER_REALNAME "rirc.vxp"

static void
copy_str(char *dst, size_t dstsz, const char *src)
{
	size_t n = strlen(src);

	if (n >= dstsz)
		n = dstsz - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

/* Replace anything that could break out of an IRC line */
static void
scrub(char *s)
{
	for (; *s; s++) {
		if (*s == '\r' || *s == '\n')
			*s = ' ';
	}
}

static void
logf_(irc_client_t *c, irc_log_kind_t kind, const char *fmt, ...)
{
	char buf[IRC_LOG_LINE_MAX];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	irc_log_add(&c->log, kind, buf);
}

/* Queue "<formatted>\r\n"; the formatted part is capped at 510 bytes. */
static int
tx_queue(irc_client_t *c, const char *fmt, ...)
{
	char buf[IRC_LINE_MAX - 1];
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (n < 0)
		return -1;
	if ((size_t)n >= sizeof(buf))
		n = (int)sizeof(buf) - 1;

	scrub(buf);

	if (c->tx_len + (unsigned)n + 2 > IRC_TX_MAX) {
		irc_log_add(&c->log, IRC_LOG_ERROR, "Output buffer full, message dropped");
		return -1;
	}

	memcpy(c->tx + c->tx_len, buf, (size_t)n);
	c->tx_len += (unsigned)n;
	c->tx[c->tx_len++] = '\r';
	c->tx[c->tx_len++] = '\n';

	return 0;
}

static void
normalize_channel(char *out, size_t outsz, const char *in)
{
	size_t n = 0;

	if (!strchr("#&+!", in[0]) && outsz > 1)
		out[n++] = '#';

	while (*in && n + 1 < outsz)
		out[n++] = *in++;
	out[n] = '\0';
}

void
irc_client_init(irc_client_t *c)
{
	memset(c, 0, sizeof(*c));
	irc_log_init(&c->log);
	c->state = IRC_ST_DISCONNECTED;
	copy_str(c->cfg.server, sizeof(c->cfg.server), RIRC_DEFAULT_SERVER);
	copy_str(c->cfg.nick, sizeof(c->cfg.nick), RIRC_DEFAULT_NICK);
	copy_str(c->cfg.channel, sizeof(c->cfg.channel), RIRC_DEFAULT_CHANNEL);
	irc_log_add(&c->log, IRC_LOG_INFO, "rirc.vxp - plain IRC (port 6667). Open Menu to connect.");
}

int
irc_client_set_server(irc_client_t *c, const char *server)
{
	if (!irc_valid_server(server))
		return -1;
	copy_str(c->cfg.server, sizeof(c->cfg.server), server);
	return 0;
}

int
irc_client_set_nick(irc_client_t *c, const char *nick)
{
	if (!irc_valid_nick(nick))
		return -1;
	copy_str(c->cfg.nick, sizeof(c->cfg.nick), nick);
	return 0;
}

int
irc_client_set_channel(irc_client_t *c, const char *channel)
{
	char tmp[IRC_CHAN_MAX];

	if (!channel || !*channel) {
		c->cfg.channel[0] = '\0';
		return 0;
	}

	normalize_channel(tmp, sizeof(tmp), channel);
	if (!irc_valid_channel(tmp))
		return -1;
	copy_str(c->cfg.channel, sizeof(c->cfg.channel), tmp);
	return 0;
}

int
irc_client_begin_connect(irc_client_t *c)
{
	if (c->state != IRC_ST_DISCONNECTED || !c->cfg.server[0])
		return -1;

	c->state = IRC_ST_CONNECTING;
	c->rx_len = 0;
	c->rx_overflow = 0;
	c->tx_len = 0;
	c->nick_retries = 0;
	c->target[0] = '\0';
	copy_str(c->nick, sizeof(c->nick), c->cfg.nick);
	logf_(c, IRC_LOG_INFO, "Connecting to %s:%d ...", c->cfg.server, RIRC_IRC_PORT);
	return 0;
}

void
irc_client_on_connected(irc_client_t *c)
{
	if (c->state != IRC_ST_CONNECTING)
		return;

	c->state = IRC_ST_REGISTERING;
	irc_log_add(&c->log, IRC_LOG_INFO, "Connected, registering...");
	tx_queue(c, "NICK %s", c->nick);
	tx_queue(c, "USER %s 0 * :%s", c->nick, IRC_USER_REALNAME);
}

void
irc_client_on_closed(irc_client_t *c, const char *reason)
{
	int was_idle = (c->state == IRC_ST_DISCONNECTED);

	c->state = IRC_ST_DISCONNECTED;
	c->rx_len = 0;
	c->rx_overflow = 0;
	c->tx_len = 0;
	c->target[0] = '\0';

	if (!was_idle)
		logf_(c, IRC_LOG_ERROR, "Disconnected: %s", reason ? reason : "closed");
}

void
irc_client_request_quit(irc_client_t *c)
{
	if (c->state == IRC_ST_REGISTERING || c->state == IRC_ST_ONLINE)
		tx_queue(c, "QUIT :rirc.vxp");
}

const char *
irc_state_name(irc_state_t st)
{
	switch (st) {
	case IRC_ST_CONNECTING:  return "connecting";
	case IRC_ST_REGISTERING: return "registering";
	case IRC_ST_ONLINE:      return "online";
	default:                 return "offline";
	}
}

const char *
irc_client_tx_peek(const irc_client_t *c, size_t *len)
{
	*len = c->tx_len;
	return c->tx;
}

void
irc_client_tx_consume(irc_client_t *c, size_t n)
{
	if (n >= c->tx_len) {
		c->tx_len = 0;
		return;
	}
	memmove(c->tx, c->tx + n, c->tx_len - n);
	c->tx_len -= (unsigned)n;
}

/* ---- incoming messages ------------------------------------------------ */

static int
is_me(const irc_client_t *c, const char *nick)
{
	return irc_strcasecmp(c->nick, nick) == 0;
}

static const char *
param(const irc_msg_t *m, int i)
{
	return i < m->nparams ? m->params[i] : "";
}

static int
is_numeric(const char *cmd)
{
	return cmd[0] >= '0' && cmd[0] <= '9' && cmd[1] >= '0' && cmd[1] <= '9' &&
	       cmd[2] >= '0' && cmd[2] <= '9' && !cmd[3];
}

static void
handle_privmsg(irc_client_t *c, const irc_msg_t *m, int notice)
{
	char who[IRC_NICK_MAX];
	const char *to = param(m, 0);
	char *text = (char *)param(m, 1);
	size_t len = strlen(text);
	int channel_msg = strchr("#&+!", to[0]) != NULL;
	int other_chan = channel_msg && irc_strcasecmp(to, c->target) != 0;

	irc_prefix_nick(m->prefix, who, sizeof(who));
	if (!who[0])
		copy_str(who, sizeof(who), "server");

	if (len >= 2 && text[0] == '\x01') {
		/* CTCP: only ACTION (/me) is displayed */
		if (len > 8 && !strncmp(text + 1, "ACTION ", 7)) {
			text += 8;
			if (*text && text[strlen(text) - 1] == '\x01')
				text[strlen(text) - 1] = '\0';
			if (other_chan || (!channel_msg && !notice))
				logf_(c, IRC_LOG_CHAT, "[%s] * %s %s", other_chan ? to : "pm", who, text);
			else
				logf_(c, IRC_LOG_CHAT, "* %s %s", who, text);
		}
		return;
	}

	if (notice)
		logf_(c, IRC_LOG_CHAT, "-%s- %s", who, text);
	else if (other_chan)
		logf_(c, IRC_LOG_CHAT, "[%s] <%s> %s", to, who, text);
	else if (!channel_msg)
		logf_(c, IRC_LOG_CHAT, "[pm] <%s> %s", who, text);
	else
		logf_(c, IRC_LOG_CHAT, "<%s> %s", who, text);
}

static void
handle_numeric(irc_client_t *c, const irc_msg_t *m)
{
	char buf[IRC_LOG_LINE_MAX];
	int code = (m->command[0] - '0') * 100 + (m->command[1] - '0') * 10 + (m->command[2] - '0');
	size_t n = 0;
	int i;

	switch (code) {
	case 1:
		c->state = IRC_ST_ONLINE;
		if (m->nparams > 0 && irc_valid_nick(m->params[0]))
			copy_str(c->nick, sizeof(c->nick), m->params[0]);
		logf_(c, IRC_LOG_INFO, "%s", param(m, m->nparams > 1 ? 1 : 0));
		if (c->cfg.channel[0])
			tx_queue(c, "JOIN %s", c->cfg.channel);
		return;
	case 366: /* end of names */
	case 315:
		return;
	case 332:
		logf_(c, IRC_LOG_INFO, "Topic for %s: %s", param(m, 1), param(m, 2));
		return;
	case 432: /* erroneous nickname: retrying with a suffix cannot help */
		logf_(c, IRC_LOG_ERROR, "Invalid nickname: %s", param(m, 1));
		return;
	case 433: /* nick in use */
	case 436:
		if (c->state != IRC_ST_ONLINE && c->nick_retries < 4 && strlen(c->nick) + 1 < sizeof(c->nick)) {
			size_t l = strlen(c->nick);
			c->nick[l] = '_';
			c->nick[l + 1] = '\0';
			c->nick_retries++;
			logf_(c, IRC_LOG_INFO, "Nick unavailable, trying %s", c->nick);
			tx_queue(c, "NICK %s", c->nick);
		} else {
			logf_(c, IRC_LOG_ERROR, "Nickname rejected: %s", param(m, m->nparams - 1));
		}
		return;
	default:
		break;
	}

	/* generic: params after our own nick, space separated */
	buf[0] = '\0';
	for (i = 1; i < m->nparams && n + 1 < sizeof(buf); i++) {
		int w = snprintf(buf + n, sizeof(buf) - n, "%s%s", n ? " " : "", m->params[i]);
		if (w < 0)
			break;
		n += (size_t)w;
		if (n >= sizeof(buf)) {
			n = sizeof(buf) - 1;
			break;
		}
	}
	if (buf[0])
		irc_log_add(&c->log, code >= 400 ? IRC_LOG_ERROR : IRC_LOG_INFO, buf);
}

static void
handle_message(irc_client_t *c, irc_msg_t *m)
{
	char who[IRC_NICK_MAX];
	const char *cmd = m->command;

	if (!irc_strcasecmp(cmd, "PING")) {
		tx_queue(c, "PONG :%s", m->nparams ? m->params[m->nparams - 1] : "");
		return;
	}

	if (is_numeric(cmd)) {
		handle_numeric(c, m);
		return;
	}

	irc_prefix_nick(m->prefix, who, sizeof(who));

	if (!irc_strcasecmp(cmd, "PRIVMSG")) {
		handle_privmsg(c, m, 0);
	} else if (!irc_strcasecmp(cmd, "NOTICE")) {
		handle_privmsg(c, m, 1);
	} else if (!irc_strcasecmp(cmd, "JOIN")) {
		const char *chan = param(m, 0);
		if (is_me(c, who)) {
			copy_str(c->target, sizeof(c->target), chan);
			logf_(c, IRC_LOG_EVENT, "Joined %s", chan);
		} else {
			logf_(c, IRC_LOG_EVENT, "--> %s joined %s", who, chan);
		}
	} else if (!irc_strcasecmp(cmd, "PART")) {
		const char *chan = param(m, 0);
		if (is_me(c, who)) {
			if (!irc_strcasecmp(chan, c->target))
				c->target[0] = '\0';
			logf_(c, IRC_LOG_EVENT, "Left %s", chan);
		} else {
			logf_(c, IRC_LOG_EVENT, "<-- %s left %s", who, chan);
		}
	} else if (!irc_strcasecmp(cmd, "QUIT")) {
		logf_(c, IRC_LOG_EVENT, "<-- %s quit (%s)", who, param(m, 0));
	} else if (!irc_strcasecmp(cmd, "NICK")) {
		if (is_me(c, who))
			copy_str(c->nick, sizeof(c->nick), param(m, 0));
		logf_(c, IRC_LOG_EVENT, "%s is now known as %s", who, param(m, 0));
	} else if (!irc_strcasecmp(cmd, "KICK")) {
		if (is_me(c, param(m, 1)) && !irc_strcasecmp(param(m, 0), c->target))
			c->target[0] = '\0';
		logf_(c, IRC_LOG_EVENT, "%s kicked %s from %s (%s)", who, param(m, 1), param(m, 0), param(m, 2));
	} else if (!irc_strcasecmp(cmd, "TOPIC")) {
		logf_(c, IRC_LOG_EVENT, "%s set topic of %s: %s", who, param(m, 0), param(m, 1));
	} else if (!irc_strcasecmp(cmd, "ERROR")) {
		logf_(c, IRC_LOG_ERROR, "Server error: %s", param(m, 0));
	}
	/* everything else is ignored */
}

static void
process_line(irc_client_t *c, char *line)
{
	irc_msg_t msg;

	if (irc_parse(line, &msg) == 0)
		handle_message(c, &msg);
}

void
irc_client_feed(irc_client_t *c, const char *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		char ch = data[i];

		if (ch == '\n') {
			if (c->rx_len && c->rx[c->rx_len - 1] == '\r')
				c->rx_len--;
			c->rx[c->rx_len] = '\0';
			if (!c->rx_overflow && c->rx_len)
				process_line(c, c->rx);
			c->rx_len = 0;
			c->rx_overflow = 0;
		} else if (c->rx_len < sizeof(c->rx) - 1) {
			c->rx[c->rx_len++] = ch;
		} else {
			c->rx_overflow = 1; /* over-long line: dropped at the next LF */
		}
	}
}

/* ---- user input ------------------------------------------------------- */

static void
send_chat(irc_client_t *c, const char *text, int action)
{
	/* "PRIVMSG " + target + " :" must leave room inside 510 bytes */
	if (c->state != IRC_ST_ONLINE) {
		irc_log_add(&c->log, IRC_LOG_ERROR, "Not connected");
		return;
	}
	if (!c->target[0]) {
		irc_log_add(&c->log, IRC_LOG_ERROR, "No channel. Use /join #channel");
		return;
	}

	if (action) {
		tx_queue(c, "PRIVMSG %s :\x01" "ACTION %s\x01", c->target, text);
		logf_(c, IRC_LOG_SELF, "* %s %s", c->nick, text);
	} else {
		tx_queue(c, "PRIVMSG %s :%s", c->target, text);
		logf_(c, IRC_LOG_SELF, "<%s> %s", c->nick, text);
	}
}

static void
show_help(irc_client_t *c)
{
	irc_log_add(&c->log, IRC_LOG_INFO, "/join #chan  /part [#chan]  /nick name");
	irc_log_add(&c->log, IRC_LOG_INFO, "/me action  /msg nick text  /quit");
}

static void
handle_command(irc_client_t *c, char *cmd)
{
	char *arg = cmd;

	while (*arg && *arg != ' ')
		arg++;
	if (*arg)
		*arg++ = '\0';
	while (*arg == ' ')
		arg++;

	if (!irc_strcasecmp(cmd, "help")) {
		show_help(c);
	} else if (!irc_strcasecmp(cmd, "me")) {
		if (*arg)
			send_chat(c, arg, 1);
	} else if (!irc_strcasecmp(cmd, "join") || !irc_strcasecmp(cmd, "j")) {
		char chan[IRC_CHAN_MAX];
		char *end = arg;

		while (*end && *end != ' ')
			end++;
		*end = '\0';
		normalize_channel(chan, sizeof(chan), arg);
		if (c->state != IRC_ST_ONLINE)
			irc_log_add(&c->log, IRC_LOG_ERROR, "Not connected");
		else if (!*arg || !irc_valid_channel(chan))
			irc_log_add(&c->log, IRC_LOG_ERROR, "Usage: /join #channel");
		else
			tx_queue(c, "JOIN %s", chan);
	} else if (!irc_strcasecmp(cmd, "part")) {
		char chan[IRC_CHAN_MAX];

		copy_str(chan, sizeof(chan), *arg ? arg : c->target);
		if (c->state != IRC_ST_ONLINE)
			irc_log_add(&c->log, IRC_LOG_ERROR, "Not connected");
		else if (!irc_valid_channel(chan))
			irc_log_add(&c->log, IRC_LOG_ERROR, "Usage: /part [#channel]");
		else
			tx_queue(c, "PART %s", chan);
	} else if (!irc_strcasecmp(cmd, "nick")) {
		if (!irc_valid_nick(arg)) {
			irc_log_add(&c->log, IRC_LOG_ERROR, "Invalid nickname");
		} else if (c->state == IRC_ST_ONLINE) {
			tx_queue(c, "NICK %s", arg);
		} else {
			irc_client_set_nick(c, arg);
			irc_log_add(&c->log, IRC_LOG_INFO, "Nickname saved");
		}
	} else if (!irc_strcasecmp(cmd, "msg") || !irc_strcasecmp(cmd, "query")) {
		char *text = arg;

		while (*text && *text != ' ')
			text++;
		if (*text)
			*text++ = '\0';
		while (*text == ' ')
			text++;

		if (c->state != IRC_ST_ONLINE)
			irc_log_add(&c->log, IRC_LOG_ERROR, "Not connected");
		else if (!*arg || !*text)
			irc_log_add(&c->log, IRC_LOG_ERROR, "Usage: /msg target text");
		else if (!irc_valid_nick(arg) && !irc_valid_channel(arg))
			irc_log_add(&c->log, IRC_LOG_ERROR, "Invalid target");
		else {
			tx_queue(c, "PRIVMSG %s :%s", arg, text);
			logf_(c, IRC_LOG_SELF, "[%s] <%s> %s", arg, c->nick, text);
		}
	} else if (!irc_strcasecmp(cmd, "quit")) {
		if (c->state == IRC_ST_REGISTERING || c->state == IRC_ST_ONLINE)
			tx_queue(c, "QUIT :%s", *arg ? arg : "rirc.vxp");
	} else {
		irc_log_add(&c->log, IRC_LOG_ERROR, "Unknown command. Try /help");
	}
}

void
irc_client_input(irc_client_t *c, const char *text)
{
	char buf[IRC_INPUT_MAX + 1];

	while (*text == ' ')
		text++;
	if (!*text)
		return;

	copy_str(buf, sizeof(buf), text);
	scrub(buf);

	if (buf[0] == '/' && buf[1] != '/' && buf[1])
		handle_command(c, buf + 1);
	else
		send_chat(c, buf[0] == '/' ? buf + 1 : buf, 0);
}
