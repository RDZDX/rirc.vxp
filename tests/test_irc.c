/* Host-side unit tests for the portable IRC core (irc/). */
#include <stdio.h>
#include <string.h>

#include "irc_client.h"
#include "irc_parse.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const char *last_line(const irc_client_t *c)
{
	const irc_log_line_t *l = irc_log_get(&c->log, c->log.count - 1);
	return l ? l->text : "";
}

static int tx_contains(irc_client_t *c, const char *s)
{
	size_t len;
	const char *p = irc_client_tx_peek(c, &len);
	char tmp[IRC_TX_MAX + 1];

	memcpy(tmp, p, len);
	tmp[len] = '\0';
	return strstr(tmp, s) != NULL;
}

static void feed(irc_client_t *c, const char *s)
{
	irc_client_feed(c, s, strlen(s));
}

static void online(irc_client_t *c)
{
	irc_client_init(c);
	irc_client_set_nick(c, "tester");
	irc_client_set_server(c, "irc.example.org");
	irc_client_set_channel(c, "chan");
	CHECK(irc_client_begin_connect(c) == 0);
	irc_client_on_connected(c);
	feed(c, ":srv 001 tester :Welcome\r\n");
	irc_client_tx_consume(c, c->tx_len);
	feed(c, ":tester!u@h JOIN #chan\r\n");
}

static void test_parse(void)
{
	char l1[] = ":nick!user@host PRIVMSG #c :hello world";
	char l2[] = "PING :irc.example.org";
	char l3[] = "@time=1 :srv 001 me :Welcome to IRC";
	char l4[] = "";
	char l5[] = ":prefixonly";
	irc_msg_t m;
	char nick[IRC_NICK_MAX];

	CHECK(irc_parse(l1, &m) == 0);
	CHECK(!strcmp(m.prefix, "nick!user@host") && !strcmp(m.command, "PRIVMSG"));
	CHECK(m.nparams == 2 && !strcmp(m.params[0], "#c") && !strcmp(m.params[1], "hello world"));
	irc_prefix_nick(m.prefix, nick, sizeof(nick));
	CHECK(!strcmp(nick, "nick"));

	CHECK(irc_parse(l2, &m) == 0 && m.prefix == NULL && m.nparams == 1);
	CHECK(!strcmp(m.params[0], "irc.example.org"));

	CHECK(irc_parse(l3, &m) == 0 && !strcmp(m.command, "001") && m.nparams == 2);
	CHECK(irc_parse(l4, &m) == -1);
	CHECK(irc_parse(l5, &m) == -1);

	CHECK(irc_valid_nick("abc_1") && !irc_valid_nick("1abc") && !irc_valid_nick("a b"));
	CHECK(!irc_valid_nick("abcdefghijklmnopq"));
	CHECK(irc_valid_channel("#a") && !irc_valid_channel("a") && !irc_valid_channel("#a b"));
	CHECK(irc_valid_server("irc.libera.chat") && !irc_valid_server("a b") && !irc_valid_server(""));
	CHECK(irc_strcasecmp("#ChAn", "#chan") == 0);
}

static void test_handshake_and_ping(void)
{
	irc_client_t c;

	irc_client_init(&c);
	irc_client_set_nick(&c, "tester");
	CHECK(irc_client_set_channel(&c, "chan") == 0 && !strcmp(c.cfg.channel, "#chan"));
	CHECK(irc_client_begin_connect(&c) == 0 && c.state == IRC_ST_CONNECTING);
	CHECK(irc_client_begin_connect(&c) == -1);
	irc_client_on_connected(&c);
	CHECK(c.state == IRC_ST_REGISTERING);
	CHECK(tx_contains(&c, "NICK tester\r\n") && tx_contains(&c, "USER tester 0 * :"));
	irc_client_tx_consume(&c, c.tx_len);

	feed(&c, "PING :abc123\r\n");
	CHECK(tx_contains(&c, "PONG :abc123\r\n"));
	irc_client_tx_consume(&c, c.tx_len);

	/* split across chunks */
	feed(&c, ":srv 001 tester :Wel");
	CHECK(c.state == IRC_ST_REGISTERING);
	feed(&c, "come\r\n");
	CHECK(c.state == IRC_ST_ONLINE);
	CHECK(tx_contains(&c, "JOIN #chan\r\n"));

	feed(&c, ":tester!u@h JOIN #chan\r\n");
	CHECK(!strcmp(c.target, "#chan"));
}

static void test_nick_in_use(void)
{
	irc_client_t c;

	irc_client_init(&c);
	irc_client_set_nick(&c, "tester");
	irc_client_begin_connect(&c);
	irc_client_on_connected(&c);
	irc_client_tx_consume(&c, c.tx_len);
	feed(&c, ":srv 433 * tester :Nickname is already in use\r\n");
	CHECK(!strcmp(c.nick, "tester_") && tx_contains(&c, "NICK tester_\r\n"));
}

static void test_nick_errors(void)
{
	irc_client_t c;
	int i;

	irc_client_init(&c);
	irc_client_set_nick(&c, "tester");
	irc_client_begin_connect(&c);
	irc_client_on_connected(&c);
	irc_client_tx_consume(&c, c.tx_len);
	feed(&c, ":srv 432 * tester :Erroneous nickname\r\n");
	CHECK(!strcmp(c.nick, "tester") && c.tx_len == 0);

	for (i = 0; i < 8; i++)
		feed(&c, ":srv 433 * tester :in use\r\n");
	CHECK(!strcmp(c.nick, "tester____"));
}

static void test_events(void)
{
	irc_client_t c;

	online(&c);
	feed(&c, ":srv 332 tester #chan :The topic\r\n");
	CHECK(!strcmp(last_line(&c), "Topic for #chan: The topic"));
	feed(&c, ":op!o@h TOPIC #chan :new topic\r\n");
	CHECK(!strcmp(last_line(&c), "op set topic of #chan: new topic"));
	feed(&c, ":bob!b@h QUIT :bye\r\n");
	CHECK(!strcmp(last_line(&c), "<-- bob quit (bye)"));
	feed(&c, ":srv ERROR :Closing link\r\n");
	CHECK(!strcmp(last_line(&c), "Server error: Closing link"));
	feed(&c, ":bob!b@h PRIVMSG #chan :\x01" "ACTION \x01\r\n");
	feed(&c, ":op!o@h KICK #chan tester :bye\r\n");
	CHECK(c.target[0] == '\0');

	irc_client_input(&c, "/msg #other hello");
	CHECK(tx_contains(&c, "PRIVMSG #other :hello\r\n"));
	irc_client_tx_consume(&c, c.tx_len);
	irc_client_input(&c, "/nick newnick");
	CHECK(tx_contains(&c, "NICK newnick\r\n"));
	irc_client_tx_consume(&c, c.tx_len);
	irc_client_request_quit(&c);
	CHECK(tx_contains(&c, "QUIT :"));
}

static void test_messages(void)
{
	irc_client_t c;

	online(&c);
	feed(&c, ":bob!b@h PRIVMSG #chan :hi there\r\n");
	CHECK(!strcmp(last_line(&c), "<bob> hi there"));
	feed(&c, ":bob!b@h PRIVMSG #other :yo\r\n");
	CHECK(!strcmp(last_line(&c), "[#other] <bob> yo"));
	feed(&c, ":bob!b@h PRIVMSG tester :psst\r\n");
	CHECK(!strcmp(last_line(&c), "[pm] <bob> psst"));
	feed(&c, ":bob!b@h PRIVMSG #chan :\x01" "ACTION waves\x01\r\n");
	CHECK(!strcmp(last_line(&c), "* bob waves"));
	feed(&c, ":bob!b@h NOTICE tester :note\r\n");
	CHECK(!strcmp(last_line(&c), "-bob- note"));
	feed(&c, ":bob!b@h JOIN #chan\r\n");
	CHECK(!strcmp(last_line(&c), "--> bob joined #chan"));
	feed(&c, ":bob!b@h NICK :robert\r\n");
	CHECK(!strcmp(last_line(&c), "bob is now known as robert"));
	feed(&c, ":srv 404 tester #chan :Cannot send to channel\r\n");
	CHECK(c.log.lines[(c.log.head + c.log.count - 1) % IRC_LOG_LINES].kind == IRC_LOG_ERROR);
	feed(&c, ":x!y@z PRIVMSG #chan :bad\x01\x02 ctl\r\n");
	CHECK(!strcmp(last_line(&c), "<x> bad?? ctl"));
}

static void test_user_input(void)
{
	irc_client_t c;

	online(&c);
	irc_client_input(&c, "hello world");
	CHECK(tx_contains(&c, "PRIVMSG #chan :hello world\r\n"));
	CHECK(!strcmp(last_line(&c), "<tester> hello world"));
	irc_client_tx_consume(&c, c.tx_len);

	irc_client_input(&c, "/me dances");
	CHECK(tx_contains(&c, "PRIVMSG #chan :\x01" "ACTION dances\x01\r\n"));
	irc_client_tx_consume(&c, c.tx_len);

	irc_client_input(&c, "/join foo");
	CHECK(tx_contains(&c, "JOIN #foo\r\n"));
	irc_client_tx_consume(&c, c.tx_len);

	irc_client_input(&c, "/msg bob secret stuff");
	CHECK(tx_contains(&c, "PRIVMSG bob :secret stuff\r\n"));
	irc_client_tx_consume(&c, c.tx_len);

	irc_client_input(&c, "//not a command");
	CHECK(tx_contains(&c, "PRIVMSG #chan :/not a command\r\n"));
	irc_client_tx_consume(&c, c.tx_len);

	irc_client_input(&c, "/bogus");
	CHECK(c.tx_len == 0 && !strncmp(last_line(&c), "Unknown", 7));

	irc_client_input(&c, "/part");
	CHECK(tx_contains(&c, "PART #chan\r\n"));
	irc_client_tx_consume(&c, c.tx_len);
	feed(&c, ":tester!u@h PART #chan\r\n");
	CHECK(c.target[0] == '\0');
	irc_client_input(&c, "anyone?");
	CHECK(c.tx_len == 0 && !strncmp(last_line(&c), "No channel", 10));

	irc_client_on_closed(&c, "gone");
	irc_client_input(&c, "x");
	CHECK(c.tx_len == 0 && !strcmp(last_line(&c), "Not connected"));
	CHECK(c.state == IRC_ST_DISCONNECTED);
}

static void test_injection_and_limits(void)
{
	irc_client_t c;
	char big[2000];
	int i;

	online(&c);
	irc_client_input(&c, "line\r\nQUIT :pwned");
	CHECK(!tx_contains(&c, "\r\nQUIT"));

	irc_client_tx_consume(&c, c.tx_len);
	memset(big, 'a', sizeof(big) - 1);
	big[sizeof(big) - 1] = '\0';
	irc_client_input(&c, big);
	CHECK(c.tx_len <= IRC_LINE_MAX);

	/* over-long incoming line is dropped, next line still parsed */
	irc_client_tx_consume(&c, c.tx_len);
	for (i = 0; i < 700; i++)
		feed(&c, "x");
	feed(&c, "\r\nPING :ok\r\n");
	CHECK(tx_contains(&c, "PONG :ok\r\n"));

	/* outbound queue never overflows */
	for (i = 0; i < 100; i++)
		irc_client_input(&c, "flood flood flood flood flood flood flood flood flood flood flood");
	CHECK(c.tx_len <= IRC_TX_MAX);
}

static void test_log_ring(void)
{
	irc_log_t log;
	char buf[16];
	int i;

	irc_log_init(&log);
	for (i = 0; i < IRC_LOG_LINES + 5; i++) {
		snprintf(buf, sizeof(buf), "l%d", i);
		irc_log_add(&log, IRC_LOG_INFO, buf);
	}
	CHECK(log.count == IRC_LOG_LINES);
	CHECK(!strcmp(irc_log_get(&log, 0)->text, "l5"));
	CHECK(!strcmp(irc_log_get(&log, IRC_LOG_LINES - 1)->text, "l104"));
	CHECK(irc_log_get(&log, IRC_LOG_LINES) == NULL);
}

int main(void)
{
	test_parse();
	test_handshake_and_ping();
	test_nick_in_use();
	test_nick_errors();
	test_events();
	test_messages();
	test_user_input();
	test_injection_and_limits();
	test_log_ring();

	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
