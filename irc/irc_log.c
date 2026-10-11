/* See irc_log.h. rirc.vxp, MIT licensed (see LICENSE). */
#include "irc_log.h"

#include <string.h>

void
irc_log_init(irc_log_t *log)
{
	memset(log, 0, sizeof(*log));
}

void
irc_log_clear(irc_log_t *log)
{
	log->head = 0;
	log->count = 0;
	log->serial++;
}

void
irc_log_add(irc_log_t *log, irc_log_kind_t kind, const char *text)
{
	irc_log_line_t *line;
	unsigned slot;
	size_t i;

	if (log->count < IRC_LOG_LINES) {
		slot = (log->head + log->count) % IRC_LOG_LINES;
		log->count++;
	} else {
		slot = log->head;
		log->head = (log->head + 1) % IRC_LOG_LINES;
	}

	line = &log->lines[slot];
	line->kind = (unsigned char)kind;

	for (i = 0; text && text[i] && i < IRC_LOG_LINE_MAX - 1; i++) {
		unsigned char c = (unsigned char)text[i];
		line->text[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
	}
	line->text[i] = '\0';

	log->serial++;
}

const irc_log_line_t *
irc_log_get(const irc_log_t *log, unsigned idx)
{
	if (idx >= log->count)
		return NULL;
	return &log->lines[(log->head + idx) % IRC_LOG_LINES];
}
