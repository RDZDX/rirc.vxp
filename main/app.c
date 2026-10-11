/* rirc.vxp application glue, MIT licensed (see LICENSE). */
#include "app.h"

#include "net_tcp.h"
#include "rirc_config.h"

static irc_client_t client;
static int connect_ticks;

irc_client_t *
app_client(void)
{
	return &client;
}

static void
pump_tx(void)
{
	size_t len;
	const char *p;

	if (!net_active())
		return;

	p = irc_client_tx_peek(&client, &len);
	if (len) {
		int n = net_send(p, (int)len);
		if (n > 0)
			irc_client_tx_consume(&client, (size_t)n);
	}
}

static void
pump_rx(void)
{
	char buf[256];
	int n;

	while (net_active() && (n = net_recv(buf, sizeof(buf))) > 0)
		irc_client_feed(&client, buf, (size_t)n);
}

static void
net_handler(net_event_t ev)
{
	switch (ev) {
	case NET_EV_CONNECTED:
		irc_client_on_connected(&client);
		pump_tx();
		break;
	case NET_EV_READABLE:
		pump_rx();
		pump_tx(); /* PONG etc. */
		break;
	case NET_EV_WRITABLE:
		pump_tx();
		break;
	case NET_EV_HOST_NOT_FOUND:
		irc_client_on_closed(&client, "host not found");
		break;
	case NET_EV_CLOSED:
		irc_client_on_closed(&client, "connection closed");
		break;
	}
}

void
app_init(void)
{
	irc_client_init(&client);
	connect_ticks = 0;
}

void
app_shutdown(void)
{
	if (net_active()) {
		irc_client_request_quit(&client);
		pump_tx();
		net_close();
	}
}

void
app_connect(void)
{
	if (irc_client_begin_connect(&client) != 0)
		return;

	connect_ticks = 0;
	if (net_connect(client.cfg.server, net_handler) != 0)
		irc_client_on_closed(&client, "cannot start TCP connection");
}

void
app_disconnect(void)
{
	if (client.state == IRC_ST_DISCONNECTED)
		return;

	irc_client_request_quit(&client);
	pump_tx();
	net_close();
	irc_client_on_closed(&client, "by user");
}

void
app_send_line(const char *text)
{
	irc_client_input(&client, text);
	pump_tx();
}

void
app_tick(void)
{
	if (client.state == IRC_ST_CONNECTING) {
		if (++connect_ticks > (RIRC_CONNECT_TIMEOUT_S * 1000) / APP_TICK_MS) {
			net_close();
			irc_client_on_closed(&client, "connect timeout");
		}
	}

	/* retry data the socket did not accept earlier */
	pump_tx();
}
