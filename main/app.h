/* Application glue: connects the IRC state machine to the MRE TCP adapter. */
#ifndef APP_H
#define APP_H

#include "irc_client.h"

#define APP_TICK_MS 200

void app_init(void);
void app_shutdown(void);

irc_client_t *app_client(void);

void app_connect(void);
void app_disconnect(void);              /* user requested */
void app_send_line(const char *text);   /* chat text or /command */

/* Called from the periodic timer (every APP_TICK_MS). */
void app_tick(void);

#endif
