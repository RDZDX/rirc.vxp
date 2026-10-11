# Design notes

```
 MRE events ──► main.c ──► ui.c (keys, drawing)          irc/  (portable, no MRE)
 vm_tcp cb  ──► net_tcp.c ─► app.c ──► irc_client_feed ─► irc_parse, irc_log
                              ▲        irc_client_input
                              └─ tx queue (irc_client_tx_peek / consume)
```

- `irc_client_t` never touches a socket. `app.c` feeds bytes from `net_recv` into
  `irc_client_feed`, and drains the output queue into `net_send` on connect, on
  `CAN_WRITE`, after reads (for `PONG`) and on every 200 ms timer tick.
- rirc's blocking event loop is replaced by MRE callbacks (`vm_tcp_connect` callback,
  `vm_create_timer`, keyboard/system event callbacks).
- All buffers are static and bounded (see `include/rirc_config.h`).
- Registration: `NICK`, `USER`; `001` marks the session online and triggers the optional `JOIN`.
- Only port 6667 (`RIRC_IRC_PORT`) is ever used; there is no TLS.
