/* rirc.vxp MRE entry point, MIT licensed (see LICENSE). */
#include "vmgraph.h"
#include "vmio.h"
#include "vmsys.h"
#include "vmtimer.h"

#include "app.h"
#include "ui.h"

static VMINT layer_hdls[1] = { -1 };
static VMINT timer_id = -1;

static void handle_sysevt(VMINT message, VMINT param);
static void handle_keyevt(VMINT event, VMINT keycode);

static void
timer_cb(VMINT tid)
{
	(void)tid;
	app_tick();
	ui_tick();
}

static void
create_surface(void)
{
	int w = vm_graphic_get_screen_width();
	int h = vm_graphic_get_screen_height();

	if (layer_hdls[0] < 0) {
		layer_hdls[0] = vm_graphic_create_layer(0, 0, w, h, -1);
		vm_graphic_set_clip(0, 0, w, h);
	}
	ui_set_surface(layer_hdls[0], vm_graphic_get_layer_buffer(layer_hdls[0]));
}

static void
destroy_surface(void)
{
	ui_set_surface(-1, 0);
	if (layer_hdls[0] >= 0) {
		vm_graphic_delete_layer(layer_hdls[0]);
		layer_hdls[0] = -1;
	}
}

void
vm_main(void)
{
	ui_init(vm_graphic_get_screen_width(), vm_graphic_get_screen_height());
	app_init();

	vm_reg_sysevt_callback(handle_sysevt);
	vm_reg_keyboard_callback(handle_keyevt);
}

static void
handle_sysevt(VMINT message, VMINT param)
{
	(void)param;

	switch (message) {
	case VM_MSG_CREATE:
	case VM_MSG_ACTIVE:
		create_surface();
		if (timer_id < 0)
			timer_id = vm_create_timer(APP_TICK_MS, timer_cb);
		ui_draw();
		break;
	case VM_MSG_PAINT:
		ui_draw();
		break;
	case VM_MSG_INACTIVE:
		if (timer_id >= 0) {
			vm_delete_timer(timer_id);
			timer_id = -1;
		}
		destroy_surface();
		break;
	case VM_MSG_QUIT:
		if (timer_id >= 0) {
			vm_delete_timer(timer_id);
			timer_id = -1;
		}
		app_shutdown();
		destroy_surface();
		break;
	}
}

static void
handle_keyevt(VMINT event, VMINT keycode)
{
	ui_key(event, keycode);
}
