/* Keypad oriented UI: chat view, menu, and a multi-tap text editor. */
#ifndef UI_H
#define UI_H

#include "vmsys.h"

void ui_init(int screen_w, int screen_h);

/* Called when the drawing layer is (re)created or destroyed (buf == NULL). */
void ui_set_surface(VMINT layer_handle, VMUINT8 *buf);

void ui_key(VMINT event, VMINT keycode);
void ui_tick(void);  /* periodic: multi-tap timeout, redraw on change */
void ui_draw(void);  /* force a full redraw */

#endif
