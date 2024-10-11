#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }

/* appearance */
static const int          sloppyfocus               = 1; /* focus follows mouse */
static const int          bypass_surface_visibility = 0; /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
             int          warp_cursor               = 1; /* toggle with alt+ctrl+w (mnemo warp), basically your mouse moves to the center of focus */
             int          monaxis                   = 1; /* toggle with alt+apostrophe, 1 means horizontally, 0 means vertically*/
static const int          smartborders              = 1;
static const unsigned int borderpx                  = 2; /* border pixel of windows */
static const float        bordercolor[]             = COLOR(0x7f7f7fff);
static const float        rootcolor[]               = COLOR(0x000000ff);
static const float        focuscolor[]              = COLOR(0xbb4f8eff);
static const float        urgentcolor[]             = COLOR(0xff0000ff);
/* To conform the xdg-protocol, set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[] = {0.1f, 0.1f, 0.1f, 1.0f};

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (9)

static int log_level = WLR_ERROR;

// floatpos 1 means regular left, 2 means middle and 3 means right below the status bar
static const Rule rules[] = {
  /* app_id     title                            tags_mask  floatpos   monitor */
    {"firefox",     "Firefox — Sharing Indicator",     0, 2, -1},
    {"LibreWolf",   "LibreWolf — Sharing Indicator",   0, 2, -1},
    {"imv",         "shotpreview-",                    0, 1, -1},
    {"foot",        "ocr-shotpreview-",                0, 2, -1},
    {"floatermid",  "",                                0, 2, -1},
    {"imv",         "qrshare",                         0, 2, -1}, // Imv can't disguise as other app but can switch its window name
    {"imv",         "shotpreview-",                    0, 3, -1},
    {"floateright", "",                                0, 3, -1},
};

/* layout(s) */
static const Layout layouts[] = {
  /* symbol     arrange function */
    {"[]=", tile   },
    {"><>", NULL   }, /* no layout function means floating behavior */
    {"[M]", monocle},
    {"||",  col },
};

/* monitors */
static const MonitorRule monrules[] = {
  /* name       mfact nmaster scale layout       rotate/reflect */
  /* example of a HiDPI laptop monitor:
  { "eDP-1",    0.5,  1,      2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL },
  */
  /* defaults */
    { NULL,      0.55f, 1, 1, &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL, -1, -1},
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
    /* can specify fields: rules, model, layout, variant, options */
    /* example:
    .options = "ctrl:nocaps",

    */
    .layout  = "plde",
    .model   = "pc104",
    .options = ""};
static const int repeat_rate  = 30;
static const int repeat_delay = 190;

/* Trackpad */
static const int tap_to_click            = 1;
static const int tap_and_drag            = 1;
static const int drag_lock               = 1;
static const int natural_scrolling       = 0;
static const int disable_while_typing    = 0;
static const int left_handed             = 0;
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
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double                             accel_speed   = 0.0;

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
/* helper shorthand for spawning processes */
#define SPAWN(...) {.v = (const char*[]){ __VA_ARGS__, NULL } }

/* Users with lower end specs might want to use footie instead of foot. This is a wrapper script
 * that activates foot's client - server capabilities reducing resource consumption with multiple
 * terminals and startup overhead. For now though this has some minor issues with dynamic theming. */
#define TERMINAL "foot"
/* commands */
static const char *termcmd[]      = {TERMINAL,    NULL};
static const char *fmsummon[]     = {TERMINAL, "-o", "environment.START_IN_LFCD=true", NULL};

/* shell commands macros to help with keys array readability */
#define MAIL " sh -c 'mailsync & ESCDELAY=0 neomutt'"
#define DWLB_TOGGLE "dwlb", "-toggle-visibility", "selected"
#define VOLUME_UP "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+; kill -44 $(pidof someblocks)"
#define VOLUME_DOWN "wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-; kill -44 $(pidof someblocks)"
#define VOLUME_MUTE_TOGGLE "wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle; kill -44 $(pidof someblocks)"
#define CAPTURE_MUTE_TOGGLE "amixer","sset","Capture","toggle"
// toggling between having the system sleep after 4 minutes. Corresponds to sb-lock
#define LOCK_TOGGLE "kill -45 $(pidof someblocks)"

static const Key keys[] = {
  /* Note that Shift changes certain key codes: c -> C, 2 -> at, etc. */
  /* modifier                          key                            function          argument */
     /* Special, terminal spawn must remain first to get spawned in the same directory */
    {MODKEY,                           XKB_KEY_space,                 spawn,            {.v = termcmd}                    },
    /* window management */
    {MODKEY,                           XKB_KEY_j,                     focusstack,       {.i = +1}                         },
    {MODKEY,                           XKB_KEY_k,                     focusstack,       {.i = -1}                         },
    {MODKEY,                           XKB_KEY_i,                     incnmaster,       {.i = +1}                         },
    {MODKEY,                           XKB_KEY_d,                     incnmaster,       {.i = -1}                         },
    {MODKEY,                           XKB_KEY_h,                     setmfact,         {.f = -0.05f}                     },
    {MODKEY,                           XKB_KEY_l,                     setmfact,         {.f = +0.05f}                     },
    {MODKEY,                           XKB_KEY_Return,                zoom,             {0}                               },
    {MODKEY,                           XKB_KEY_s,                     setlayout,        {0}                               },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_C,                     killclient,       {0}                               },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_space,                 togglefloating,   {0}                               },
    {MODKEY,                           XKB_KEY_apostrophe,            monaxischange,    {0} /* ↓ don't change these 4 */  },
    {MODKEY,                           XKB_KEY_comma,                 focusmon,         {.i = WLR_DIRECTION_LEFT}         },
    {MODKEY,                           XKB_KEY_period,                focusmon,         {.i = WLR_DIRECTION_RIGHT}        },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_less,                  tagmon,           {.i = WLR_DIRECTION_LEFT}         },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_greater,               tagmon,           {.i = WLR_DIRECTION_RIGHT}        },
    {MODKEY,                           XKB_KEY_Tab,                   view,             {0}                               },
    {MODKEY,                           XKB_KEY_f,                     togglefullscreen, {0}                               },
    {MODKEY,                           XKB_KEY_0,                     view,             {.ui = ~0}                        },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_parenright,            tag,              {.ui = ~0}                        },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_Q,                     quit,             {0}                               },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_Q,                     spawn,            SHCMD("killall someblocks")       },

    /* app launch shortcuts */
    {MODKEY,                           XKB_KEY_o,                     spawn,            {.v = fmsummon}                   },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_Return,                spawn,            SPAWN("launcher")                 },
    {MODKEY,                           XKB_KEY_q,                     spawn,            SPAWN("quickact")                 },
    {MODKEY,                           XKB_KEY_w,                     spawn,            SPAWN("firefox")                  },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_W,                     spawn,            SPAWN("chromium")                 },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_M,                     spawn,            SPAWN("supersonic")               },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_P,                     spawn,            SPAWN("passmenu")                 },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_B,                     spawn,            SPAWN("bookmarkselect")           },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_L,                     spawn,            SPAWN("quicktrans")               },
    {0,                                XKB_KEY_XF86Mail,              spawn,            SHCMD(TERMINAL MAIL)              },

    /* very useful misc */
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_T,                     spawn,            SPAWN("themeset", "-o")           },
    {MODKEY,                           XKB_KEY_b,                     spawn,            SPAWN(DWLB_TOGGLE)                },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_S,                     spawn,            SPAWN("sus")                      },
    {MODKEY | WLR_MODIFIER_CTRL,       XKB_KEY_s,                     spawn,            SHCMD(LOCK_TOGGLE) },
    {MODKEY | WLR_MODIFIER_CTRL,       XKB_KEY_w,                     togglewarpcursor, {0}                               },
    {MODKEY,                           XKB_KEY_F5,                    spawn,            SPAWN("powertoggle")              },

     /* layouts */
    {MODKEY,                           XKB_KEY_t,                     setlayout,        {.v = &layouts[0]}                },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_F,                     setlayout,        {.v = &layouts[1]}                },
    {MODKEY,                           XKB_KEY_m,                     setlayout,        {.v = &layouts[2]}                },
    {MODKEY,                           XKB_KEY_backslash,             setlayout,        {.v = &layouts[3]}                },

    /* media */
    {0,                                XKB_KEY_XF86MonBrightnessUp,   spawn,            SPAWN("brightnessctl","set","5%+")},
    {0,                                XKB_KEY_XF86MonBrightnessDown, spawn,            SPAWN("brightnessctl","set","5%-")},
    {0,                                XKB_KEY_XF86AudioRaiseVolume,  spawn,            SHCMD(VOLUME_UP)                  },
    {0,                                XKB_KEY_XF86AudioLowerVolume,  spawn,            SHCMD(VOLUME_DOWN)                },
    {0,                                XKB_KEY_XF86AudioMute,         spawn,            SHCMD(VOLUME_MUTE_TOGGLE)         },
    {WLR_MODIFIER_SHIFT,               XKB_KEY_XF86AudioMute,         spawn,            SPAWN("sinkchoose")               },
    {0,                                XKB_KEY_XF86AudioMicMute,      spawn,            SPAWN(CAPTURE_MUTE_TOGGLE)               },
    {0,                                XKB_KEY_Print,                 spawn,            SPAWN("screenshott")              },
    {WLR_MODIFIER_SHIFT,               XKB_KEY_Print,                 spawn,            SPAWN("screensave")               },
    {MODKEY,                           XKB_KEY_Print,                 spawn,            SPAWN("showshot")                 },
    // use an OCR and show chars from print screen
    {MODKEY | WLR_MODIFIER_CTRL,       XKB_KEY_Print,                 spawn,            SPAWN("showshot", "-c")           },
    // show screenshot directory
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_Print,                 spawn,            SPAWN("showshot", "-d")           },
    {WLR_MODIFIER_CTRL,                XKB_KEY_Print,                 spawn,            SPAWN("screenrecord")             },
    {MODKEY,                           XKB_KEY_F12,                   spawn,            SPAWN("clipsync")                 },
    {MODKEY | WLR_MODIFIER_SHIFT,      XKB_KEY_F12,                   spawn,            SPAWN("clipsync", "-f")           },

    /* move floating windows with a mouse */
    {MODKEY,                           XKB_KEY_Up,                    floathmovebykey,  {.i = -80}                        },
    {MODKEY,                           XKB_KEY_Down,                  floathmovebykey,  {.i = +80}                        },
    {MODKEY,                           XKB_KEY_Left,                  floatvmovebykey,  {.i = -80}                        },
    {MODKEY,                           XKB_KEY_Right,                 floatvmovebykey,  {.i = +80}                        },

    /* tags */
    TAGKEYS(XKB_KEY_1, XKB_KEY_exclam, 0),
    TAGKEYS(XKB_KEY_2, XKB_KEY_at, 1),
    TAGKEYS(XKB_KEY_3, XKB_KEY_numbersign, 2),
    TAGKEYS(XKB_KEY_4, XKB_KEY_dollar, 3),
    TAGKEYS(XKB_KEY_5, XKB_KEY_percent, 4),
    TAGKEYS(XKB_KEY_6, XKB_KEY_asciicircum, 5),
    TAGKEYS(XKB_KEY_7, XKB_KEY_ampersand, 6),
    TAGKEYS(XKB_KEY_8, XKB_KEY_asterisk, 7),
    TAGKEYS(XKB_KEY_9, XKB_KEY_parenleft, 8),

    /* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server    */
    {WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT,  XKB_KEY_Terminate_Server,      quit,             {0}                                              },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
    CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6), CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
    {MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove}                                  },
    {MODKEY, BTN_MIDDLE, togglefloating, {0}                                              },
    {MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize}                                },
    {MODKEY, BTN_SIDE,   spawn,          {.v = (const char *[]){"quicktrans", "-c", NULL}}},
};

