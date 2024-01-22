/*
 * See LICENSE file for copyright and license details.
 */
#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#endif

#include "util.h"

/* macros */
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define ROUND(X) ((int)((X < 0) ? (X - 0.5) : (X + 0.5)))
#define CLEANMASK(mask) (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M) \
	((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define END(A) ((A) + LENGTH(A))
#define TAGMASK ((1u << TAGCOUNT) - 1)
#define LISTEN(E, L, H) wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)                                       \
	do {                                                      \
		static struct wl_listener _l = { .notify = (H) }; \
		wl_signal_add((E), &_l);                          \
	} while (0)

/* enums */
enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */
enum { XDGShell, LayerShell, X11 }; /* client types */
enum {
	LyrBg,
	LyrBottom,
	LyrTile,
	LyrFloat,
	LyrFS,
	LyrTop,
	LyrOverlay,
	LyrBlock,
	NUM_LAYERS
}; /* scene layers */
#ifdef XWAYLAND
enum {
	NetWMWindowTypeDialog,
	NetWMWindowTypeSplash,
	NetWMWindowTypeToolbar,
	NetWMWindowTypeUtility,
	NetLast
}; /* EWMH atoms */
#endif

typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct Pertag Pertag;
typedef struct Monitor Monitor;
typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* XDGShell or X11* */
	struct wlr_box geom; /* layout-relative, includes border */
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_rect *border[4]; /* top, bottom, left, right */
	struct wlr_scene_tree *scene_surface;
	struct wl_list link;
	struct wl_list flink;
	union {
		struct wlr_xdg_surface *xdg;
		struct wlr_xwayland_surface *xwayland;
	} surface;
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener maximize;
	struct wl_listener unmap;
	struct wl_listener destroy;
	struct wl_listener set_title;
	struct wl_listener fullscreen;
	struct wl_listener set_decoration_mode;
	struct wl_listener destroy_decoration;
	struct wlr_box prev; /* layout-relative, includes border */
	struct wlr_box bounds;
#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
	unsigned int bw;
	uint32_t tags;
	int iscentered, isfloating, isurgent, isfullscreen;
	uint32_t resize; /* configure serial of a pending resize */
} Client;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wl_list link;
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods; /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
} KeyboardGroup;

typedef struct {
	/* Must keep these three elements in this order */
	unsigned int type; /* LayerShell */
	struct wlr_box geom;
	Monitor *mon;
	struct wlr_scene_tree *scene;
	struct wlr_scene_tree *popups;
	struct wlr_scene_layer_surface_v1 *scene_layer;
	struct wl_list link;
	int mapped;
	struct wlr_layer_surface_v1 *layer_surface;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
} LayerSurface;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

struct Monitor {
	struct wl_list link;
	struct wlr_output *wlr_output;
	struct wlr_scene_output *scene_output;
	struct wlr_scene_rect *fullscreen_bg; /* See createmon() for info */
	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener request_state;
	struct wl_listener destroy_lock_surface;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_box m; /* monitor area, layout-relative */
	struct wlr_box w; /* window area, layout-relative */
	struct wl_list layers[4]; /* LayerSurface.link */
	const Layout *lt[2];
	Pertag *pertag;
	unsigned int seltags;
	unsigned int sellt;
	uint32_t tagset[2];
	float mfact;
	int gamma_lut_changed;
	int nmaster;
	char ltsymbol[16];
};

typedef struct {
	const char *name;
	float mfact;
	int nmaster;
	float scale;
	const Layout *lt;
	enum wl_output_transform rr;
	int x, y;
} MonitorRule;

typedef struct {
	const char *id;
	const char *title;
	uint32_t tags;
	int isfloating;
	int monitor;
} Rule;

typedef struct {
	struct wlr_scene_tree *scene;

	struct wlr_session_lock_v1 *lock;
	struct wl_listener new_surface;
	struct wl_listener unlock;
	struct wl_listener destroy;
} SessionLock;

/* function declarations */
static void applybounds(Client *c, struct wlr_box *bbox);
static void applyrules(Client *c);
static void arrange(Monitor *m);
static void arrangelayer(Monitor *m, struct wl_list *list,
			 struct wlr_box *usable_area, int exclusive);
static void arrangelayers(Monitor *m);
static void axisnotify(struct wl_listener *listener, void *data);
static void buttonpress(struct wl_listener *listener, void *data);
static void chvt(const Arg *arg);
static void checkidleinhibitor(struct wlr_surface *exclude);
static void cleanup(void);
static void cleanupmon(struct wl_listener *listener, void *data);
static void closemon(Monitor *m);
static void column(Monitor *m);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void commitnotify(struct wl_listener *listener, void *data);
static void createdecoration(struct wl_listener *listener, void *data);
static void createidleinhibitor(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_keyboard *keyboard);
static void createlayersurface(struct wl_listener *listener, void *data);
static void createlocksurface(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createnotify(struct wl_listener *listener, void *data);
static void createpointer(struct wlr_pointer *pointer);
static void cursorframe(struct wl_listener *listener, void *data);
static void destroydecoration(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void destroyidleinhibitor(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylock(SessionLock *lock, int unlocked);
static void destroylocksurface(struct wl_listener *listener, void *data);
static void destroynotify(struct wl_listener *listener, void *data);
static void destroysessionlock(struct wl_listener *listener, void *data);
static void destroysessionmgr(struct wl_listener *listener, void *data);
static Monitor *dirtomon(enum wlr_direction dir);
static void focusclient(Client *c, int lift);
static void focusmon(const Arg *arg);
static void monaxis(const Arg *arg);
static void focusstack(const Arg *arg);
static Client *focustop(Monitor *m);
static void fullscreennotify(struct wl_listener *listener, void *data);
static void handlesig(int signo);
static void incnmaster(const Arg *arg);
static void inputdevice(struct wl_listener *listener, void *data);
static int keybinding(uint32_t mods, xkb_keysym_t sym);
static void keypress(struct wl_listener *listener, void *data);
static void keypressmod(struct wl_listener *listener, void *data);
static int keyrepeat(void *data);
static void killclient(const Arg *arg);
static void locksession(struct wl_listener *listener, void *data);
static void maplayersurfacenotify(struct wl_listener *listener, void *data);
static void mapnotify(struct wl_listener *listener, void *data);
static void maximizenotify(struct wl_listener *listener, void *data);
static void monocle(Monitor *m);
static void motionabsolute(struct wl_listener *listener, void *data);
static void motionnotify(uint32_t time);
static void floatvmovebykey(const Arg *arg);
static void floathmovebykey(const Arg *arg);
static void motionrelative(struct wl_listener *listener, void *data);
static void moveresize(const Arg *arg);
static void outputmgrapply(struct wl_listener *listener, void *data);
static void outputmgrapplyortest(struct wlr_output_configuration_v1 *config,
				 int test);
static void outputmgrtest(struct wl_listener *listener, void *data);
static void pointerfocus(Client *c, struct wlr_surface *surface, double sx,
			 double sy, uint32_t time);
static void printstatus(void);
static void quit(const Arg *arg);
static void rendermon(struct wl_listener *listener, void *data);
static void requestdecorationmode(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void resize(Client *c, struct wlr_box geo, int interact,
		   int draw_borders);
static void requestmonstate(struct wl_listener *listener, void *data);
static void run(char *startup_cmd);
static void setcursor(struct wl_listener *listener, void *data);
static void setcursorshape(struct wl_listener *listener, void *data);
static void setfloating(Client *c, int floating);
static void setfullscreen(Client *c, int fullscreen);
static void setgamma(struct wl_listener *listener, void *data);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setmon(Client *c, Monitor *m, uint32_t newtags);
static void setpsel(struct wl_listener *listener, void *data);
static void setsel(struct wl_listener *listener, void *data);
static void setup(void);
static void spawn(const Arg *arg);
static void startdrag(struct wl_listener *listener, void *data);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *m);
static void togglefloating(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void unlocksession(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void unmapnotify(struct wl_listener *listener, void *data);
static void updatemons(struct wl_listener *listener, void *data);
static void updatetitle(struct wl_listener *listener, void *data);
static void urgent(struct wl_listener *listener, void *data);
static void view(const Arg *arg);
static void virtualkeyboard(struct wl_listener *listener, void *data);
static void warpcursor(const Client *c);
static Monitor *xytomon(double x, double y);
static void xytonode(double x, double y, struct wlr_surface **psurface,
		     Client **pc, LayerSurface **pl, double *nx, double *ny);
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static pid_t child_pid = -1;
static int locked;
static void *exclusive_focus;
static struct wl_display *dpy;
static struct wlr_backend *backend;
static struct wlr_scene *scene;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static struct wlr_scene_tree *drag_icon;
/* Map from ZWLR_LAYER_SHELL_* constants to Lyr* enum */
static const int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay };
static struct wlr_renderer *drw;
static struct wlr_allocator *alloc;
static struct wlr_compositor *compositor;
static struct wlr_session *session;

static struct wlr_xdg_shell *xdg_shell;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wl_list clients; /* tiling order */
static struct wl_list fstack; /* focus order */
static struct wlr_idle_notifier_v1 *idle_notifier;
static struct wlr_idle_inhibit_manager_v1 *idle_inhibit_mgr;
static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_gamma_control_manager_v1 *gamma_control_mgr;
static struct wlr_virtual_keyboard_manager_v1 *virtual_keyboard_mgr;
static struct wlr_cursor_shape_manager_v1 *cursor_shape_mgr;

static struct wlr_cursor *cursor;
static struct wlr_xcursor_manager *cursor_mgr;

static struct wlr_scene_rect *root_bg;
static struct wlr_session_lock_manager_v1 *session_lock_mgr;
static struct wlr_scene_rect *locked_bg;
static struct wlr_session_lock_v1 *cur_lock;
static struct wl_listener lock_listener = { .notify = locksession };

static struct wlr_seat *seat;
static KeyboardGroup kb_group = { 0 };
static KeyboardGroup vkb_group = { 0 };
static struct wlr_surface *held_grab;
static unsigned int cursor_mode;
static Client *grabc;
static int grabcx, grabcy; /* client-relative */

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;
static struct wl_list mons;
static Monitor *selmon;

#ifdef XWAYLAND
static void activatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void createnotifyx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static xcb_atom_t getatom(xcb_connection_t *xc, const char *name);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
static struct wlr_xwayland *xwayland;
static xcb_atom_t netatom[NetLast];
#endif

static char horiz_monaxis = 0;
/* configuration, allows nested code to access above variables */
#include "config.h"

/* attempt to encapsulate suck into one file */
#include "client.h"

struct Pertag {
	unsigned int curtag, prevtag; /* current and previous tag */
	int nmasters[TAGCOUNT + 1]; /* number of windows in master area */
	float mfacts[TAGCOUNT + 1]; /* mfacts per tag */
	unsigned int sellts[TAGCOUNT + 1]; /* selected layouts */
	const Layout *ltidxs[TAGCOUNT + 1]
			    [2]; /* matrix of tags and layouts indexes  */
};

/* function implementations */
void applybounds(Client *c, struct wlr_box *bbox)
{
	/* set minimum possible */
	c->geom.width = MAX(1, c->geom.width);
	c->geom.height = MAX(1, c->geom.height);

	if (c->geom.x >= bbox->x + bbox->width)
		c->geom.x = bbox->x + bbox->width - c->geom.width;
	if (c->geom.y >= bbox->y + bbox->height)
		c->geom.y = bbox->y + bbox->height - c->geom.height;
	if (c->geom.x + c->geom.width + 2 * (int)c->bw <= bbox->x)
		c->geom.x = bbox->x;
	if (c->geom.y + c->geom.height + 2 * (int)c->bw <= bbox->y)
		c->geom.y = bbox->y;
}

void applyrules(Client *c)
{
	/* rule matching */
	const char *appid, *title;
	uint32_t newtags = 0;
	int i;
	const Rule *r;
	Monitor *mon = selmon, *m;

	c->isfloating = client_is_float_type(c);
	if (!(appid = client_get_appid(c)))
		appid = broken;
	if (!(title = client_get_title(c)))
		title = broken;

	for (r = rules; r < END(rules); r++) {
		if ((!r->title || strstr(title, r->title)) &&
		    (!r->id || strstr(appid, r->id))) {
			c->isfloating = r->isfloating;
			newtags |= r->tags;
			i = 0;
			wl_list_for_each(m, &mons, link)
			{
				if (r->monitor == i++)
					mon = m;
			}
		}
	}

	if (c->isfloating == 2) {
		c->geom.x = (mon->w.width - c->geom.width) / 2 + mon->m.x;
		c->geom.y = mon->w.y;
	} else if (c->isfloating == 3) {
		c->geom.x = (mon->w.width - c->geom.width) + mon->m.x;
		c->geom.y = mon->w.y;
	}

	wlr_scene_node_reparent(&c->scene->node,
				layers[c->isfloating ? LyrFloat : LyrTile]);
	setmon(c, mon, newtags);
}

void arrange(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &clients, link)
	{
		if (c->mon == m) {
			wlr_scene_node_set_enabled(&c->scene->node,
						   VISIBLEON(c, m));
			client_set_suspended(c, !VISIBLEON(c, m));
		}
	}

	wlr_scene_node_set_enabled(&m->fullscreen_bg->node,
				   (c = focustop(m)) && c->isfullscreen);

	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));

	if (m->lt[m->sellt]->arrange)
		m->lt[m->sellt]->arrange(m);
	motionnotify(0);
	checkidleinhibitor(NULL);
}

void arrangelayer(Monitor *m, struct wl_list *list, struct wlr_box *usable_area,
		  int exclusive)
{
	LayerSurface *l;
	struct wlr_box full_area = m->m;

	wl_list_for_each(l, list, link)
	{
		struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;

		if (exclusive != (layer_surface->current.exclusive_zone > 0))
			continue;

		wlr_scene_layer_surface_v1_configure(l->scene_layer, &full_area,
						     usable_area);
		wlr_scene_node_set_position(&l->popups->node, l->scene->node.x,
					    l->scene->node.y);
		l->geom.x = l->scene->node.x;
		l->geom.y = l->scene->node.y;
	}
}

void arrangelayers(Monitor *m)
{
	int i;
	struct wlr_box usable_area = m->m;
	LayerSurface *l;
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	if (!m->wlr_output->enabled)
		return;

	/* Arrange exclusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 1);

	if (!wlr_box_equal(&usable_area, &m->w)) {
		m->w = usable_area;
		arrange(m);
	}

	/* Arrange non-exlusive surfaces from top->bottom */
	for (i = 3; i >= 0; i--)
		arrangelayer(m, &m->layers[i], &usable_area, 0);

	/* Find topmost keyboard interactive layer, if such a layer exists */
	for (i = 0; i < (int)LENGTH(layers_above_shell); i++) {
		wl_list_for_each_reverse(l, &m->layers[layers_above_shell[i]],
					 link)
		{
			if (locked ||
			    !l->layer_surface->current.keyboard_interactive ||
			    !l->mapped)
				continue;
			/* Deactivate the focused client. */
			focusclient(NULL, 0);
			exclusive_focus = l;
			client_notify_enter(l->layer_surface->surface,
					    wlr_seat_get_keyboard(seat));
			return;
		}
	}
}

void axisnotify(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an axis event,
	 * for example when you move the scroll wheel. */
	struct wlr_pointer_axis_event *event = data;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);
	/* TODO: allow usage of scroll whell for mousebindings, it can be implemented
	 * checking the event's orientation and the delta of the event */
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation,
				     event->delta, event->delta_discrete,
				     event->source);
}

void buttonpress(struct wl_listener *listener, void *data)
{
	struct wlr_pointer_button_event *event = data;
	struct wlr_keyboard *keyboard;
	uint32_t mods;
	Client *c;
	const Button *b;

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		cursor_mode = CurPressed;
		held_grab = seat->pointer_state.focused_surface;
		if (locked)
			break;

		/* Change focus if the button was _pressed_ over a client */
		xytonode(cursor->x, cursor->y, NULL, &c, NULL, NULL, NULL);
		if (c && (!client_is_unmanaged(c) || client_wants_focus(c)))
			focusclient(c, 1);

		keyboard = wlr_seat_get_keyboard(seat);
		mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
		for (b = buttons; b < END(buttons); b++) {
			if (CLEANMASK(mods) == CLEANMASK(b->mod) &&
			    event->button == b->button && b->func) {
				b->func(&b->arg);
				return;
			}
		}
		break;
	case WLR_BUTTON_RELEASED:
		held_grab = NULL;
		/* If you released any buttons, we exit interactive move/resize mode. */
		/* TODO should reset to the pointer focus's current setcursor */
		if (!locked && cursor_mode != CurNormal &&
		    cursor_mode != CurPressed) {
			wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");
			cursor_mode = CurNormal;
			/* Drop the window off on its new monitor */
			selmon = xytomon(cursor->x, cursor->y);
			setmon(grabc, selmon, 0);
			return;
		} else {
			cursor_mode = CurNormal;
		}
		break;
	}
	/* If the event wasn't handled by the compositor, notify the client with
	 * pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(seat, event->time_msec, event->button,
				       event->state);
}

void chvt(const Arg *arg)
{
	wlr_session_change_vt(session, arg->ui);
}

void checkidleinhibitor(struct wlr_surface *exclude)
{
	int inhibited = 0, unused_lx, unused_ly;
	struct wlr_idle_inhibitor_v1 *inhibitor;
	wl_list_for_each(inhibitor, &idle_inhibit_mgr->inhibitors, link)
	{
		struct wlr_surface *surface =
			wlr_surface_get_root_surface(inhibitor->surface);
		struct wlr_scene_tree *tree = surface->data;
		if (exclude != surface &&
		    (bypass_surface_visibility ||
		     (!tree || wlr_scene_node_coords(&tree->node, &unused_lx,
						     &unused_ly)))) {
			inhibited = 1;
			break;
		}
	}

	wlr_idle_notifier_v1_set_inhibited(idle_notifier, inhibited);
}

void cleanup(void)
{
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif
	wl_display_destroy_clients(dpy);
	if (child_pid > 0) {
		kill(child_pid, SIGTERM);
		waitpid(child_pid, NULL, 0);
	}
	wlr_xcursor_manager_destroy(cursor_mgr);
	wlr_output_layout_destroy(output_layout);

	/* Remove event source that use the dpy event loop before destroying dpy */
	wl_event_source_remove(kb_group.key_repeat_source);
	wl_event_source_remove(vkb_group.key_repeat_source);

	wl_display_destroy(dpy);
	/* Destroy after the wayland display (when the monitors are already destroyed)
	   to avoid destroying them with an invalid scene output. */
	wlr_scene_node_destroy(&scene->tree.node);
}

void cleanupmon(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy);
	LayerSurface *l, *tmp;
	size_t i;

	/* m->layers[i] are intentionally not unlinked */
	for (i = 0; i < LENGTH(m->layers); i++) {
		wl_list_for_each_safe(l, tmp, &m->layers[i], link)
			wlr_layer_surface_v1_destroy(l->layer_surface);
	}

	wl_list_remove(&m->destroy.link);
	wl_list_remove(&m->frame.link);
	wl_list_remove(&m->link);
	wl_list_remove(&m->request_state.link);
	m->wlr_output->data = NULL;
	wlr_output_layout_remove(output_layout, m->wlr_output);
	wlr_scene_output_destroy(m->scene_output);

	free(m->pertag);
	closemon(m);
	wlr_scene_node_destroy(&m->fullscreen_bg->node);
	free(m);
}

void closemon(Monitor *m)
{
	/* update selmon if needed and
	 * move closed monitor's clients to the focused one */
	Client *c;
	int i = 0, nmons = wl_list_length(&mons);
	if (!nmons) {
		selmon = NULL;
	} else if (m == selmon) {
		do /* don't switch to disabled mons */
			selmon = wl_container_of(mons.next, selmon, link);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	}

	wl_list_for_each(c, &clients, link)
	{
		if (c->isfloating && c->geom.x > m->m.width)
			resize(c,
			       (struct wlr_box){ .x = c->geom.x - m->w.width,
						 .y = c->geom.y,
						 .width = c->geom.width,
						 .height = c->geom.height },
			       0, 1);
		if (c->mon == m)
			setmon(c, selmon, c->tags);
	}
	focusclient(focustop(selmon), 1);
	printstatus();
}

void column(Monitor *m)
{
	Client *c;
	unsigned int n = 0, i = 0, draw_borders = 1;

	if (n == smartborders)
		draw_borders = 0;

	wl_list_for_each(c, &clients, link) if (VISIBLEON(c, m) &&
						!c->isfloating &&
						!c->isfullscreen) n++;

	wl_list_for_each(c, &clients, link)
	{
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		resize(c,
		       (struct wlr_box){ .x = m->w.x + i * m->w.width / n,
					 .y = m->w.y,
					 .width = m->w.width / n,
					 .height = m->w.height },
		       1, draw_borders);
		i++;
	}
}

void commitlayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, surface_commit);
	struct wlr_layer_surface_v1 *layer_surface = l->layer_surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->current.layer]];

	if (layer_surface->current.committed == 0 &&
	    l->mapped == layer_surface->surface->mapped)
		return;
	l->mapped = layer_surface->surface->mapped;

	if (scene_layer != l->scene->node.parent) {
		wlr_scene_node_reparent(&l->scene->node, scene_layer);
		wl_list_remove(&l->link);
		wl_list_insert(&l->mon->layers[layer_surface->current.layer],
			       &l->link);
		wlr_scene_node_reparent(
			&l->popups->node,
			(layer_surface->current.layer <
					 ZWLR_LAYER_SHELL_V1_LAYER_TOP ?
				 layers[LyrTop] :
				 scene_layer));
	}

	arrangelayers(l->mon);
}

void commitnotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, commit);

	if (client_surface(c)->mapped)
		resize(c, c->geom, (c->isfloating && !c->isfullscreen),
		       c->bw > 0);

	/* mark a pending resize as completed */
	if (c->resize && c->resize <= c->surface.xdg->current.configure_serial)
		c->resize = 0;
}

void createdecoration(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_toplevel_decoration_v1 *deco = data;
	Client *c = deco->toplevel->base->data;
	c->decoration = deco;

	LISTEN(&deco->events.request_mode, &c->set_decoration_mode,
	       requestdecorationmode);
	LISTEN(&deco->events.destroy, &c->destroy_decoration,
	       destroydecoration);

	requestdecorationmode(&c->set_decoration_mode, deco);
}

void createidleinhibitor(struct wl_listener *listener, void *data)
{
	struct wlr_idle_inhibitor_v1 *idle_inhibitor = data;
	LISTEN_STATIC(&idle_inhibitor->events.destroy, destroyidleinhibitor);

	checkidleinhibitor(NULL);
}

void createkeyboard(struct wlr_keyboard *keyboard)
{
	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(keyboard, kb_group.wlr_group->keyboard.keymap);
	wlr_keyboard_set_repeat_info(keyboard, repeat_rate, repeat_delay);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(kb_group.wlr_group, keyboard);
}

void createlayersurface(struct wl_listener *listener, void *data)
{
	struct wlr_layer_surface_v1 *layer_surface = data;
	LayerSurface *l;
	struct wlr_surface *surface = layer_surface->surface;
	struct wlr_scene_tree *scene_layer =
		layers[layermap[layer_surface->pending.layer]];
	struct wlr_layer_surface_v1_state old_state;

	if (!layer_surface->output &&
	    !(layer_surface->output = selmon ? selmon->wlr_output : NULL)) {
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	l = layer_surface->data = ecalloc(1, sizeof(*l));
	l->type = LayerShell;
	LISTEN(&surface->events.commit, &l->surface_commit,
	       commitlayersurfacenotify);
	LISTEN(&surface->events.map, &l->map, maplayersurfacenotify);
	LISTEN(&surface->events.unmap, &l->unmap, unmaplayersurfacenotify);
	LISTEN(&layer_surface->events.destroy, &l->destroy,
	       destroylayersurfacenotify);

	l->layer_surface = layer_surface;
	l->mon = layer_surface->output->data;
	l->scene_layer =
		wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
	l->scene = l->scene_layer->tree;
	l->popups = surface->data = wlr_scene_tree_create(
		layer_surface->current.layer < ZWLR_LAYER_SHELL_V1_LAYER_TOP ?
			layers[LyrTop] :
			scene_layer);
	l->scene->node.data = l->popups->node.data = l;

	wl_list_insert(&l->mon->layers[layer_surface->pending.layer], &l->link);
	wlr_surface_send_enter(surface, layer_surface->output);

	/* Temporarily set the layer's current state to pending
	 * so that we can easily arrange it
	 */
	old_state = layer_surface->current;
	layer_surface->current = layer_surface->pending;
	l->mapped = 1;
	arrangelayers(l->mon);
	layer_surface->current = old_state;
}

void createlocksurface(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	Monitor *m = lock_surface->output->data;
	struct wlr_scene_tree *scene_tree = lock_surface->surface->data =
		wlr_scene_subsurface_tree_create(lock->scene,
						 lock_surface->surface);
	m->lock_surface = lock_surface;

	wlr_scene_node_set_position(&scene_tree->node, m->m.x, m->m.y);
	wlr_session_lock_surface_v1_configure(lock_surface, m->m.width,
					      m->m.height);

	LISTEN(&lock_surface->events.destroy, &m->destroy_lock_surface,
	       destroylocksurface);

	if (m == selmon)
		client_notify_enter(lock_surface->surface,
				    wlr_seat_get_keyboard(seat));
}

void createmon(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new output (aka a display or
	 * monitor) becomes available. */
	struct wlr_output *wlr_output = data;
	const MonitorRule *r;
	size_t i;
	struct wlr_output_state state;
	Monitor *m;

	if (!wlr_output_init_render(wlr_output, alloc, drw))
		return;

	m = wlr_output->data = ecalloc(1, sizeof(*m));
	m->wlr_output = wlr_output;

	for (i = 0; i < LENGTH(m->layers); i++)
		wl_list_init(&m->layers[i]);

	wlr_output_state_init(&state);
	/* Initialize monitor state using configured rules */
	m->tagset[0] = m->tagset[1] = 1;
	for (r = monrules; r < END(monrules); r++) {
		if (!r->name || strstr(wlr_output->name, r->name)) {
			m->m.x = r->x;
			m->m.y = r->y;
			m->mfact = r->mfact;
			m->nmaster = r->nmaster;
			m->lt[0] = m->lt[1] = r->lt;
			strncpy(m->ltsymbol, m->lt[m->sellt]->symbol,
				LENGTH(m->ltsymbol));
			wlr_output_state_set_scale(&state, r->scale);
			wlr_output_state_set_transform(&state, r->rr);
			break;
		}
	}

	/* The mode is a tuple of (width, height, refresh rate), and each
	 * monitor supports only a specific set of modes. We just pick the
	 * monitor's preferred mode; a more sophisticated compositor would let
	 * the user configure it. */
	wlr_output_state_set_mode(&state,
				  wlr_output_preferred_mode(wlr_output));

	/* Set up event listeners */
	LISTEN(&wlr_output->events.frame, &m->frame, rendermon);
	LISTEN(&wlr_output->events.destroy, &m->destroy, cleanupmon);
	LISTEN(&wlr_output->events.request_state, &m->request_state,
	       requestmonstate);

	wlr_output_state_set_enabled(&state, 1);
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wl_list_insert(&mons, &m->link);
	printstatus();

	m->pertag = calloc(1, sizeof(Pertag));
	m->pertag->curtag = m->pertag->prevtag = 1;

	for (i = 0; i <= TAGCOUNT; i++) {
		m->pertag->nmasters[i] = m->nmaster;
		m->pertag->mfacts[i] = m->mfact;

		m->pertag->ltidxs[i][0] = m->lt[0];
		m->pertag->ltidxs[i][1] = m->lt[1];
		m->pertag->sellts[i] = m->sellt;
	}

	/* The xdg-protocol specifies:
	 *
	 * If the fullscreened surface is not opaque, the compositor must make
	 * sure that other screen content not part of the same surface tree (made
	 * up of subsurfaces, popups or similarly coupled surfaces) are not
	 * visible below the fullscreened surface.
	 *
	 */
	/* updatemons() will resize and set correct position */
	m->fullscreen_bg =
		wlr_scene_rect_create(layers[LyrFS], 0, 0, fullscreen_bg);
	wlr_scene_node_set_enabled(&m->fullscreen_bg->node, 0);

	/* Adds this to the output layout in the order it was configured in.
	 *
	 * The output layout utility automatically adds a wl_output global to the
	 * display, which Wayland clients can see to find out information about the
	 * output (such as DPI, scale factor, manufacturer, etc).
	 */
	m->scene_output = wlr_scene_output_create(scene, wlr_output);
	if (m->m.x < 0 || m->m.y < 0)
		wlr_output_layout_add_auto(output_layout, wlr_output);
	else
		wlr_output_layout_add(output_layout, wlr_output, m->m.x,
				      m->m.y);
}

void createnotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when wlr_xdg_shell receives a new xdg surface from a
	 * client, either a toplevel (application window) or popup,
	 * or when wlr_layer_shell receives a new popup from a layer.
	 * If you want to do something tricky with popups you should check if
	 * its parent is wlr_xdg_shell or wlr_layer_shell */
	struct wlr_xdg_surface *xdg_surface = data;
	Client *c = NULL;
	LayerSurface *l = NULL;

	if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
		struct wlr_xdg_popup *popup = xdg_surface->popup;
		struct wlr_box box;
		if (toplevel_from_wlr_surface(popup->base->surface, &c, &l) < 0)
			return;
		popup->base->surface->data = wlr_scene_xdg_surface_create(
			popup->parent->data, popup->base);
		if ((l && !l->mon) || (c && !c->mon))
			return;
		box = l ? l->mon->m : c->mon->w;
		box.x -= (l ? l->geom.x : c->geom.x);
		box.y -= (l ? l->geom.y : c->geom.y);
		wlr_xdg_popup_unconstrain_from_box(popup, &box);
		return;
	} else if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_NONE)
		return;

	/* Allocate a Client for this surface */
	c = xdg_surface->data = ecalloc(1, sizeof(*c));
	c->surface.xdg = xdg_surface;
	c->bw = borderpx;

	wlr_xdg_toplevel_set_wm_capabilities(
		xdg_surface->toplevel,
		WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

	LISTEN(&xdg_surface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xdg_surface->surface->events.commit, &c->commit, commitnotify);
	LISTEN(&xdg_surface->surface->events.map, &c->map, mapnotify);
	LISTEN(&xdg_surface->surface->events.unmap, &c->unmap, unmapnotify);
	LISTEN(&xdg_surface->toplevel->events.request_fullscreen,
	       &c->fullscreen, fullscreennotify);
	LISTEN(&xdg_surface->toplevel->events.request_maximize, &c->maximize,
	       maximizenotify);
	LISTEN(&xdg_surface->toplevel->events.set_title, &c->set_title,
	       updatetitle);
}

void createpointer(struct wlr_pointer *pointer)
{
	struct libinput_device *device;
	if (wlr_input_device_is_libinput(&pointer->base) &&
	    (device = wlr_libinput_get_device_handle(&pointer->base))) {
		if (libinput_device_config_tap_get_finger_count(device)) {
			libinput_device_config_tap_set_enabled(device,
							       tap_to_click);
			libinput_device_config_tap_set_drag_enabled(
				device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(
				device, drag_lock);
			libinput_device_config_tap_set_button_map(device,
								  button_map);
		}

		if (libinput_device_config_scroll_has_natural_scroll(device))
			libinput_device_config_scroll_set_natural_scroll_enabled(
				device, natural_scrolling);

		if (libinput_device_config_dwt_is_available(device))
			libinput_device_config_dwt_set_enabled(
				device, disable_while_typing);

		if (libinput_device_config_left_handed_is_available(device))
			libinput_device_config_left_handed_set(device,
							       left_handed);

		if (libinput_device_config_middle_emulation_is_available(
			    device))
			libinput_device_config_middle_emulation_set_enabled(
				device, middle_button_emulation);

		if (libinput_device_config_scroll_get_methods(device) !=
		    LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method(device,
								 scroll_method);

		if (libinput_device_config_click_get_methods(device) !=
		    LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method(device,
								click_method);

		if (libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(
				device, send_events_mode);

		if (libinput_device_config_accel_is_available(device)) {
			libinput_device_config_accel_set_profile(device,
								 accel_profile);
			libinput_device_config_accel_set_speed(device,
							       accel_speed);
		}
	}

	wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void cursorframe(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an frame
	 * event. Frame events are sent after regular pointer events to group
	 * multiple events together. For instance, two axis events may happen at the
	 * same time, in which case a frame event won't be sent in between. */
	/* Notify the client with pointer focus of the frame event. */
	wlr_seat_pointer_notify_frame(seat);
}

void destroydecoration(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, destroy_decoration);

	wl_list_remove(&c->destroy_decoration.link);
	wl_list_remove(&c->set_decoration_mode.link);
}

void destroydragicon(struct wl_listener *listener, void *data)
{
	/* Focus enter isn't sent during drag, so refocus the focused node. */
	focusclient(focustop(selmon), 1);
	motionnotify(0);
}

void destroyidleinhibitor(struct wl_listener *listener, void *data)
{
	/* `data` is the wlr_surface of the idle inhibitor being destroyed,
	 * at this point the idle inhibitor is still in the list of the manager */
	checkidleinhibitor(wlr_surface_get_root_surface(data));
}

void destroylayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, destroy);

	wl_list_remove(&l->link);
	wl_list_remove(&l->destroy.link);
	wl_list_remove(&l->map.link);
	wl_list_remove(&l->unmap.link);
	wl_list_remove(&l->surface_commit.link);
	wlr_scene_node_destroy(&l->scene->node);
	wlr_scene_node_destroy(&l->popups->node);
	free(l);
}

void destroylock(SessionLock *lock, int unlock)
{
	wlr_seat_keyboard_notify_clear_focus(seat);
	if ((locked = !unlock))
		goto destroy;

	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	focusclient(focustop(selmon), 0);
	motionnotify(0);

destroy:
	wl_list_remove(&lock->new_surface.link);
	wl_list_remove(&lock->unlock.link);
	wl_list_remove(&lock->destroy.link);

	wlr_scene_node_destroy(&lock->scene->node);
	cur_lock = NULL;
	free(lock);
}

void destroylocksurface(struct wl_listener *listener, void *data)
{
	Monitor *m = wl_container_of(listener, m, destroy_lock_surface);
	struct wlr_session_lock_surface_v1 *surface,
		*lock_surface = m->lock_surface;

	m->lock_surface = NULL;
	wl_list_remove(&m->destroy_lock_surface.link);

	if (lock_surface->surface != seat->keyboard_state.focused_surface)
		return;

	if (locked && cur_lock && !wl_list_empty(&cur_lock->surfaces)) {
		surface =
			wl_container_of(cur_lock->surfaces.next, surface, link);
		client_notify_enter(surface->surface,
				    wlr_seat_get_keyboard(seat));
	} else if (!locked) {
		focusclient(focustop(selmon), 1);
	} else {
		wlr_seat_keyboard_clear_focus(seat);
	}
}

void destroynotify(struct wl_listener *listener, void *data)
{
	/* Called when the xdg_toplevel is destroyed. */
	Client *c = wl_container_of(listener, c, destroy);
	wl_list_remove(&c->destroy.link);
	wl_list_remove(&c->set_title.link);
	wl_list_remove(&c->fullscreen.link);
#ifdef XWAYLAND
	if (c->type != XDGShell) {
		wl_list_remove(&c->activate.link);
		wl_list_remove(&c->associate.link);
		wl_list_remove(&c->configure.link);
		wl_list_remove(&c->dissociate.link);
		wl_list_remove(&c->set_hints.link);
	} else
#endif
	{
		wl_list_remove(&c->commit.link);
		wl_list_remove(&c->map.link);
		wl_list_remove(&c->unmap.link);
	}
	free(c);
}

void destroysessionlock(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, destroy);
	destroylock(lock, 0);
}

void destroysessionmgr(struct wl_listener *listener, void *data)
{
	wl_list_remove(&lock_listener.link);
	wl_list_remove(&listener->link);
}

Monitor *dirtomon(enum wlr_direction dir)
{
	struct wlr_output *next;
	if (!wlr_output_layout_get(output_layout, selmon->wlr_output))
		return selmon;
	if ((next = wlr_output_layout_adjacent_output(
		     output_layout, dir, selmon->wlr_output, selmon->m.x,
		     selmon->m.y)))
		return next->data;
	if ((next = wlr_output_layout_farthest_output(
		     output_layout,
		     dir ^ (WLR_DIRECTION_LEFT | WLR_DIRECTION_RIGHT),
		     selmon->wlr_output, selmon->m.x, selmon->m.y)))
		return next->data;
	return selmon;
}

void focusclient(Client *c, int lift)
{
	struct wlr_surface *old = seat->keyboard_state.focused_surface;
	int unused_lx, unused_ly, old_client_type;
	Client *old_c = NULL;
	LayerSurface *old_l = NULL;

	if (locked)
		return;

	/* Warp cursor to center of client if it is outside */
	if (cursor_warp && c)
		warpcursor(c);

	/* Raise client in stacking order if requested */
	if (c && lift)
		wlr_scene_node_raise_to_top(&c->scene->node);

	if (c && client_surface(c) == old)
		return;

	if ((old_client_type = toplevel_from_wlr_surface(old, &old_c,
							 &old_l)) == XDGShell) {
		struct wlr_xdg_popup *popup, *tmp;
		wl_list_for_each_safe(popup, tmp, &old_c->surface.xdg->popups,
				      link) wlr_xdg_popup_destroy(popup);
	}

	/* Put the new client atop the focus stack and select its monitor */
	if (c && !client_is_unmanaged(c)) {
		wl_list_remove(&c->flink);
		wl_list_insert(&fstack, &c->flink);
		selmon = c->mon;
		c->isurgent = 0;
		client_restack_surface(c);

		/* Don't change border color if there is an exclusive focus or we are
		 * handling a drag operation */
		if (!exclusive_focus && !seat->drag)
			client_set_border_color(c, focuscolor);
	}

	/* Deactivate old client if focus is changing */
	if (old && (!c || client_surface(c) != old)) {
		/* If an overlay is focused, don't focus or activate the client,
		 * but only update its position in fstack to render its border with focuscolor
		 * and focus it after the overlay is closed. */
		if (old_client_type == LayerShell &&
		    wlr_scene_node_coords(&old_l->scene->node, &unused_lx,
					  &unused_ly) &&
		    old_l->layer_surface->current.layer >=
			    ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
			return;
		} else if (old_c && old_c == exclusive_focus &&
			   client_wants_focus(old_c)) {
			return;
			/* Don't deactivate old client if the new one wants focus, as this causes issues with winecfg
		 * and probably other clients */
		} else if (old_c && !client_is_unmanaged(old_c) &&
			   (!c || !client_wants_focus(c))) {
			client_set_border_color(old_c, bordercolor);

			client_activate_surface(old, 0);
		}
	}
	printstatus();

	if (!c) {
		/* With no client, all we have left is to clear focus */
		wlr_seat_keyboard_notify_clear_focus(seat);
		return;
	}

	/* Change cursor surface */
	motionnotify(0);

	/* Have a client, so focus its top-level wlr_surface */
	client_notify_enter(client_surface(c), wlr_seat_get_keyboard(seat));

	/* Activate the new client */
	client_activate_surface(client_surface(c), 1);
}

void focusmon(const Arg *arg)
{
	int i = 0, nmons = wl_list_length(&mons);
	int dir;
	if (horiz_monaxis && arg->i == WLR_DIRECTION_DOWN) {
		dir = WLR_DIRECTION_LEFT;
	} else if (horiz_monaxis && arg->i == WLR_DIRECTION_UP) {
		dir = WLR_DIRECTION_RIGHT;
	} else {
                dir = arg->i;
        }
	if (nmons) {
		do /* don't switch to disabled mons */
			selmon = dirtomon(dir);
		while (!selmon->wlr_output->enabled && i++ < nmons);
	}
	focusclient(focustop(selmon), 1);
}

void monaxis(const Arg *arg)
{
        horiz_monaxis = !horiz_monaxis;
}

void focusstack(const Arg *arg)
{
	/* Focus the next or previous client (in tiling order) on selmon */
	Client *c, *sel = focustop(selmon);
	if (!sel || sel->isfullscreen)
		return;
	if (arg->i > 0) {
		wl_list_for_each(c, &sel->link, link)
		{
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	} else {
		wl_list_for_each_reverse(c, &sel->link, link)
		{
			if (&c->link == &clients)
				continue; /* wrap past the sentinel node */
			if (VISIBLEON(c, selmon))
				break; /* found it */
		}
	}
	/* If only one client is visible on selmon, then c == sel */
	focusclient(c, 1);
}

/* We probably should change the name of this, it sounds like
 * will focus the topmost client of this mon, when actually will
 * only return that client */
Client *focustop(Monitor *m)
{
	Client *c;
	wl_list_for_each(c, &fstack, flink)
	{
		if (VISIBLEON(c, m))
			return c;
	}
	return NULL;
}

void fullscreennotify(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, fullscreen);
	setfullscreen(c, client_wants_fullscreen(c));
}

void handlesig(int signo)
{
	if (signo == SIGCHLD) {
#ifdef XWAYLAND
		siginfo_t in;
		/* wlroots expects to reap the XWayland process itself, so we
		 * use WNOWAIT to keep the child waitable until we know it's not
		 * XWayland.
		 */
		while (!waitid(P_ALL, 0, &in, WEXITED | WNOHANG | WNOWAIT) &&
		       in.si_pid &&
		       (!xwayland || in.si_pid != xwayland->server->pid))
			waitpid(in.si_pid, NULL, 0);
#else
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
#endif
	} else if (signo == SIGINT || signo == SIGTERM) {
		quit(NULL);
	}
}

void incnmaster(const Arg *arg)
{
	if (!arg || !selmon)
		return;
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] =
		MAX(selmon->nmaster + arg->i, 0);
	arrange(selmon);
}

void inputdevice(struct wl_listener *listener, void *data)
{
	/* This event is raised by the backend when a new input device becomes
	 * available. */
	struct wlr_input_device *device = data;
	uint32_t caps;

	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		createkeyboard(wlr_keyboard_from_input_device(device));
		break;
	case WLR_INPUT_DEVICE_POINTER:
		createpointer(wlr_pointer_from_input_device(device));
		break;
	default:
		/* TODO handle other input device types */
		break;
	}

	/* We need to let the wlr_seat know what our capabilities are, which is
	 * communiciated to the client. In dwl we always have a cursor, even if
	 * there are no pointer devices, so we always include that capability. */
	/* TODO do we actually require a cursor? */
	caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&kb_group.wlr_group->devices))
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	wlr_seat_set_capabilities(seat, caps);
}

int keybinding(uint32_t mods, xkb_keysym_t sym)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod) && sym == k->keysym &&
		    k->func) {
			// If it is the first keybinding, aka the one that spawns the terminal, let that stuff pass to the client
			if (k == keys) {
				Client *sel;
				// Pass the keypress to client
				if ((sel = focustop(selmon)) &&
				    !strcmp(client_get_appid(sel), TERMINAL)) {
					return 0;
				}
			}
			k->func(&k->arg);
			return 1;
		}
	}
	return 0;
}

void keypress(struct wl_listener *listener, void *data)
{
	int i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(group->wlr_group->keyboard.xkb_state,
					   keycode, &syms);

	int handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i]) || handled;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysyms = syms;
		group->nsyms = nsyms;
		wl_event_source_timer_update(
			group->key_repeat_source,
			group->wlr_group->keyboard.repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	/* Pass unhandled keycodes along to the client. */
	wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode,
				     event->state);
}

void keypressmod(struct wl_listener *listener, void *data)
{
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	wlr_seat_set_keyboard(seat, &group->wlr_group->keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(
		seat, &group->wlr_group->keyboard.modifiers);
}

int keyrepeat(void *data)
{
	KeyboardGroup *group = data;
	int i;
	if (!group->nsyms || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(
		group->key_repeat_source,
		1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(group->mods, group->keysyms[i]);

	return 0;
}

void killclient(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		client_send_close(sel);
}

void locksession(struct wl_listener *listener, void *data)
{
	struct wlr_session_lock_v1 *session_lock = data;
	SessionLock *lock;
	wlr_scene_node_set_enabled(&locked_bg->node, 1);
	if (cur_lock) {
		wlr_session_lock_v1_destroy(session_lock);
		return;
	}
	lock = session_lock->data = ecalloc(1, sizeof(*lock));
	focusclient(NULL, 0);

	lock->scene = wlr_scene_tree_create(layers[LyrBlock]);
	cur_lock = lock->lock = session_lock;
	locked = 1;

	LISTEN(&session_lock->events.new_surface, &lock->new_surface,
	       createlocksurface);
	LISTEN(&session_lock->events.destroy, &lock->destroy,
	       destroysessionlock);
	LISTEN(&session_lock->events.unlock, &lock->unlock, unlocksession);

	wlr_session_lock_v1_send_locked(session_lock);
}

void maplayersurfacenotify(struct wl_listener *listener, void *data)
{
	motionnotify(0);
}

void mapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is mapped, or ready to display on-screen. */
	Client *p, *w, *c = wl_container_of(listener, c, map);
	Monitor *m;
	int i;

	/* Create scene tree for this client and its border */
	c->scene = client_surface(c)->data =
		wlr_scene_tree_create(layers[LyrTile]);
	wlr_scene_node_set_enabled(&c->scene->node, c->type != XDGShell);
	c->scene_surface =
		c->type == XDGShell ?
			wlr_scene_xdg_surface_create(c->scene, c->surface.xdg) :
			wlr_scene_subsurface_tree_create(c->scene,
							 client_surface(c));
	c->scene->node.data = c->scene_surface->node.data = c;

	client_get_geometry(c, &c->geom);

	/* Handle unmanaged clients first so we can return prior create borders */
	if (client_is_unmanaged(c)) {
		/* Unmanaged clients always are floating */
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		wlr_scene_node_set_position(&c->scene->node,
					    c->geom.x + borderpx,
					    c->geom.y + borderpx);
		if (client_wants_focus(c)) {
			focusclient(c, 1);
			exclusive_focus = c;
		}
		goto unset_fullscreen;
	}

	for (i = 0; i < 4; i++) {
		c->border[i] = wlr_scene_rect_create(c->scene, 0, 0,
						     c->isurgent ? urgentcolor :
								   bordercolor);
		c->border[i]->node.data = c;
	}

	/* Initialize client geometry with room for border */
	client_set_tiled(c, WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT |
				    WLR_EDGE_RIGHT);
	c->geom.width += 2 * c->bw;
	c->geom.height += 2 * c->bw;

	/* Insert this client into client lists. */
	wl_list_insert(&clients, &c->link);
	wl_list_insert(&fstack, &c->flink);

	/* Set initial monitor, tags, floating status, and focus:
	 * we always consider floating, clients that have parent and thus
	 * we set the same tags and monitor than its parent, if not
	 * try to apply rules for them */
	/* TODO: https://github.com/djpohly/dwl/pull/334#issuecomment-1330166324 */
	if (c->type == XDGShell && (p = client_get_parent(c))) {
		c->isfloating = 1;
		wlr_scene_node_reparent(&c->scene->node, layers[LyrFloat]);
		setmon(c, p->mon, p->tags);
	} else {
		applyrules(c);
	}
	printstatus();

unset_fullscreen:
	m = c->mon ? c->mon : xytomon(c->geom.x, c->geom.y);
	wl_list_for_each(w, &clients, link)
	{
		if (w != c && w->isfullscreen && m == w->mon &&
		    (w->tags & c->tags))
			setfullscreen(w, 0);
	}
}

void maximizenotify(struct wl_listener *listener, void *data)
{
	/* This event is raised when a client would like to maximize itself,
	 * typically because the user clicked on the maximize button on
	 * client-side decorations. dwl doesn't support maximization, but
	 * to conform to xdg-shell protocol we still must send a configure.
	 * Since xdg-shell protocol v5 we should ignore request of unsupported
	 * capabilities, just schedule a empty configure when the client uses <5
	 * protocol version
	 * wlr_xdg_surface_schedule_configure() is used to send an empty reply. */
	Client *c = wl_container_of(listener, c, maximize);
	if (wl_resource_get_version(c->surface.xdg->toplevel->resource) <
	    XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION)
		wlr_xdg_surface_schedule_configure(c->surface.xdg);
}

void monocle(Monitor *m)
{
	Client *c;
	int n = 0;

	wl_list_for_each(c, &clients, link)
	{
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		resize(c, m->w, 0, !smartborders);
		n++;
	}
	if (n)
		snprintf(m->ltsymbol, LENGTH(m->ltsymbol), "[%d]", n);
	if ((c = focustop(m)))
		wlr_scene_node_raise_to_top(&c->scene->node);
}

void motionabsolute(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits an _absolute_
	 * motion event, from 0..1 on each axis. This happens, for example, when
	 * wlroots is running under a Wayland window rather than KMS+DRM, and you
	 * move the mouse over the window. You could enter the window from any edge,
	 * so we have to warp the mouse there. There is also some hardware which
	 * emits these events. */
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x,
				 event->y);
	motionnotify(event->time_msec);
}
void floatvmovebykey(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || client_is_unmanaged(sel) || sel->isfullscreen)
		return;
	setfloating(sel, 1);
	resize(sel,
	       (struct wlr_box){ .x = sel->geom.x + arg->i,
				 .y = sel->geom.y,
				 .width = sel->geom.width,
				 .height = sel->geom.height },
	       1, 1);
}

void floathmovebykey(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || client_is_unmanaged(sel) || sel->isfullscreen)
		return;
	setfloating(sel, 1);
	resize(sel,
	       (struct wlr_box){ .x = sel->geom.x,
				 .y = sel->geom.y + arg->i,
				 .width = sel->geom.width,
				 .height = sel->geom.height },
	       1, 1);
}
void motionnotify(uint32_t time)
{
	double sx = 0, sy = 0;
	Client *c = NULL, *w = NULL;
	LayerSurface *l = NULL;
	struct wlr_surface *surface = NULL;

	/* time is 0 in internal calls meant to restore pointer focus. */
	if (time) {
		wlr_idle_notifier_v1_notify_activity(idle_notifier, seat);

		/* Update selmon (even while dragging a window) */
		if (sloppyfocus)
			selmon = xytomon(cursor->x, cursor->y);
	}

	/* Update drag icon's position */
	wlr_scene_node_set_position(&drag_icon->node, ROUND(cursor->x),
				    ROUND(cursor->y));

	/* If we are currently grabbing the mouse, handle and return */
	if (cursor_mode == CurMove) {
		/* Move the grabbed client to the new position. */
		resize(grabc,
		       (struct wlr_box){ .x = ROUND(cursor->x) - grabcx,
					 .y = ROUND(cursor->y) - grabcy,
					 .width = grabc->geom.width,
					 .height = grabc->geom.height },
		       1, 1);
		return;
	} else if (cursor_mode == CurResize) {
		resize(grabc,
		       (struct wlr_box){
			       .x = grabc->geom.x,
			       .y = grabc->geom.y,
			       .width = ROUND(cursor->x) - grabc->geom.x,
			       .height = ROUND(cursor->y) - grabc->geom.y },
		       1, 1);
		return;
	}

	/* Find the client under the pointer and send the event along. */
	xytonode(cursor->x, cursor->y, &surface, &c, NULL, &sx, &sy);

	if (cursor_mode == CurPressed && !seat->drag && surface != held_grab &&
	    toplevel_from_wlr_surface(held_grab, &w, &l) >= 0) {
		c = w;
		surface = held_grab;
		sx = cursor->x - (l ? l->geom.x : w->geom.x);
		sy = cursor->y - (l ? l->geom.y : w->geom.y);
	}

	/* If there's no client surface under the cursor, set the cursor image to a
	 * default. This is what makes the cursor image appear when you move it
	 * off of a client or over its border. */
	if (!surface && !seat->drag)
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	pointerfocus(c, surface, sx, sy, time);
}

void motionrelative(struct wl_listener *listener, void *data)
{
	/* This event is forwarded by the cursor when a pointer emits a _relative_
	 * pointer motion event (i.e. a delta) */
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(cursor, &event->pointer->base, event->delta_x,
			event->delta_y);
	motionnotify(event->time_msec);
}

void moveresize(const Arg *arg)
{
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	xytonode(cursor->x, cursor->y, NULL, &grabc, NULL, NULL, NULL);
	if (!grabc || client_is_unmanaged(grabc) || grabc->isfullscreen)
		return;

	/* Float the window and tell motionnotify to grab it */
	setfloating(grabc, 1);
	switch (cursor_mode = arg->ui) {
	case CurMove:
		grabcx = ROUND(cursor->x) - grabc->geom.x;
		grabcy = ROUND(cursor->y) - grabc->geom.y;
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "fleur");
		break;
	case CurResize:
		/* Doesn't work for X11 output - the next absolute motion event
		 * returns the cursor to where it started */
		wlr_cursor_warp_closest(cursor, NULL,
					grabc->geom.x + grabc->geom.width,
					grabc->geom.y + grabc->geom.height);
		wlr_cursor_set_xcursor(cursor, cursor_mgr, "se-resize");
		break;
	}
}

void outputmgrapply(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 0);
}

void outputmgrapplyortest(struct wlr_output_configuration_v1 *config, int test)
{
	/*
	 * Called when a client such as wlr-randr requests a change in output
	 * configuration. This is only one way that the layout can be changed,
	 * so any Monitor information should be updated by updatemons() after an
	 * output_layout.change event, not here.
	 */
	struct wlr_output_configuration_head_v1 *config_head;
	int ok = 1;

	wl_list_for_each(config_head, &config->heads, link)
	{
		struct wlr_output *wlr_output = config_head->state.output;
		Monitor *m = wlr_output->data;
		struct wlr_output_state state;

		wlr_output_state_init(&state);
		wlr_output_state_set_enabled(&state,
					     config_head->state.enabled);
		if (!config_head->state.enabled)
			goto apply_or_test;

		if (config_head->state.mode)
			wlr_output_state_set_mode(&state,
						  config_head->state.mode);
		else
			wlr_output_state_set_custom_mode(
				&state, config_head->state.custom_mode.width,
				config_head->state.custom_mode.height,
				config_head->state.custom_mode.refresh);

		/* Don't move monitors if position wouldn't change, this to avoid
		 * wlroots marking the output as manually configured */
		if (m->m.x != config_head->state.x ||
		    m->m.y != config_head->state.y)
			wlr_output_layout_add(output_layout, wlr_output,
					      config_head->state.x,
					      config_head->state.y);
		wlr_output_state_set_transform(&state,
					       config_head->state.transform);
		wlr_output_state_set_scale(&state, config_head->state.scale);
		wlr_output_state_set_adaptive_sync_enabled(
			&state, config_head->state.adaptive_sync_enabled);

apply_or_test:
		ok &= test ? wlr_output_test_state(wlr_output, &state) :
			     wlr_output_commit_state(wlr_output, &state);

		wlr_output_state_finish(&state);
	}

	if (ok)
		wlr_output_configuration_v1_send_succeeded(config);
	else
		wlr_output_configuration_v1_send_failed(config);
	wlr_output_configuration_v1_destroy(config);

	/* TODO: use a wrapper function? */
	updatemons(NULL, NULL);
}

void outputmgrtest(struct wl_listener *listener, void *data)
{
	struct wlr_output_configuration_v1 *config = data;
	outputmgrapplyortest(config, 1);
}

void pointerfocus(Client *c, struct wlr_surface *surface, double sx, double sy,
		  uint32_t time)
{
	struct timespec now;

	if (sloppyfocus && time && c && !client_is_unmanaged(c))
		focusclient(c, 0);

	/* If surface is NULL, clear pointer focus */
	if (!surface) {
		wlr_seat_pointer_notify_clear_focus(seat);
		return;
	}

	if (!time) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
	}

	/* Let the client know that the mouse cursor has entered one
	 * of its surfaces, and make keyboard focus follow if desired.
	 * wlroots makes this a no-op if surface is already focused */
	wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
	wlr_seat_pointer_notify_motion(seat, time, sx, sy);
}

void printstatus(void)
{
	Monitor *m = NULL;
	Client *c;
	uint32_t occ, urg, sel;
	const char *appid, *title;

	wl_list_for_each(m, &mons, link)
	{
		occ = urg = 0;
		wl_list_for_each(c, &clients, link)
		{
			if (c->mon != m)
				continue;
			occ |= c->tags;
			if (c->isurgent)
				urg |= c->tags;
		}
		if ((c = focustop(m))) {
			title = client_get_title(c);
			appid = client_get_appid(c);
			printf("%s title %s\n", m->wlr_output->name,
			       title ? title : broken);
			printf("%s appid %s\n", m->wlr_output->name,
			       appid ? appid : broken);
			printf("%s fullscreen %u\n", m->wlr_output->name,
			       c->isfullscreen);
			printf("%s floating %u\n", m->wlr_output->name,
			       c->isfloating);
			sel = c->tags;
		} else {
			printf("%s title \n", m->wlr_output->name);
			printf("%s appid \n", m->wlr_output->name);
			printf("%s fullscreen \n", m->wlr_output->name);
			printf("%s floating \n", m->wlr_output->name);
			sel = 0;
		}

		printf("%s selmon %u\n", m->wlr_output->name, m == selmon);
		printf("%s tags %u %u %u %u\n", m->wlr_output->name, occ,
		       m->tagset[m->seltags], sel, urg);
		printf("%s layout %s\n", m->wlr_output->name, m->ltsymbol);
	}
	fflush(stdout);
}

void quit(const Arg *arg)
{
	wl_display_terminate(dpy);
}

void rendermon(struct wl_listener *listener, void *data)
{
	/* This function is called every time an output is ready to display a frame,
	 * generally at the output's refresh rate (e.g. 60Hz). */
	Monitor *m = wl_container_of(listener, m, frame);
	Client *c;
	struct wlr_output_state pending = { 0 };
	struct wlr_gamma_control_v1 *gamma_control;
	struct timespec now;

	/* Render if no XDG clients have an outstanding resize and are visible on
	 * this monitor. */
	wl_list_for_each(c, &clients, link)
	{
		if (c->resize && !c->isfloating &&
		    client_is_rendered_on_mon(c, m) && !client_is_stopped(c))
			goto skip;
	}

	/*
	 * HACK: The "correct" way to set the gamma is to commit it together with
	 * the rest of the state in one go, but to do that we would need to rewrite
	 * wlr_scene_output_commit() in order to add the gamma to the pending
	 * state before committing, instead try to commit the gamma in one frame,
	 * and commit the rest of the state in the next one (or in the same frame if
	 * the gamma can not be committed).
	 */
	if (m->gamma_lut_changed) {
		gamma_control = wlr_gamma_control_manager_v1_get_control(
			gamma_control_mgr, m->wlr_output);
		m->gamma_lut_changed = 0;

		if (!wlr_gamma_control_v1_apply(gamma_control, &pending))
			goto commit;

		if (!wlr_output_test_state(m->wlr_output, &pending)) {
			wlr_gamma_control_v1_send_failed_and_destroy(
				gamma_control);
			goto commit;
		}
		wlr_output_commit_state(m->wlr_output, &pending);
		wlr_output_schedule_frame(m->wlr_output);
	} else {
commit:
		wlr_scene_output_commit(m->scene_output, NULL);
	}

skip:
	/* Let clients know a frame has been rendered */
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(m->scene_output, &now);
	wlr_output_state_finish(&pending);
}

void requestdecorationmode(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_decoration_mode);
	wlr_xdg_toplevel_decoration_v1_set_mode(
		c->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
}

void requeststartdrag(struct wl_listener *listener, void *data)
{
	struct wlr_seat_request_start_drag_event *event = data;

	if (wlr_seat_validate_pointer_grab_serial(seat, event->origin,
						  event->serial))
		wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

void requestmonstate(struct wl_listener *listener, void *data)
{
	struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(event->output, event->state);
	updatemons(NULL, NULL);
}

void resize(Client *c, struct wlr_box geo, int interact, int draw_borders)
{
	struct wlr_box *bbox = interact ? &sgeom : &c->mon->w;
	struct wlr_box clip;
	client_set_bounds(c, geo.width, geo.height);
	c->geom = geo;
	c->bw = draw_borders ? borderpx : 0;
	applybounds(c, bbox);

	/* Update scene-graph, including borders */
	wlr_scene_node_set_position(&c->scene->node, c->geom.x, c->geom.y);
	wlr_scene_node_set_position(&c->scene_surface->node, c->bw, c->bw);
	wlr_scene_rect_set_size(c->border[0], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[1], c->geom.width, c->bw);
	wlr_scene_rect_set_size(c->border[2], c->bw,
				c->geom.height - 2 * c->bw);
	wlr_scene_rect_set_size(c->border[3], c->bw,
				c->geom.height - 2 * c->bw);
	wlr_scene_node_set_position(&c->border[1]->node, 0,
				    c->geom.height - c->bw);
	wlr_scene_node_set_position(&c->border[2]->node, 0, c->bw);
	wlr_scene_node_set_position(&c->border[3]->node, c->geom.width - c->bw,
				    c->bw);

	/* this is a no-op if size hasn't changed */
	c->resize = client_set_size(c, c->geom.width - 2 * c->bw,
				    c->geom.height - 2 * c->bw);
	client_get_clip(c, &clip);
	wlr_scene_subsurface_tree_set_clip(&c->scene_surface->node, &clip);
}

void run(char *startup_cmd)
{
	/* Add a Unix socket to the Wayland display. */
	const char *socket = wl_display_add_socket_auto(dpy);
	if (!socket)
		die("startup: display_add_socket_auto");
	setenv("WAYLAND_DISPLAY", socket, 1);

	/* Start the backend. This will enumerate outputs and inputs, become the DRM
	 * master, etc */
	if (!wlr_backend_start(backend))
		die("startup: backend_start");

	/* Now that the socket exists and the backend is started, run the startup command */
	if (startup_cmd) {
		int piperw[2];
		if (pipe(piperw) < 0)
			die("startup: pipe:");
		if ((child_pid = fork()) < 0)
			die("startup: fork:");
		if (child_pid == 0) {
			dup2(piperw[0], STDIN_FILENO);
			close(piperw[0]);
			close(piperw[1]);
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, NULL);
			die("startup: execl:");
		}
		dup2(piperw[1], STDOUT_FILENO);
		close(piperw[1]);
		close(piperw[0]);
	}
	printstatus();

	/* At this point the outputs are initialized, choose initial selmon based on
	 * cursor position, and set default cursor image */
	selmon = xytomon(cursor->x, cursor->y);

	/* TODO hack to get cursor to display in its initial location (100, 100)
	 * instead of (0, 0) and then jumping. still may not be fully
	 * initialized, as the image/coordinates are not transformed for the
	 * monitor when displayed here */
	wlr_cursor_warp_closest(cursor, NULL, cursor->x, cursor->y);
	wlr_cursor_set_xcursor(cursor, cursor_mgr, "default");

	/* Run the Wayland event loop. This does not return until you exit the
	 * compositor. Starting the backend rigged up all of the necessary event
	 * loop configuration to listen to libinput events, DRM events, generate
	 * frame events at the refresh rate, and so on. */
	wl_display_run(dpy);
}

void setcursor(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client provides a cursor image */
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	/* If we're "grabbing" the cursor, don't use the client's image, we will
	 * restore it after "grabbing" sending a leave event, followed by a enter
	 * event, which will result in the client requesting set the cursor surface */
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided surface as the cursor image. It will set the
	 * hardware cursor on the output that it's currently on and continue to
	 * do so as the cursor moves between outputs. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x,
				       event->hotspot_y);
}

void setcursorshape(struct wl_listener *listener, void *data)
{
	struct wlr_cursor_shape_manager_v1_request_set_shape_event *event =
		data;
	if (cursor_mode != CurNormal && cursor_mode != CurPressed)
		return;
	/* This can be sent by any client, so we check to make sure this one is
	 * actually has pointer focus first. If so, we can tell the cursor to
	 * use the provided cursor shape. */
	if (event->seat_client == seat->pointer_state.focused_client)
		wlr_cursor_set_xcursor(cursor, cursor_mgr,
				       wlr_cursor_shape_v1_name(event->shape));
}

void setfloating(Client *c, int floating)
{
	c->isfloating = floating;
	if (!c->mon)
		return;
	wlr_scene_node_reparent(&c->scene->node,
				layers[c->isfullscreen ? LyrFS :
				       c->isfloating   ? LyrFloat :
							 LyrTile]);
	if (c->isfloating && !c->bw)
		resize(c, c->mon->m, 0, 1);
	arrange(c->mon);
	printstatus();
}

void setfullscreen(Client *c, int fullscreen)
{
	c->isfullscreen = fullscreen;
	if (!c->mon)
		return;
	c->bw = fullscreen ? 0 : borderpx;
	client_set_fullscreen(c, fullscreen);
	wlr_scene_node_reparent(&c->scene->node,
				layers[c->isfullscreen ? LyrFS :
				       c->isfloating   ? LyrFloat :
							 LyrTile]);

	if (fullscreen) {
		c->prev = c->geom;
		resize(c, c->mon->m, 0, 0);
	} else {
		/* restore previous size instead of arrange for floating windows since
		 * client positions are set by the user and cannot be recalculated */
		resize(c, c->prev, 0, 1);
	}
	arrange(c->mon);
	printstatus();
}

void setgamma(struct wl_listener *listener, void *data)
{
	struct wlr_gamma_control_manager_v1_set_gamma_event *event = data;
	Monitor *m = event->output->data;
	if (!m)
		return;
	m->gamma_lut_changed = 1;
	wlr_output_schedule_frame(m->wlr_output);
}

void setlayout(const Arg *arg)
{
	if (!selmon)
		return;
	if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
		selmon->sellt =
			selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
	if (arg && arg->v)
		selmon->lt[selmon->sellt] =
			selmon->pertag->ltidxs[selmon->pertag->curtag]
					      [selmon->sellt] =
				(Layout *)arg->v;
	strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol,
		LENGTH(selmon->ltsymbol));
	if (!selmon->lt[selmon->sellt]->arrange) {
		/* floating layout, draw borders around all clients */
		Client *c;
		wl_list_for_each(c, &clients, link) resize(c, c->mon->m, 0, 1);
	}
	arrange(selmon);
	printstatus();
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg)
{
	float f;

	if (!arg || !selmon || !selmon->lt[selmon->sellt]->arrange)
		return;
	f = arg->f < 1.0f ? arg->f + selmon->mfact : arg->f - 1.0f;
	if (f < 0.1 || f > 0.9)
		return;
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
	arrange(selmon);
}

void setmon(Client *c, Monitor *m, uint32_t newtags)
{
	Monitor *oldmon = c->mon;

	if (oldmon == m)
		return;
	c->mon = m;
	c->prev = c->geom;

	/* Scene graph sends surface leave/enter events on move and resize */
	if (oldmon)
		arrange(oldmon);
	if (m) {
		/* Make sure window actually overlaps with the monitor */
		resize(c, c->geom, 0, 1);
		c->tags =
			newtags ?
				newtags :
				m->tagset[m->seltags]; /* assign tags of target monitor */
		setfullscreen(
			c,
			c->isfullscreen); /* This will call arrange(c->mon) */
		setfloating(c, c->isfloating);
	}
	focusclient(focustop(selmon), 1);
}

void setpsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void setsel(struct wl_listener *listener, void *data)
{
	/* This event is raised by the seat when a client wants to set the selection,
	 * usually when the user copies something. wlroots allows compositors to
	 * ignore such requests if they so choose, but in dwl we always honor
	 */
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(seat, event->source, event->serial);
}

void setup(void)
{
	struct xkb_context *context;
	struct xkb_keymap *keymap;

	int i, sig[] = { SIGCHLD, SIGINT, SIGTERM, SIGPIPE };
	struct sigaction sa = { .sa_flags = SA_RESTART,
				.sa_handler = handlesig };
	sigemptyset(&sa.sa_mask);

	for (i = 0; i < (int)LENGTH(sig); i++)
		sigaction(sig[i], &sa, NULL);

	wlr_log_init(log_level, NULL);

	/* The Wayland display is managed by libwayland. It handles accepting
	 * clients from the Unix socket, manging Wayland globals, and so on. */
	dpy = wl_display_create();

	/* The backend is a wlroots feature which abstracts the underlying input and
	 * output hardware. The autocreate option will choose the most suitable
	 * backend based on the current environment, such as opening an X11 window
	 * if an X11 server is running. */
	if (!(backend = wlr_backend_autocreate(dpy, &session)))
		die("couldn't create backend");

	/* Initialize the scene graph used to lay out windows */
	scene = wlr_scene_create();
	root_bg = wlr_scene_rect_create(&scene->tree, 0, 0, rootcolor);
	for (i = 0; i < NUM_LAYERS; i++)
		layers[i] = wlr_scene_tree_create(&scene->tree);
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

	/* Autocreates a renderer, either Pixman, GLES2 or Vulkan for us. The user
	 * can also specify a renderer using the WLR_RENDERER env var.
	 * The renderer is responsible for defining the various pixel formats it
	 * supports for shared memory, this configures that for clients. */
	if (!(drw = wlr_renderer_autocreate(backend)))
		die("couldn't create renderer");

	/* Create shm, drm and linux_dmabuf interfaces by ourselves.
	 * The simplest way is call:
	 *      wlr_renderer_init_wl_display(drw);
	 * but we need to create manually the linux_dmabuf interface to integrate it
	 * with wlr_scene. */
	wlr_renderer_init_wl_shm(drw, dpy);

	if (wlr_renderer_get_dmabuf_texture_formats(drw)) {
		wlr_drm_create(dpy, drw);
		wlr_scene_set_linux_dmabuf_v1(
			scene,
			wlr_linux_dmabuf_v1_create_with_renderer(dpy, 4, drw));
	}

	/* Autocreates an allocator for us.
	 * The allocator is the bridge between the renderer and the backend. It
	 * handles the buffer creation, allowing wlroots to render onto the
	 * screen */
	if (!(alloc = wlr_allocator_autocreate(backend, drw)))
		die("couldn't create allocator");

	/* This creates some hands-off wlroots interfaces. The compositor is
	 * necessary for clients to allocate surfaces and the data device manager
	 * handles the clipboard. Each of these wlroots interfaces has room for you
	 * to dig your fingers in and play with their behavior if you want. Note that
	 * the clients cannot set the selection directly without compositor approval,
	 * see the setsel() function. */
	compositor = wlr_compositor_create(dpy, 6, drw);
	wlr_subcompositor_create(dpy);
	wlr_data_device_manager_create(dpy);
	wlr_export_dmabuf_manager_v1_create(dpy);
	wlr_screencopy_manager_v1_create(dpy);
	wlr_data_control_manager_v1_create(dpy);
	wlr_primary_selection_v1_device_manager_create(dpy);
	wlr_viewporter_create(dpy);
	wlr_single_pixel_buffer_manager_v1_create(dpy);
	wlr_fractional_scale_manager_v1_create(dpy, 1);

	/* Initializes the interface used to implement urgency hints */
	activation = wlr_xdg_activation_v1_create(dpy);
	LISTEN_STATIC(&activation->events.request_activate, urgent);

	gamma_control_mgr = wlr_gamma_control_manager_v1_create(dpy);
	LISTEN_STATIC(&gamma_control_mgr->events.set_gamma, setgamma);

	/* Creates an output layout, which a wlroots utility for working with an
	 * arrangement of screens in a physical layout. */
	output_layout = wlr_output_layout_create();
	LISTEN_STATIC(&output_layout->events.change, updatemons);
	wlr_xdg_output_manager_v1_create(dpy, output_layout);

	/* Configure a listener to be notified when new outputs are available on the
	 * backend. */
	wl_list_init(&mons);
	LISTEN_STATIC(&backend->events.new_output, createmon);

	/* Set up our client lists, the xdg-shell and the layer-shell. The xdg-shell is a
	 * Wayland protocol which is used for application windows. For more
	 * detail on shells, refer to the article:
	 *
	 * https://drewdevault.com/2018/07/29/Wayland-shells.html
	 */
	wl_list_init(&clients);
	wl_list_init(&fstack);

	xdg_shell = wlr_xdg_shell_create(dpy, 6);
	LISTEN_STATIC(&xdg_shell->events.new_surface, createnotify);

	layer_shell = wlr_layer_shell_v1_create(dpy, 3);
	LISTEN_STATIC(&layer_shell->events.new_surface, createlayersurface);

	idle_notifier = wlr_idle_notifier_v1_create(dpy);

	idle_inhibit_mgr = wlr_idle_inhibit_v1_create(dpy);
	LISTEN_STATIC(&idle_inhibit_mgr->events.new_inhibitor,
		      createidleinhibitor);

	session_lock_mgr = wlr_session_lock_manager_v1_create(dpy);
	wl_signal_add(&session_lock_mgr->events.new_lock, &lock_listener);
	LISTEN_STATIC(&session_lock_mgr->events.destroy, destroysessionmgr);
	locked_bg = wlr_scene_rect_create(layers[LyrBlock], sgeom.width,
					  sgeom.height,
					  (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f });
	wlr_scene_node_set_enabled(&locked_bg->node, 0);

	/* Use decoration protocols to negotiate server-side decorations */
	wlr_server_decoration_manager_set_default_mode(
		wlr_server_decoration_manager_create(dpy),
		WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(dpy);
	LISTEN_STATIC(&xdg_decoration_mgr->events.new_toplevel_decoration,
		      createdecoration);

	/*
	 * Creates a cursor, which is a wlroots utility for tracking the cursor
	 * image shown on screen.
	 */
	cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(cursor, output_layout);

	/* Creates an xcursor manager, another wlroots utility which loads up
	 * Xcursor themes to source cursor images from and makes sure that cursor
	 * images are available at all scale factors on the screen (necessary for
	 * HiDPI support). Scaled cursors will be loaded with each output. */
	cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
	setenv("XCURSOR_SIZE", "24", 1);

	/*
	 * wlr_cursor *only* displays an image on screen. It does not move around
	 * when the pointer moves. However, we can attach input devices to it, and
	 * it will generate aggregate events for all of them. In these events, we
	 * can choose how we want to process them, forwarding them to clients and
	 * moving the cursor around. More detail on this process is described in
	 * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html
	 *
	 * And more comments are sprinkled throughout the notify functions above.
	 */
	LISTEN_STATIC(&cursor->events.motion, motionrelative);
	LISTEN_STATIC(&cursor->events.motion_absolute, motionabsolute);
	LISTEN_STATIC(&cursor->events.button, buttonpress);
	LISTEN_STATIC(&cursor->events.axis, axisnotify);
	LISTEN_STATIC(&cursor->events.frame, cursorframe);

	cursor_shape_mgr = wlr_cursor_shape_manager_v1_create(dpy, 1);
	LISTEN_STATIC(&cursor_shape_mgr->events.request_set_shape,
		      setcursorshape);

	/*
	 * Configures a seat, which is a single "seat" at which a user sits and
	 * operates the computer. This conceptually includes up to one keyboard,
	 * pointer, touch, and drawing tablet device. We also rig up a listener to
	 * let us know when new input devices are available on the backend.
	 */
	LISTEN_STATIC(&backend->events.new_input, inputdevice);
	virtual_keyboard_mgr = wlr_virtual_keyboard_manager_v1_create(dpy);
	LISTEN_STATIC(&virtual_keyboard_mgr->events.new_virtual_keyboard,
		      virtualkeyboard);
	seat = wlr_seat_create(dpy, "seat0");
	LISTEN_STATIC(&seat->events.request_set_cursor, setcursor);
	LISTEN_STATIC(&seat->events.request_set_selection, setsel);
	LISTEN_STATIC(&seat->events.request_set_primary_selection, setpsel);
	LISTEN_STATIC(&seat->events.request_start_drag, requeststartdrag);
	LISTEN_STATIC(&seat->events.start_drag, startdrag);

	/*
	 * Configures a keyboard group, which will keep track of all connected
	 * keyboards, keep their modifier and LED states in sync, and handle
	 * keypresses
	 */
	kb_group.wlr_group = wlr_keyboard_group_create();
	kb_group.wlr_group->data = &kb_group;

	/*
	 * Virtual keyboards need to be in a different group
	 * https://codeberg.org/dwl/dwl/issues/554
	 */
	vkb_group.wlr_group = wlr_keyboard_group_create();
	vkb_group.wlr_group->data = &vkb_group;

	/* Prepare an XKB keymap and assign it to the keyboard group. */
	context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!(keymap = xkb_keymap_new_from_names(context, &xkb_rules,
						 XKB_KEYMAP_COMPILE_NO_FLAGS)))
		die("failed to compile keymap");

	wlr_keyboard_set_keymap(&kb_group.wlr_group->keyboard, keymap);
	wlr_keyboard_set_keymap(&vkb_group.wlr_group->keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);

	wlr_keyboard_set_repeat_info(&kb_group.wlr_group->keyboard, repeat_rate,
				     repeat_delay);
	wlr_keyboard_set_repeat_info(&vkb_group.wlr_group->keyboard,
				     repeat_rate, repeat_delay);

	/* Set up listeners for keyboard events */
	LISTEN(&kb_group.wlr_group->keyboard.events.key, &kb_group.key,
	       keypress);
	LISTEN(&kb_group.wlr_group->keyboard.events.modifiers,
	       &kb_group.modifiers, keypressmod);
	LISTEN(&vkb_group.wlr_group->keyboard.events.key, &vkb_group.key,
	       keypress);
	LISTEN(&vkb_group.wlr_group->keyboard.events.modifiers,
	       &vkb_group.modifiers, keypressmod);

	kb_group.key_repeat_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), keyrepeat, &kb_group);
	vkb_group.key_repeat_source = wl_event_loop_add_timer(
		wl_display_get_event_loop(dpy), keyrepeat, &vkb_group);

	/* A seat can only have one keyboard, but this is a limitation of the
	 * Wayland protocol - not wlroots. We assign all connected keyboards to the
	 * same wlr_keyboard_group, which provides a single wlr_keyboard interface for
	 * all of them. Set this combined wlr_keyboard as the seat keyboard.
	 */
	wlr_seat_set_keyboard(seat, &kb_group.wlr_group->keyboard);

	output_mgr = wlr_output_manager_v1_create(dpy);
	LISTEN_STATIC(&output_mgr->events.apply, outputmgrapply);
	LISTEN_STATIC(&output_mgr->events.test, outputmgrtest);

	wlr_scene_set_presentation(scene,
				   wlr_presentation_create(dpy, backend));

	/* Make sure XWayland clients don't connect to the parent X server,
	 * e.g when running in the x11 backend or the wayland backend and the
	 * compositor has Xwayland support */
	unsetenv("DISPLAY");
#ifdef XWAYLAND
	/*
	 * Initialise the XWayland X server.
	 * It will be started when the first X client is started.
	 */
	if ((xwayland = wlr_xwayland_create(dpy, compositor, 1))) {
		LISTEN_STATIC(&xwayland->events.ready, xwaylandready);
		LISTEN_STATIC(&xwayland->events.new_surface, createnotifyx11);

		setenv("DISPLAY", xwayland->display_name, 1);
	} else {
		fprintf(stderr,
			"failed to setup XWayland X server, continuing without it\n");
	}
#endif
}

void spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		die("dwl: execvp %s failed:", ((char **)arg->v)[0]);
	}
}

void startdrag(struct wl_listener *listener, void *data)
{
	struct wlr_drag *drag = data;
	if (!drag->icon)
		return;

	drag->icon->data =
		&wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void tag(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (!sel || (arg->ui & TAGMASK) == 0)
		return;

	sel->tags = arg->ui & TAGMASK;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void tagmon(const Arg *arg)
{

	Client *sel = focustop(selmon);
	int dir;
	if (horiz_monaxis && arg->i == WLR_DIRECTION_DOWN) {
		dir = WLR_DIRECTION_LEFT;
	} else if (horiz_monaxis && arg->i == WLR_DIRECTION_UP) {
		dir = WLR_DIRECTION_RIGHT;
	} else {
                dir = arg->i;
        }
	if (sel)
		setmon(sel, dirtomon(dir), 0);
}

void tile(Monitor *m)
{
	unsigned int mw, my, ty, draw_borders = 1;
	int i, n = 0;
	Client *c;

	wl_list_for_each(c, &clients, link) if (VISIBLEON(c, m) &&
						!c->isfloating &&
						!c->isfullscreen) n++;
	if (n == 0)
		return;

	if (n == smartborders)
		draw_borders = 0;

	if (n > m->nmaster)
		mw = m->nmaster ? ROUND(m->w.width * m->mfact) : 0;
	else
		mw = m->w.width;
	i = my = ty = 0;
	wl_list_for_each(c, &clients, link)
	{
		if (!VISIBLEON(c, m) || c->isfloating || c->isfullscreen)
			continue;
		if (i < m->nmaster) {
			resize(c,
			       (struct wlr_box){
				       .x = m->w.x,
				       .y = m->w.y + my,
				       .width = mw,
				       .height = (m->w.height - my) /
						 (MIN(n, m->nmaster) - i) },
			       0, draw_borders);
			my += c->geom.height;
		} else {
			resize(c,
			       (struct wlr_box){ .x = m->w.x + mw,
						 .y = m->w.y + ty,
						 .width = m->w.width - mw,
						 .height = (m->w.height - ty) /
							   (n - i) },
			       0, draw_borders);
			ty += c->geom.height;
		}
		i++;
	}
}

void togglefloating(const Arg *arg)
{
	Client *sel = focustop(selmon);
	/* return if fullscreen */
	if (sel && !sel->isfullscreen)
		setfloating(sel, !sel->isfloating);
}

void togglefullscreen(const Arg *arg)
{
	Client *sel = focustop(selmon);
	if (sel)
		setfullscreen(sel, !sel->isfullscreen);
}

void toggletag(const Arg *arg)
{
	uint32_t newtags;
	Client *sel = focustop(selmon);
	if (!sel || !(newtags = sel->tags ^ (arg->ui & TAGMASK)))
		return;

	sel->tags = newtags;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void toggleview(const Arg *arg)
{
	uint32_t newtagset = selmon ? selmon->tagset[selmon->seltags] ^
					      (arg->ui & TAGMASK) :
				      0;
	size_t i;

	if (!newtagset)
		return;

	if (newtagset == ~0) {
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = 0;
	}

	/* test if the user did not select the same tag */
	if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
		selmon->pertag->prevtag = selmon->pertag->curtag;
		for (i = 0; !(newtagset & 1 << i); i++)
			;
		selmon->pertag->curtag = i + 1;
	}

	/* apply settings for this view */
	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] =
		selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt ^ 1] =
		selmon->pertag
			->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

	selmon->tagset[selmon->seltags] = newtagset;
	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void unlocksession(struct wl_listener *listener, void *data)
{
	SessionLock *lock = wl_container_of(listener, lock, unlock);
	destroylock(lock, 1);
}

void unmaplayersurfacenotify(struct wl_listener *listener, void *data)
{
	LayerSurface *l = wl_container_of(listener, l, unmap);

	l->mapped = 0;
	wlr_scene_node_set_enabled(&l->scene->node, 0);
	if (l == exclusive_focus)
		exclusive_focus = NULL;
	if (l->layer_surface->output &&
	    (l->mon = l->layer_surface->output->data))
		arrangelayers(l->mon);
	if (l->layer_surface->surface == seat->keyboard_state.focused_surface)
		focusclient(focustop(selmon), 1);
	motionnotify(0);
}

void unmapnotify(struct wl_listener *listener, void *data)
{
	/* Called when the surface is unmapped, and should no longer be shown. */
	Client *c = wl_container_of(listener, c, unmap);
	if (c == grabc) {
		cursor_mode = CurNormal;
		grabc = NULL;
	}

	if (client_is_unmanaged(c)) {
		if (c == exclusive_focus) {
			exclusive_focus = NULL;
			focusclient(focustop(selmon), 1);
		}
	} else {
		wl_list_remove(&c->link);
		setmon(c, NULL, 0);
		wl_list_remove(&c->flink);
	}

	wlr_scene_node_destroy(&c->scene->node);
	printstatus();
	motionnotify(0);
}

void updatemons(struct wl_listener *listener, void *data)
{
	/*
	 * Called whenever the output layout changes: adding or removing a
	 * monitor, changing an output's mode or position, etc. This is where
	 * the change officially happens and we update geometry, window
	 * positions, focus, and the stored configuration in wlroots'
	 * output-manager implementation.
	 */
	struct wlr_output_configuration_v1 *config =
		wlr_output_configuration_v1_create();
	Client *c;
	struct wlr_output_configuration_head_v1 *config_head;
	Monitor *m;

	/* First remove from the layout the disabled monitors */
	wl_list_for_each(m, &mons, link)
	{
		if (m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(
			config, m->wlr_output);
		config_head->state.enabled = 0;
		/* Remove this output from the layout to avoid cursor enter inside it */
		wlr_output_layout_remove(output_layout, m->wlr_output);
		closemon(m);
		m->m = m->w = (struct wlr_box){ 0 };
	}
	/* Insert outputs that need to */
	wl_list_for_each(m, &mons, link)
	{
		if (m->wlr_output->enabled &&
		    !wlr_output_layout_get(output_layout, m->wlr_output))
			wlr_output_layout_add_auto(output_layout,
						   m->wlr_output);
	}

	/* Now that we update the output layout we can get its box */
	wlr_output_layout_get_box(output_layout, NULL, &sgeom);

	wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

	/* Make sure the clients are hidden when dwl is locked */
	wlr_scene_node_set_position(&locked_bg->node, sgeom.x, sgeom.y);
	wlr_scene_rect_set_size(locked_bg, sgeom.width, sgeom.height);

	wl_list_for_each(m, &mons, link)
	{
		if (!m->wlr_output->enabled)
			continue;
		config_head = wlr_output_configuration_head_v1_create(
			config, m->wlr_output);

		/* Get the effective monitor geometry to use for surfaces */
		wlr_output_layout_get_box(output_layout, m->wlr_output, &m->m);
		m->w = m->m;
		wlr_scene_output_set_position(m->scene_output, m->m.x, m->m.y);

		wlr_scene_node_set_position(&m->fullscreen_bg->node, m->m.x,
					    m->m.y);
		wlr_scene_rect_set_size(m->fullscreen_bg, m->m.width,
					m->m.height);

		if (m->lock_surface) {
			struct wlr_scene_tree *scene_tree =
				m->lock_surface->surface->data;
			wlr_scene_node_set_position(&scene_tree->node, m->m.x,
						    m->m.y);
			wlr_session_lock_surface_v1_configure(
				m->lock_surface, m->m.width, m->m.height);
		}

		/* Calculate the effective monitor geometry to use for clients */
		arrangelayers(m);
		/* Don't move clients to the left output when plugging monitors */
		arrange(m);
		/* make sure fullscreen clients have the right size */
		if ((c = focustop(m)) && c->isfullscreen)
			resize(c, m->m, 0, 1);

		/* Try to re-set the gamma LUT when updating monitors,
		 * it's only really needed when enabling a disabled output, but meh. */
		m->gamma_lut_changed = 1;

		config_head->state.x = m->m.x;
		config_head->state.y = m->m.y;
	}

	if (selmon && selmon->wlr_output->enabled) {
		wl_list_for_each(c, &clients, link)
		{
			if (!c->mon && client_surface(c)->mapped)
				setmon(c, selmon, c->tags);
		}
		focusclient(focustop(selmon), 1);
		if (selmon->lock_surface) {
			client_notify_enter(selmon->lock_surface->surface,
					    wlr_seat_get_keyboard(seat));
			client_activate_surface(selmon->lock_surface->surface,
						1);
		}
	}

	/* FIXME: figure out why the cursor image is at 0,0 after turning all
	 * the monitors on.
	 * Move the cursor image where it used to be. It does not generate a
	 * wl_pointer.motion event for the clients, it's only the image what it's
	 * at the wrong position after all. */
	wlr_cursor_move(cursor, NULL, 0, 0);

	wlr_output_manager_v1_set_configuration(output_mgr, config);
}

void updatetitle(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_title);
	if (c == focustop(c->mon))
		printstatus();
}

void urgent(struct wl_listener *listener, void *data)
{
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	Client *c = NULL;
	toplevel_from_wlr_surface(event->surface, &c, NULL);
	if (!c || c == focustop(selmon))
		return;

	c->isurgent = 1;
	printstatus();

	if (client_surface(c)->mapped)
		client_set_border_color(c, urgentcolor);
}

void view(const Arg *arg)
{
	size_t i, tmptag;

	if (!selmon || (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
		return;
	selmon->seltags ^= 1; /* toggle sel tagset */
	if (arg->ui & TAGMASK) {
		selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
		selmon->pertag->prevtag = selmon->pertag->curtag;

		if (arg->ui == ~0)
			selmon->pertag->curtag = 0;
		else {
			for (i = 0; !(arg->ui & 1 << i); i++)
				;
			selmon->pertag->curtag = i + 1;
		}
	} else {
		tmptag = selmon->pertag->prevtag;
		selmon->pertag->prevtag = selmon->pertag->curtag;
		selmon->pertag->curtag = tmptag;
	}

	selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
	selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
	selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
	selmon->lt[selmon->sellt] =
		selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
	selmon->lt[selmon->sellt ^ 1] =
		selmon->pertag
			->ltidxs[selmon->pertag->curtag][selmon->sellt ^ 1];

	focusclient(focustop(selmon), 1);
	arrange(selmon);
	printstatus();
}

void virtualkeyboard(struct wl_listener *listener, void *data)
{
	struct wlr_virtual_keyboard_v1 *keyboard = data;
	/* Set the keymap to match the group keymap */
	wlr_keyboard_set_keymap(&keyboard->keyboard,
				vkb_group.wlr_group->keyboard.keymap);
	wlr_keyboard_set_repeat_info(&keyboard->keyboard, repeat_rate,
				     repeat_delay);

	/* Add the new keyboard to the group */
	wlr_keyboard_group_add_keyboard(vkb_group.wlr_group,
					&keyboard->keyboard);
}

void warpcursor(const Client *c)
{
	if (cursor->x < c->geom.x || cursor->x > c->geom.x + c->geom.width ||
	    cursor->y < c->geom.y || cursor->y > c->geom.y + c->geom.height)
		wlr_cursor_warp_closest(cursor, NULL,
					c->geom.x + c->geom.width / 2.0,
					c->geom.y + c->geom.height / 2.0);
}

Monitor *xytomon(double x, double y)
{
	struct wlr_output *o = wlr_output_layout_output_at(output_layout, x, y);
	return o ? o->data : NULL;
}

void xytonode(double x, double y, struct wlr_surface **psurface, Client **pc,
	      LayerSurface **pl, double *nx, double *ny)
{
	struct wlr_scene_node *node, *pnode;
	struct wlr_surface *surface = NULL;
	Client *c = NULL;
	LayerSurface *l = NULL;
	int layer;

	for (layer = NUM_LAYERS - 1; !surface && layer >= 0; layer--) {
		if (!(node = wlr_scene_node_at(&layers[layer]->node, x, y, nx,
					       ny)))
			continue;

		if (node->type == WLR_SCENE_NODE_BUFFER)
			surface = wlr_scene_surface_try_from_buffer(
					  wlr_scene_buffer_from_node(node))
					  ->surface;
		/* Walk the tree to find a node that knows the client */
		for (pnode = node; pnode && !c; pnode = &pnode->parent->node)
			c = pnode->data;
		if (c && c->type == LayerShell) {
			c = NULL;
			l = pnode->data;
		}
	}

	if (psurface)
		*psurface = surface;
	if (pc)
		*pc = c;
	if (pl)
		*pl = l;
}

void zoom(const Arg *arg)
{
	Client *c, *sel = focustop(selmon);

	if (!sel || !selmon || !selmon->lt[selmon->sellt]->arrange ||
	    sel->isfloating)
		return;

	/* Search for the first tiled window that is not sel, marking sel as
	 * NULL if we pass it along the way */
	wl_list_for_each(c, &clients, link)
	{
		if (VISIBLEON(c, selmon) && !c->isfloating) {
			if (c != sel)
				break;
			sel = NULL;
		}
	}

	/* Return if no other tiled window was found */
	if (&c->link == &clients)
		return;

	/* If we passed sel, move c to the front; otherwise, move sel to the
	 * front */
	if (!sel)
		sel = c;
	wl_list_remove(&sel->link);
	wl_list_insert(&clients, &sel->link);

	focusclient(sel, 1);
	arrange(selmon);
}

#ifdef XWAYLAND
void activatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, activate);

	/* Only "managed" windows can be activated */
	if (!client_is_unmanaged(c))
		wlr_xwayland_surface_activate(c->surface.xwayland, 1);
}

void associatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, associate);

	LISTEN(&client_surface(c)->events.map, &c->map, mapnotify);
	LISTEN(&client_surface(c)->events.unmap, &c->unmap, unmapnotify);
}

void configurex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, configure);
	struct wlr_xwayland_surface_configure_event *event = data;
	if (!c->mon)
		return;
	if (c->isfloating || client_is_unmanaged(c))
		resize(c,
		       (struct wlr_box){ .x = event->x,
					 .y = event->y,
					 .width = event->width,
					 .height = event->height },
		       0, 1);
	else
		arrange(c->mon);
}

void createnotifyx11(struct wl_listener *listener, void *data)
{
	struct wlr_xwayland_surface *xsurface = data;
	Client *c;

	/* Allocate a Client for this surface */
	c = xsurface->data = ecalloc(1, sizeof(*c));
	c->surface.xwayland = xsurface;
	c->type = X11;
	c->bw = borderpx;

	/* Listen to the various events it can emit */
	LISTEN(&xsurface->events.associate, &c->associate, associatex11);
	LISTEN(&xsurface->events.destroy, &c->destroy, destroynotify);
	LISTEN(&xsurface->events.dissociate, &c->dissociate, dissociatex11);
	LISTEN(&xsurface->events.request_activate, &c->activate, activatex11);
	LISTEN(&xsurface->events.request_configure, &c->configure,
	       configurex11);
	LISTEN(&xsurface->events.request_fullscreen, &c->fullscreen,
	       fullscreennotify);
	LISTEN(&xsurface->events.set_hints, &c->set_hints, sethints);
	LISTEN(&xsurface->events.set_title, &c->set_title, updatetitle);
}

void dissociatex11(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, dissociate);
	wl_list_remove(&c->map.link);
	wl_list_remove(&c->unmap.link);
}

xcb_atom_t getatom(xcb_connection_t *xc, const char *name)
{
	xcb_atom_t atom = 0;
	xcb_intern_atom_reply_t *reply;
	xcb_intern_atom_cookie_t cookie =
		xcb_intern_atom(xc, 0, strlen(name), name);
	if ((reply = xcb_intern_atom_reply(xc, cookie, NULL)))
		atom = reply->atom;
	free(reply);

	return atom;
}

void sethints(struct wl_listener *listener, void *data)
{
	Client *c = wl_container_of(listener, c, set_hints);
	struct wlr_surface *surface = client_surface(c);
	if (c == focustop(selmon))
		return;

	c->isurgent =
		xcb_icccm_wm_hints_get_urgency(c->surface.xwayland->hints);
	printstatus();

	if (c->isurgent && surface && surface->mapped)
		client_set_border_color(c, urgentcolor);
}

void xwaylandready(struct wl_listener *listener, void *data)
{
	struct wlr_xcursor *xcursor;
	xcb_connection_t *xc = xcb_connect(xwayland->display_name, NULL);
	int err = xcb_connection_has_error(xc);
	if (err) {
		fprintf(stderr,
			"xcb_connect to X server failed with code %d\n. Continuing with degraded functionality.\n",
			err);
		return;
	}

	/* Collect atoms we are interested in. If getatom returns 0, we will
	 * not detect that window type. */
	netatom[NetWMWindowTypeDialog] =
		getatom(xc, "_NET_WM_WINDOW_TYPE_DIALOG");
	netatom[NetWMWindowTypeSplash] =
		getatom(xc, "_NET_WM_WINDOW_TYPE_SPLASH");
	netatom[NetWMWindowTypeToolbar] =
		getatom(xc, "_NET_WM_WINDOW_TYPE_TOOLBAR");
	netatom[NetWMWindowTypeUtility] =
		getatom(xc, "_NET_WM_WINDOW_TYPE_UTILITY");

	/* assign the one and only seat */
	wlr_xwayland_set_seat(xwayland, seat);

	/* Set the default XWayland cursor to match the rest of dwl. */
	if ((xcursor =
		     wlr_xcursor_manager_get_xcursor(cursor_mgr, "default", 1)))
		wlr_xwayland_set_cursor(xwayland, xcursor->images[0]->buffer,
					xcursor->images[0]->width * 4,
					xcursor->images[0]->width,
					xcursor->images[0]->height,
					xcursor->images[0]->hotspot_x,
					xcursor->images[0]->hotspot_y);

	xcb_disconnect(xc);
}
#endif

int main(int argc, char *argv[])
{
	char *startup_cmd = NULL;
	int c;

	while ((c = getopt(argc, argv, "s:hdv")) != -1) {
		if (c == 's')
			startup_cmd = optarg;
		else if (c == 'd')
			log_level = WLR_DEBUG;
		else if (c == 'v')
			die("dwl " VERSION);
		else
			goto usage;
	}
	if (optind < argc)
		goto usage;

	/* Wayland requires XDG_RUNTIME_DIR for creating its communications socket */
	if (!getenv("XDG_RUNTIME_DIR"))
		die("XDG_RUNTIME_DIR must be set");
	setup();
	run(startup_cmd);
	cleanup();
	return EXIT_SUCCESS;

usage:
	die("Usage: %s [-v] [-d] [-s startup command]", argv[0]);
}
