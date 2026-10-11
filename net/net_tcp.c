/* See net_tcp.h. rirc.vxp, MIT licensed (see LICENSE). */
#include "net_tcp.h"

#include <vmsys.h>
#include <vmsock.h>

#include "rirc_config.h"

/* Access point id as used by the reference projects (gpt.vxp, IPoverObexVxp) */
#define NET_APN 1

static VMINT32 sock_id = -1;
static net_event_fn on_event = 0;

static void
tcp_callback(VMINT handle, VMINT event)
{
	net_event_fn cb = on_event;
	net_event_t ev;

	if (handle != sock_id || !cb)
		return;

	switch (event) {
	case VM_TCP_EVT_CONNECTED:
		ev = NET_EV_CONNECTED;
		break;
	case VM_TCP_EVT_CAN_READ:
		ev = NET_EV_READABLE;
		break;
	case VM_TCP_EVT_CAN_WRITE:
		ev = NET_EV_WRITABLE;
		break;
	case VM_TCP_EVT_HOST_NOT_FOUND:
		ev = NET_EV_HOST_NOT_FOUND;
		break;
	case VM_TCP_EVT_PIPE_BROKEN:
	case VM_TCP_EVT_PIPE_CLOSED:
		ev = NET_EV_CLOSED;
		break;
	default:
		return;
	}

	if (ev == NET_EV_HOST_NOT_FOUND || ev == NET_EV_CLOSED)
		net_close();

	cb(ev);
}

int
net_connect(const char *host, net_event_fn cb)
{
	if (sock_id >= 0)
		return -1;

	on_event = cb;
	sock_id = vm_tcp_connect(host, RIRC_IRC_PORT, NET_APN, tcp_callback);
	if (sock_id < 0) {
		sock_id = -1;
		return -1;
	}
	return 0;
}

void
net_close(void)
{
	if (sock_id >= 0) {
		VMINT32 id = sock_id;
		sock_id = -1;
		vm_tcp_close(id);
	}
}

int
net_active(void)
{
	return sock_id >= 0;
}

int
net_send(const char *buf, int len)
{
	int res;

	if (sock_id < 0 || len <= 0)
		return 0;

	res = vm_tcp_write(sock_id, (void *)buf, len);
	return res > 0 ? res : 0;
}

int
net_recv(char *buf, int len)
{
	int res;

	if (sock_id < 0 || len <= 0)
		return 0;

	res = vm_tcp_read(sock_id, buf, len);
	return res > 0 ? res : 0;
}
