#include <syslog.h>
#include <stdio.h>
struct hh {
  int *i;
  char buf[128];
  struct mutex m;
};

enum reset_t {
  /* Note: The order of these values are assumed to not change by the code using them. */
  reset_none = 0,
  reset_hard = 1, /* Reset everything to factory defaults. */
  reset_onvifhard =
    2, /* Reset everything except network parameters and device id. */
  reset_soft    = 3, /* Reset everything except network parameters. */
  reset_migrate = 4, /* Internal to signal migration of rwfs data. */
};

int
function(int *first, struct hh *second)
{
  float kk = 0;
  int same_line=0;

  void (*fp)(int, float) = NULL;
  if (1) {
    char hidden = 'a';
  }
  if (1) {
    const char *war = NULL;
    if (war) {
      int ff = (int)kk;
    }
    char c;
  } else if (1) {
    char el_if;
  } else if (1) {
    1 + 1;
  } else {
    char el;
  }

  if (1)
    1 + 1;

  return 0;
}

int
function2(void)
{
  return 0;
}

int
tt()
{
  int *bps, *blkalign, *const *bytespersec, *frame_size = &bps;
}

void
com_axis_Event_Consumer_event_async(sd_bus_message_handler_t callback, //TODO
                                    void *userdata)
{
  int i;
  int a                      = 0;
  int *p                     = NULL;
  type_t *t                  = (type_t)userdata;
  g_autofree gchar *key_base = NULL; //TODO
  g_autoptr(GFile) cfg_file  = NULL; //TODO
  GDateTime asd; //TODO
  GOptionContext *ctx; //TODO
  /* mode_t mask = 022; */

  /* TODO "build-in" enum types like GType */
}
int
func(void)
{
  ServiceRegistry *srv_reg = udata;
  gchar *file_path         = g_file_get_path(first);
  gchar *service_file      = g_file_get_basename(first);
}

void
func(const type_t *ptr, char a[128])
{
  to_string(ptr); //here to_string requires non const type_t! TODO cast!
}

int
wasd()
{
  int i, *j, *k = NULL, a = 0;
  /* char a[128] = {0}; */
  /* char temp[256]; */
  /* uint8_t *riff_extradata      = temp;*/
  /* uint8_t riff_extradata_start = temp; */
  /* int *uninit; */
}

enum AVColorPrimaries {
  AVCOL_PRI_SMPTE428     = 10, ///< SMPTE ST 428-1 (CIE 1931 XYZ)
  AVCOL_PRI_SMPTEST428_1 = AVCOL_PRI_SMPTE428, //TODO enum with same value

  AV_CODEC_ID_FIRST_AUDIO =
    0x10000, ///< A dummy id pointing at the start of audio codecs
  AV_CODEC_ID_PCM_S16LE = 0x10000, //TODO enum with same value
};

gboolean
led_2_camblock_i2c_setup(LedBase *self, GError **error)
{
  GError *err                  = NULL;
  GIOChannel *ioc              = NULL;
  GKeyFile *parameter_key_file = NULL;
  gchar *buf;
  gsize len;
  gint ret = TRUE;
  gchar **intensities;
  gchar **groups;
  gchar **angles;
  gint *values;
  gint nbr_of_leds;
  gsize nbr_of_angles;
  gsize nbr_of_intensities;
  int i, j;
  gsize length;
  Led_2_CAMBLOCK_I2C *led_2_camblock_i2c = LED_2_CAMBLOCK_I2C(self);
  const char *ir_control_enabled;
  gint ret_led = 0;
  guint16 val  = 0;

  if (1) {
  }
}

struct axis_mcu_uart_port {
  struct completion read_compl;
  int baudrate;
  unsigned char rx_buf[RCV_BUF_LEN];
  size_t rx_length;
  const void *clientdata;
};

typedef enum {
  G_PARAM_READABLE  = 1,
  G_PARAM_WRITABLE  = (1 << 1), //TODO
  G_PARAM_READWRITE = (G_PARAM_READABLE | G_PARAM_WRITABLE), // TODO can skip this one since it consist of two others
  /* G_PARAM_READWRITE_CONSTRUCT           = (G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT), // TODO recursive */
  /* // if G_PARAM_READWRITE: print(), elif G_PARAM_READABLE: print(), elif G_PARAM_WRITABLE: print() */
  /*  */
  G_PARAM_CONSTRUCT      = 1 << 2,
  G_PARAM_CONSTRUCT_ONLY = 1 << 3,
  G_PARAM_LAX_VALIDATION = 1 << 4,
  G_PARAM_STATIC_NAME    = 1 << 5,
  G_PARAM_STATIC_NICK    = 1 << 6,
  G_PARAM_STATIC_BLURB   = 1 << 7,
  /* #<{(| User defined flags go here |)}># */
  /* G_PARAM_EXPLICIT_NOTIFY     = 1 << 30, */
  /* #<{(| Avoid warning with -Wpedantic for gcc6 |)}># */
  G_PARAM_DEPRECATED = (gint)(1u << 31), //TODO
  G_PARAM_DUP =
    G_PARAM_CONSTRUCT, //TODO here we should combine: if G_PARAM_DEPRECATED: print(G_PARAM_DEPRECATED or G_PARAM_DUP)
} GParamFlags;

static void
wasd(void)
{
  if (1) {
    int aa;
  } else if (1)
    if (1) {
      int ab;
    }
  {
    if (1) {
      int bb;
    }
  }
}

static void
wasd(void)
{
  if (1) {
    int a;
    int a2;
  }

  if (1) {
    int ba;
  } else if (1) {
    int bb;
  }

  if (1) {
    int ca;
  } else if (1) {
    int cb;
  } else {
    int cc;
  }

  if (1)
    ;

  if (1) {
    int da;
  } else if (1)
    ;
  else if (1) {
    int db;
  }

  if (1) {
    int ea;
  } else if (1)
    ;

  if (1) {
    int fa;
  } else if (1)
    ;
  else {
    int fb;
  }

  if (1) {
    1 + 1;
  } else {
    2 + 2;
  }

  if (1) {
  }

  if (1)
    if (1) {
    } else {
    }

  if (1) {
    if (1) {
    } else if (1) {
    }
  }

  return;
}
