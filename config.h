#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }
/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const bool cursor_warp = true;
static const int smartborders              = 1;
static const unsigned int borderpx         = 2;  /* border pixel of windows */
static const float bordercolor[]           = COLOR(0x7f7f7fff);
static const float focuscolor[]            = COLOR(0xbb4f8eff);
static const float urgentcolor[]           = COLOR(0xff0000ff);
/* To conform the xdg-protocol, set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]  = {0.1, 0.1, 0.1, 1.0};

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (9)

static int log_level = WLR_ERROR;

// floatpos 1 means regular left, 2 means middle and 3 means right below the status bar
static const Rule rules[] = {
	/* app_id     title                            tags_mask  floatpos   monitor */
	{ "firefox",    "Firefox — Sharing Indicator",    0,         2,	    -1 },
	{ "LibreWolf",  "LibreWolf — Sharing Indicator",  0,         2,	    -1 },
	{ "imv",        "shotpreview-",                   0,         1,       -1 },
	{ "foot",       "ocr-shotpreview-",               0,         2,       -1 },
	{ "floatermid", "",                               0,         2,       -1 },
	{ "imv",        "qrshare",                        0,         2,       -1 }, // Imv can't disguise as other app but can switch its window name
	{ "imv",        "shotpreview-",                   0,         3,       -1 },
	{ "floateright","",                               0,         3,       -1 },
	/*{ "firefox",  NULL,       1 << 8,       0,           -1 },*/
};


/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
	{ "||",       column },
};

/* monitors */
static const MonitorRule monrules[] = {
	/* name       mfact nmaster scale layout       rotate/reflect */
	/* example of a HiDPI laptop monitor:
	{ "eDP-1",    0.5,  1,      2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL },
	*/
	/* defaults */
	{ "eDP-1",      0.50, 1,      1   ,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,  0,  0},
	{ "DP-2",      0.50, 1,      1   ,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,  0,  0},
	{ "HDMI-A-1",   0.55, 1,      1,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   0,  0},
	{ "DP-1",       0.55, 1,      1,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   0,  0},
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",

	*/
  .layout = "plde, pl",
  .model = "pc104",
  .options = "grp:alt_shift_toggle"
};
static const int repeat_rate = 30;
static const int repeat_delay = 190;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 0;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE       
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS       
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER 
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
static const double accel_speed = 0.0;


static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_LOGO

#define TAGKEYS(KEY,SKEY,TAG) \
	{ MODKEY,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,            toggleview,      {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SKEY,           tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT,SKEY,toggletag, {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[]  = { "footie",   NULL };
static const char *menucmd[] = { "launcher", NULL };
static const char *quickcmd[] = { "quickact", NULL };
//static const char *fmsummon[] = { "footie", "zsh", "-c", "'lfrun", "/", ";", "exec", "zsh'", NULL };
static const char *firesummon[]  = { "librewolf", NULL };
static const char *chromesummon[]  = { "chromium", NULL };
static const char *passsummon[]  = { "passmenu", NULL };

/* other commands are directly defined in the keys */




static const Key keys[] = {
	/* Note that Shift changes certain key codes: c -> C, 2 -> at, etc. */
	/* modifier                  key                 function        argument */
	{ MODKEY,                    XKB_KEY_space,          spawn,      {.v = menucmd} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Return,     spawn,          {.v = termcmd} },
	{ MODKEY,                    XKB_KEY_q,     	 spawn,          {.v = quickcmd} },
	{ MODKEY,                    XKB_KEY_w,          spawn,          {.v = firesummon } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_W,          spawn,          {.v = chromesummon } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_M,          spawn,          {.v = (const char*[]){ "footie", "muzyka", NULL } }},
	{ MODKEY,                    XKB_KEY_o,          spawn,          SHCMD("footie zsh -ci 'lfcd; exec zsh'") },
	{ MODKEY,                    XKB_KEY_j,          focusstack,     {.i = +1} },
	{ MODKEY,                    XKB_KEY_k,          focusstack,     {.i = -1} },
	{ MODKEY,                    XKB_KEY_i,          incnmaster,     {.i = +1} },
	{ MODKEY,                    XKB_KEY_d,          incnmaster,     {.i = -1} },
	{ MODKEY,                    XKB_KEY_h,          setmfact,       {.f = -0.05} },
	{ MODKEY,                    XKB_KEY_l,          setmfact,       {.f = +0.05} },
	{ MODKEY,                    XKB_KEY_Return,     zoom,           {0} },
	{ MODKEY,                    XKB_KEY_Tab,        view,           {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_C,          killclient,     {0} },
	{ MODKEY,                    XKB_KEY_t,          setlayout,      {.v = &layouts[0]} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_F,          setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                    XKB_KEY_m,          setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                    XKB_KEY_backslash,          setlayout,      {.v = &layouts[3]} },
	{ MODKEY,                    XKB_KEY_s,      setlayout,      {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_space,      togglefloating, {0} },
	{ MODKEY,                    XKB_KEY_Up,          floathmovebykey,           {.i = -80} },
	{ MODKEY,                    XKB_KEY_Down,          floathmovebykey,           {.i = +80} },
	{ MODKEY,                    XKB_KEY_Left,          floatvmovebykey,           {.i = -80} },
	{ MODKEY,                    XKB_KEY_Right,          floatvmovebykey,           {.i = +80} },
	{ MODKEY,                    XKB_KEY_f,         togglefullscreen, {0} },
	{ MODKEY,                    XKB_KEY_0,          view,           {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenright, tag,            {.ui = ~0} },
  /* Replaced LEFT and RIGHT with UP and DOWN for moving to the other screen when display is above*/
	{ MODKEY,                    XKB_KEY_comma,      focusmon,       {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY,                    XKB_KEY_period,     focusmon,       {.i = WLR_DIRECTION_UP} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_less,       tagmon,         {.i = WLR_DIRECTION_DOWN} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_greater,    tagmon,         {.i = WLR_DIRECTION_UP} },
	{ 0,                    XKB_KEY_XF86Mail,       spawn,         SHCMD("footie sh -c 'mailsync & ESCDELAY=0 neomutt'") },
	{ 0,       XKB_KEY_XF86MonBrightnessUp, spawn,   SHCMD("brightnessctl set 5%+") },
	{ 0,       XKB_KEY_XF86MonBrightnessDown, spawn, SHCMD("brightnessctl set 5%-")},
	{ 0,       XKB_KEY_XF86AudioRaiseVolume, spawn,   SHCMD("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+; kill -44 $(pidof someblocks)")},
	{ 0,       XKB_KEY_XF86AudioLowerVolume, spawn,   SHCMD("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-; kill -44 $(pidof someblocks)")},
 	{ 0,       XKB_KEY_XF86AudioMute,        spawn,   SHCMD("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle; kill -44 $(pidof someblocks)")}, 
	{ 0,       XKB_KEY_Print,      spawn,                 {.v = (const char*[]){ "screenshott", NULL } } },
	{ 0,       XKB_KEY_XF86Launch9,      spawn,                 {.v = (const char*[]){ "screenrecord", NULL } } }, // fn+print_screen if present on keyboard
	{ WLR_MODIFIER_CTRL,       XKB_KEY_F12,      spawn,                 {.v = (const char*[]){ "screensend", NULL } } },
	{ MODKEY,       XKB_KEY_Print,      spawn,               {.v = (const char*[]){ "showshot", NULL } } },
	{ MODKEY|WLR_MODIFIER_CTRL,   XKB_KEY_Print,  spawn,        {.v = (const char*[]){ "showshot", "-c", NULL } } }, // show chars in print screen
	{ MODKEY|WLR_MODIFIER_SHIFT,   XKB_KEY_Print,  spawn,        {.v = (const char*[]){ "showshot", "-d", NULL } } }, // show screenshot directory
	{ WLR_MODIFIER_SHIFT,    XKB_KEY_Print,      spawn,      {.v = (const char*[]){ "screensave", NULL } } }, // save screenshow
	{ MODKEY,    XKB_KEY_F12,      spawn,                 {.v = (const char*[]) {"dswitch", NULL } } },
	{ MODKEY|WLR_MODIFIER_SHIFT,    XKB_KEY_B,      spawn,      {.v = (const char*[]){ "bookmarkselect", NULL } } },
	{ MODKEY|WLR_MODIFIER_SHIFT,    XKB_KEY_T,      spawn,      {.v = (const char*[]){ "themeset", "-o", NULL } } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_L,     	 spawn,          {.v = (const char*[]){ "quicktrans", NULL } } },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q,          quit,           {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q,          spawn,           SHCMD("killall someblocks") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_S,          spawn,           {.v = (const char*[]) {"sus", NULL } } },
	  /*Z as in keyboardzzz or zaza also very easily pressable so that you can connect to the keyboard quickly */
	{ MODKEY,         XKB_KEY_z,          spawn,           SHCMD("keyboard-connect") },
	{ MODKEY|WLR_MODIFIER_SHIFT|WLR_MODIFIER_CTRL, XKB_KEY_S,    spawn,    SHCMD("kill -45 $(pidof someblocks)") },
	{ MODKEY,         XKB_KEY_b,    spawn,    {.v = (const char*[]) {"dwlb", "-toggle-visibility", "selected", NULL } }},
	{ WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT,         XKB_KEY_space,      spawn,       SHCMD("kill -45 $(pidof someblocks)")},
	{ MODKEY|WLR_MODIFIER_SHIFT,         XKB_KEY_P,      spawn,       {.v = passsummon }},
	TAGKEYS(          XKB_KEY_1, XKB_KEY_exclam,                     0),
	TAGKEYS(          XKB_KEY_2, XKB_KEY_at,                         1),
	TAGKEYS(          XKB_KEY_3, XKB_KEY_numbersign,                 2),
	TAGKEYS(          XKB_KEY_4, XKB_KEY_dollar,                     3),
	TAGKEYS(          XKB_KEY_5, XKB_KEY_percent,                    4),
	TAGKEYS(          XKB_KEY_6, XKB_KEY_asciicircum,                5),
	TAGKEYS(          XKB_KEY_7, XKB_KEY_ampersand,                  6),
	TAGKEYS(          XKB_KEY_8, XKB_KEY_asterisk,                   7),
	TAGKEYS(          XKB_KEY_9, XKB_KEY_parenleft,                  8),

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server    */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_Terminate_Server, quit, {0} },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
	{ MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
	{ MODKEY, BTN_SIDE,  spawn,     {.v = (const char*[]){ "quicktrans", "-c", NULL } } },
};

