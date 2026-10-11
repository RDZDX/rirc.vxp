/*
 * rirc.vxp UI, MIT licensed (see LICENSE).
 *
 * Screens: chat view, menu, editor. Text is drawn with the phone's built-in
 * MRE font; there is no terminal, curses or ANSI rendering.
 */
#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "vmgraph.h"
#include "vmio.h"
#include "vmsys.h"

#include "app.h"
#include "irc_log.h"
#include "rirc_config.h"

#define MAX_SEGMENTS 48
#define TAP_TICKS    4 /* multi-tap window: 4 * APP_TICK_MS (0.8-1.0 s) */

#define COL_BG      VM_COLOR_888_TO_565(0, 0, 0)
#define COL_BAR     VM_COLOR_888_TO_565(0, 64, 128)
#define COL_BAR_TXT VM_COLOR_888_TO_565(255, 255, 255)
#define COL_TEXT    VM_COLOR_888_TO_565(230, 230, 230)
#define COL_INFO    VM_COLOR_888_TO_565(150, 150, 150)
#define COL_ERROR   VM_COLOR_888_TO_565(255, 80, 80)
#define COL_SELF    VM_COLOR_888_TO_565(120, 200, 255)
#define COL_EVENT   VM_COLOR_888_TO_565(110, 200, 110)
#define COL_SEL     VM_COLOR_888_TO_565(0, 96, 0)

typedef enum { SCR_CHAT, SCR_MENU, SCR_EDIT } screen_t;
typedef enum { ED_MSG, ED_SERVER, ED_NICK, ED_CHAN } edit_kind_t;
typedef enum { MODE_abc, MODE_Abc, MODE_ABC, MODE_123, MODE_COUNT } edit_mode_t;

enum {
	MI_CONNECT, MI_WRITE, MI_SERVER, MI_NICK, MI_CHANNEL, MI_PORT,
	MI_CLEAR, MI_HELP, MI_EXIT, MI_COUNT
};

static int scr_w, scr_h;
static int line_h, bar_h;
static unsigned char char_w[128];

static VMINT layer_hdl = -1;
static VMUINT8 *surf;

static screen_t screen = SCR_CHAT;
static int dirty = 1;
static int scroll_rows;
static unsigned last_serial;
static int last_state = -1;

static int menu_sel;

static edit_kind_t edit_kind;
static char edit_buf[IRC_INPUT_MAX + 1];
static int edit_len;
static edit_mode_t edit_mode;
static int pend_key = -1, pend_idx, tap_ticks;
static const char *edit_err;

static const char *const key_chars[10] = {
	" 0", ".,'?!-1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"
};
static const char *const star_chars = "#/@:_*+=()";
static const char *const mode_names[MODE_COUNT] = { "abc", "Abc", "ABC", "123" };

/* ---- drawing helpers ------------------------------------------------- */

static int
cw(char c)
{
	return char_w[((unsigned char)c) & 0x7f];
}

static int
text_width(const char *s, int len)
{
	int w = 0, i;

	for (i = 0; i < len; i++)
		w += cw(s[i]);
	return w;
}

static void
fill(int x, int y, int w, int h, unsigned short color)
{
	vm_graphic_fill_rect(surf, x, y, w, h, color, color);
}

static void
draw_text(int x, int y, const char *s, int len, unsigned short color)
{
	VMWCHAR w[IRC_LOG_LINE_MAX + 2];
	int i;

	if (len > IRC_LOG_LINE_MAX)
		len = IRC_LOG_LINE_MAX;
	if (len <= 0)
		return;

	for (i = 0; i < len; i++)
		w[i] = (VMWCHAR)(unsigned char)s[i];
	w[len] = 0;

	vm_graphic_textout(surf, x, y, w, len, color);
}

/* Split t into rows no wider than maxw. starts[n] is the end sentinel. */
static int
wrap_segments(const char *t, int maxw, unsigned short *starts)
{
	int len = (int)strlen(t);
	int pos = 0, n = 0;

	while (pos < len && n < MAX_SEGMENTS) {
		int i = pos, w = 0, last_space = -1;

		starts[n++] = (unsigned short)pos;
		while (i < len) {
			int c = cw(t[i]);
			if (w + c > maxw && i > pos)
				break;
			w += c;
			if (t[i] == ' ')
				last_space = i;
			i++;
		}
		if (i < len && last_space > pos)
			i = last_space + 1;
		pos = i;
	}

	if (n == 0)
		starts[n++] = 0;
	starts[n] = (unsigned short)pos;
	return n;
}

static unsigned short
kind_color(int kind)
{
	switch (kind) {
	case IRC_LOG_ERROR: return COL_ERROR;
	case IRC_LOG_SELF:  return COL_SELF;
	case IRC_LOG_EVENT: return COL_EVENT;
	case IRC_LOG_INFO:  return COL_INFO;
	default:            return COL_TEXT;
	}
}

static void
draw_bar(int y, const char *left, const char *right)
{
	fill(0, y, scr_w, bar_h, COL_BAR);
	if (left)
		draw_text(3, y + (bar_h - line_h) / 2, left, (int)strlen(left), COL_BAR_TXT);
	if (right) {
		int len = (int)strlen(right);
		draw_text(scr_w - 3 - text_width(right, len), y + (bar_h - line_h) / 2, right, len, COL_BAR_TXT);
	}
}

/* ---- chat view -------------------------------------------------------- */

static int
chat_visible_rows(void)
{
	return (scr_h - 2 * bar_h) / line_h;
}

static int
chat_total_rows(void)
{
	irc_client_t *c = app_client();
	unsigned short starts[MAX_SEGMENTS + 1];
	unsigned i;
	int total = 0;

	for (i = 0; i < c->log.count; i++)
		total += wrap_segments(irc_log_get(&c->log, i)->text, scr_w - 6, starts);
	return total;
}

static void
clamp_scroll(void)
{
	int max = chat_total_rows() - chat_visible_rows();

	if (max < 0)
		max = 0;
	if (scroll_rows > max)
		scroll_rows = max;
	if (scroll_rows < 0)
		scroll_rows = 0;
}

static void
draw_chat(void)
{
	irc_client_t *c = app_client();
	char head[96];
	int rows = chat_visible_rows(), drawn = 0, skip, y;
	int idx;
	const char *left;

	snprintf(head, sizeof(head), "%s %s %s", irc_state_name(c->state), c->nick[0] ? c->nick : c->cfg.nick, c->target);
	fill(0, 0, scr_w, scr_h, COL_BG);
	draw_bar(0, head, scroll_rows ? "^" : 0);

	clamp_scroll();
	skip = scroll_rows;
	y = scr_h - bar_h - line_h;

	for (idx = (int)c->log.count - 1; idx >= 0 && drawn < rows; idx--) {
		const irc_log_line_t *l = irc_log_get(&c->log, (unsigned)idx);
		unsigned short starts[MAX_SEGMENTS + 1];
		int n = wrap_segments(l->text, scr_w - 6, starts), s;

		for (s = n - 1; s >= 0 && drawn < rows; s--) {
			if (skip > 0) {
				skip--;
				continue;
			}
			draw_text(3, y, l->text + starts[s], starts[s + 1] - starts[s], kind_color(l->kind));
			y -= line_h;
			drawn++;
		}
	}

	left = c->state == IRC_ST_DISCONNECTED ? "Connect" : c->state == IRC_ST_CONNECTING ? "Stop" : "Write";
	draw_bar(scr_h - bar_h, left, "Menu");
}

/* ---- menu --------------------------------------------------------------- */

static void
menu_label(int i, char *out, size_t outsz)
{
	irc_client_t *c = app_client();

	switch (i) {
	case MI_CONNECT:
		snprintf(out, outsz, "%s", c->state == IRC_ST_DISCONNECTED ? "Connect" : "Disconnect");
		break;
	case MI_WRITE:   snprintf(out, outsz, "Write message"); break;
	case MI_SERVER:  snprintf(out, outsz, "Server: %s", c->cfg.server); break;
	case MI_NICK:    snprintf(out, outsz, "Nick: %s", c->cfg.nick); break;
	case MI_CHANNEL: snprintf(out, outsz, "Channel: %s", c->cfg.channel[0] ? c->cfg.channel : "(none)"); break;
	case MI_PORT:    snprintf(out, outsz, "Port: %d (fixed)", RIRC_IRC_PORT); break;
	case MI_CLEAR:   snprintf(out, outsz, "Clear log"); break;
	case MI_HELP:    snprintf(out, outsz, "Commands help"); break;
	default:         snprintf(out, outsz, "Exit"); break;
	}
}

static void
draw_menu(void)
{
	int rows = (scr_h - 2 * bar_h) / line_h;
	int first = 0, i;
	char label[IRC_SERVER_MAX + 24];

	if (menu_sel >= rows)
		first = menu_sel - rows + 1;

	fill(0, 0, scr_w, scr_h, COL_BG);
	draw_bar(0, "rirc.vxp - Menu", 0);

	for (i = first; i < MI_COUNT && i - first < rows; i++) {
		int y = bar_h + (i - first) * line_h;

		if (i == menu_sel)
			fill(0, y, scr_w, line_h, COL_SEL);
		menu_label(i, label, sizeof(label));
		draw_text(3, y, label, (int)strlen(label), i == MI_PORT ? COL_INFO : COL_TEXT);
	}

	draw_bar(scr_h - bar_h, "Select", "Back");
}

/* ---- editor ------------------------------------------------------------- */

static void
edit_open(edit_kind_t kind, const char *initial)
{
	edit_kind = kind;
	edit_len = 0;
	if (initial) {
		strncpy(edit_buf, initial, IRC_INPUT_MAX);
		edit_buf[IRC_INPUT_MAX] = '\0';
		edit_len = (int)strlen(edit_buf);
	}
	edit_buf[edit_len] = '\0';
	edit_mode = MODE_abc;
	pend_key = -1;
	edit_err = 0;
	screen = SCR_EDIT;
}

static void
edit_append(char ch)
{
	if (edit_len >= IRC_INPUT_MAX)
		return;
	edit_buf[edit_len++] = ch;
	edit_buf[edit_len] = '\0';
}

static char
apply_case(char ch, int upper)
{
	if (upper && ch >= 'a' && ch <= 'z')
		return (char)(ch - 'a' + 'A');
	return ch;
}

static void
edit_multitap(const char *set, int key_id, int digit)
{
	int n = (int)strlen(set);
	int upper;

	if (edit_mode == MODE_123 && digit >= 0) {
		edit_append((char)('0' + digit));
		pend_key = -1;
		return;
	}

	if (pend_key == key_id && edit_len > 0) {
		upper = edit_buf[edit_len - 1] >= 'A' && edit_buf[edit_len - 1] <= 'Z';
		pend_idx = (pend_idx + 1) % n;
		edit_buf[edit_len - 1] = apply_case(set[pend_idx], upper);
	} else {
		if (edit_len >= IRC_INPUT_MAX)
			return;
		upper = edit_mode == MODE_ABC ||
		        (edit_mode == MODE_Abc && (edit_len == 0 || edit_buf[edit_len - 1] == ' '));
		pend_key = key_id;
		pend_idx = 0;
		edit_append(apply_case(set[0], upper));
	}
	tap_ticks = 0;
}

static void
edit_submit(void)
{
	irc_client_t *c = app_client();

	switch (edit_kind) {
	case ED_MSG:
		if (edit_len)
			app_send_line(edit_buf);
		scroll_rows = 0;
		screen = SCR_CHAT;
		return;
	case ED_SERVER:
		if (irc_client_set_server(c, edit_buf) != 0) {
			edit_err = "Invalid server name";
			return;
		}
		break;
	case ED_NICK:
		if (irc_client_set_nick(c, edit_buf) != 0) {
			edit_err = "Invalid nick (a-z 0-9 _-, max 16)";
			return;
		}
		break;
	case ED_CHAN:
		if (irc_client_set_channel(c, edit_buf) != 0) {
			edit_err = "Invalid channel name";
			return;
		}
		break;
	}
	screen = SCR_MENU;
}

static void
edit_key(VMINT event, VMINT key)
{
	if (event == VM_KEY_EVENT_REPEAT) {
		if (key == VM_KEY_RIGHT_SOFTKEY && edit_len > 0) {
			edit_buf[--edit_len] = '\0';
			pend_key = -1;
		}
		return;
	}
	if (event != VM_KEY_EVENT_DOWN)
		return;

	edit_err = 0;

	if (key >= VM_KEY_NUM0 && key <= VM_KEY_NUM9) {
		int d = key - VM_KEY_NUM0;
		edit_multitap(key_chars[d], d, d);
		return;
	}

	switch (key) {
	case VM_KEY_STAR:
		edit_multitap(star_chars, 10, -1);
		break;
	case VM_KEY_POUND:
		edit_mode = (edit_mode_t)((edit_mode + 1) % MODE_COUNT);
		pend_key = -1;
		break;
	case VM_KEY_OK:
	case VM_KEY_LEFT_SOFTKEY:
		edit_submit();
		break;
	case VM_KEY_RIGHT_SOFTKEY:
		if (edit_len > 0) {
			edit_buf[--edit_len] = '\0';
			pend_key = -1;
		} else {
			screen = edit_kind == ED_MSG ? SCR_CHAT : SCR_MENU;
		}
		break;
	}
}

static void
draw_edit(void)
{
	static const char *const titles[] = { "Message", "Server (port 6667)", "Nickname", "Channel" };
	char shown[IRC_INPUT_MAX + 2];
	unsigned short starts[MAX_SEGMENTS + 1];
	int n, s, first, rows, y;

	fill(0, 0, scr_w, scr_h, COL_BG);
	draw_bar(0, titles[edit_kind], mode_names[edit_mode]);

	memcpy(shown, edit_buf, (size_t)edit_len);
	shown[edit_len] = '_';
	shown[edit_len + 1] = '\0';

	n = wrap_segments(shown, scr_w - 6, starts);
	rows = (scr_h - 2 * bar_h - line_h) / line_h;
	first = n > rows ? n - rows : 0;
	y = bar_h + 2;
	for (s = first; s < n; s++, y += line_h)
		draw_text(3, y, shown + starts[s], starts[s + 1] - starts[s], COL_TEXT);

	if (edit_err)
		draw_text(3, scr_h - bar_h - line_h, edit_err, (int)strlen(edit_err), COL_ERROR);

	draw_bar(scr_h - bar_h, "OK", edit_len ? "Del" : "Back");
}

/* ---- public ------------------------------------------------------------- */

void
ui_init(int screen_w, int screen_h)
{
	int c;

	scr_w = screen_w;
	scr_h = screen_h;
	line_h = vm_graphic_get_character_height() + 1;
	bar_h = line_h + 4;

	for (c = 0; c < 128; c++)
		char_w[c] = (unsigned char)(c >= 32 && c < 127 ? vm_graphic_get_character_width((VMWCHAR)c) : 0);

	screen = SCR_CHAT;
	dirty = 1;
}

void
ui_set_surface(VMINT layer_handle, VMUINT8 *buf)
{
	layer_hdl = layer_handle;
	surf = buf;
	dirty = 1;
}

void
ui_draw(void)
{
	VMINT handles[1];

	if (!surf)
		return;

	switch (screen) {
	case SCR_MENU: draw_menu(); break;
	case SCR_EDIT: draw_edit(); break;
	default:       draw_chat(); break;
	}

	handles[0] = layer_hdl;
	vm_graphic_flush_layer(handles, 1);
	dirty = 0;
}

static void
menu_activate(void)
{
	irc_client_t *c = app_client();

	switch (menu_sel) {
	case MI_CONNECT:
		if (c->state == IRC_ST_DISCONNECTED)
			app_connect();
		else
			app_disconnect();
		screen = SCR_CHAT;
		scroll_rows = 0;
		break;
	case MI_WRITE:  edit_open(ED_MSG, 0); break;
	case MI_SERVER: edit_open(ED_SERVER, c->cfg.server); break;
	case MI_NICK:   edit_open(ED_NICK, c->cfg.nick); break;
	case MI_CHANNEL: edit_open(ED_CHAN, c->cfg.channel); break;
	case MI_CLEAR:
		irc_log_clear(&c->log);
		scroll_rows = 0;
		screen = SCR_CHAT;
		break;
	case MI_HELP:
		app_send_line("/help");
		scroll_rows = 0;
		screen = SCR_CHAT;
		break;
	case MI_EXIT:
		app_shutdown();
		vm_exit_app();
		break;
	default:
		break;
	}
}

static void
menu_key(VMINT key)
{
	switch (key) {
	case VM_KEY_UP:
		menu_sel = (menu_sel + MI_COUNT - 1) % MI_COUNT;
		break;
	case VM_KEY_DOWN:
		menu_sel = (menu_sel + 1) % MI_COUNT;
		break;
	case VM_KEY_OK:
	case VM_KEY_LEFT_SOFTKEY:
		menu_activate();
		break;
	case VM_KEY_RIGHT_SOFTKEY:
		screen = SCR_CHAT;
		break;
	}
}

static void
chat_key(VMINT key)
{
	irc_client_t *c = app_client();

	switch (key) {
	case VM_KEY_UP:
		scroll_rows++;
		break;
	case VM_KEY_DOWN:
		scroll_rows--;
		break;
	case VM_KEY_LEFT:
		scroll_rows += chat_visible_rows() - 1;
		break;
	case VM_KEY_RIGHT:
		scroll_rows -= chat_visible_rows() - 1;
		break;
	case VM_KEY_OK:
	case VM_KEY_LEFT_SOFTKEY:
		if (c->state == IRC_ST_DISCONNECTED)
			app_connect();
		else if (c->state == IRC_ST_CONNECTING)
			app_disconnect();
		else
			edit_open(ED_MSG, 0);
		scroll_rows = 0;
		break;
	case VM_KEY_RIGHT_SOFTKEY:
		menu_sel = 0;
		screen = SCR_MENU;
		break;
	default:
		/* typing a digit starts a message right away */
		if (key >= VM_KEY_NUM0 && key <= VM_KEY_NUM9 && c->state == IRC_ST_ONLINE) {
			edit_open(ED_MSG, 0);
			edit_key(VM_KEY_EVENT_DOWN, key);
		}
		break;
	}
	clamp_scroll();
}

void
ui_key(VMINT event, VMINT keycode)
{
	if (screen == SCR_EDIT) {
		edit_key(event, keycode);
	} else if (event == VM_KEY_EVENT_DOWN || event == VM_KEY_EVENT_REPEAT) {
		if (screen == SCR_MENU)
			menu_key(keycode);
		else
			chat_key(keycode);
	}
	ui_draw();
}

void
ui_tick(void)
{
	irc_client_t *c = app_client();

	if (pend_key >= 0 && ++tap_ticks > TAP_TICKS)
		pend_key = -1;

	if (c->log.serial != last_serial || (int)c->state != last_state) {
		last_serial = c->log.serial;
		last_state = (int)c->state;
		dirty = 1;
	}

	if (dirty)
		ui_draw();
}
