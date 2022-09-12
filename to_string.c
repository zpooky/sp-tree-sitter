#include "to_string.h"

#include <assert.h>
#include <string.h>

#include "sp_str.h"

static void
__format_numeric(struct arg_list *result,
                 const char *pprefix,
                 const char *format)
{
  char buffer[64] = {'\0'};
  if (result->pointer) {
    sp_str buf_tmp;

    sprintf(buffer, "%s%s", format, "%s");
    result->format = strdup(buffer);

    sp_str_init(&buf_tmp, 0);
    sp_str_appends(&buf_tmp, pprefix, result->variable, " ? *", pprefix,
                   result->variable, " : 0, ", NULL);
    sp_str_appends(&buf_tmp, pprefix, result->variable, " ? \"\" : \"(NULL)\"",
                   NULL);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else {
    result->format = strdup(format);
  }
}

static bool
__format_libxml2(struct sp_ts_Context *ctx,
                 struct arg_list *result,
                 const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "xmlNode") == 0 ||
      strcmp(result->type, "xmlNodePtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlNodePtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->line", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".line", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlNotation") == 0 ||
      strcmp(result->type, "xmlNotationPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlNodePtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->line", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".line", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlAtrr") == 0 ||
      strcmp(result->type, "xmlAttrPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer || strcmp(result->type, "xmlAttrPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlAttribute") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlDoc") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlElement") == 0 ||
      strcmp(result->type, "xmlElementPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]";
    if (result->pointer || strcmp(result->type, "xmlElementPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlEntity") == 0 ||
      strcmp(result->type, "xmlEntityPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]URI[%s]";
    if (result->pointer || strcmp(result->type, "xmlEntityPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->URI", " : \"(NULL)\", ",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".URI, ", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlDoc") == 0 ||
      strcmp(result->type, "xmlDocPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "URL[%s]";
    if (result->pointer || strcmp(result->type, "xmlDocPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->URL", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".URL", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlID") == 0 ||
      strcmp(result->type, "xmlIDPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlIDPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->lineno", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".lineno", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  if (strcmp(result->type, "xmlRef") == 0 ||
      strcmp(result->type, "xmlRefPtr") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "name[%s]line[%u]";
    if (result->pointer || strcmp(result->type, "xmlRefPtr") == 0) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", " : \"(NULL)\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->lineno", "1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".lineno", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
    return true;
  }

  return false;
}

static bool
__format_alsa(struct sp_ts_Context *ctx,
              struct arg_list *result,
              const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "snd_ctl_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? snd_ctl_name(",
                     pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_name(&", pprefix, result->variable, ")",
                     NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_ctl_event_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s:%u,%u";

    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_event_elem_get_name(", pprefix,
                     result->variable, ")", " : \"(NULL)\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_event_elem_get_device(", pprefix,
                     result->variable, ")", " : 1337, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_event_elem_get_subdevice(", pprefix,
                     result->variable, ")", " : 1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_name(&", pprefix,
                     result->variable, "), ", NULL);
      sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_device(&", pprefix,
                     result->variable, "), ", NULL);
      sp_str_appends(&buf_tmp, "snd_ctl_event_elem_get_subdevice(&", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);

  } else if (strcmp(result->type, "snd_ctl_card_info_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_card_info_get_name(", pprefix,
                     result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_card_info_get_name(&", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);

  } else if (strcmp(result->type, "snd_ctl_elem_type_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_elem_type_name(*", pprefix, result->variable,
                     ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_elem_type_name(", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_ctl_elem_value_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_elem_value_get_name(", pprefix,
                     result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_elem_value_get_name(&", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);

  } else if (strcmp(result->type, "snd_ctl_event_type_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_event_type_name(", pprefix, result->variable,
                     ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_event_type_name(&", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);

  } else if (strcmp(result->type, "snd_ctl_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? snd_ctl_name(",
                     pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_name(&", pprefix, result->variable, ")",
                     NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);

  } else if (strcmp(result->type, "snd_ctl_elem_id_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? snd_ctl_elem_id_get_name(", pprefix, result->variable,
                     ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "snd_ctl_elem_id_get_name(&", pprefix,
                     result->variable, ")", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_mixer_t") == 0) {
    result->format = "%p";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    sp_str_append(&buf_tmp, "(const void*)");
    if (result->pointer) {
    } else {
      sp_str_append(&buf_tmp, "&");
    }
    sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_pcm") == 0) {
    //kernel
#if 0
struct snd_pcm {
	struct snd_card *card;
	struct list_head list;
	int device; /* device number */
	unsigned int info_flags;
	unsigned short dev_class;
	unsigned short dev_subclass;
	char id[64];
	char name[80];
	struct snd_pcm_str streams[2];
	struct mutex open_mutex;
	wait_queue_head_t open_wait;
	void *private_data;
	void (*private_free) (struct snd_pcm *pcm);
	bool internal; /* pcm is for internal use only */
	bool nonatomic; /* whole PCM operations are in non-atomic context */
};
#endif
    result->format = "%p:dev[%d]id[%s]name[%s]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, ", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->device : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : \"\"", NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_pcm_substream") == 0) {
#if 0
struct snd_pcm_substream {
	struct snd_pcm *pcm;
	struct snd_pcm_str *pstr;
	void *private_data;		/* copied from pcm->private_data */
	int number;
	char name[32];			/* substream name */
	int stream;			/* stream (direction) */
};
#endif
    result->format = "pcm[%p]number[%d]name[%s]stream[%d]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->number : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->stream : 1337",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_dai") == 0) {
#if 0
struct snd_soc_dai {
	const char *name;
	int id;
	struct device *dev;
};
#endif
    result->format = "name[%s]id[%d]dev[%p]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL", NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_pcm_runtime") == 0) {
#if 0
#if 0
struct snd_pcm_runtime {
	struct snd_pcm_substream *trigger_master;
	struct timespec trigger_tstamp;	/* trigger timestamp */
	bool trigger_tstamp_latched;     /* trigger timestamp latched in low-level driver/hardware */
	int overrange;
	snd_pcm_uframes_t avail_max;
	snd_pcm_uframes_t hw_ptr_base;	/* Position at buffer restart */
	snd_pcm_uframes_t hw_ptr_interrupt; /* Position at interrupt time */
	unsigned long hw_ptr_jiffies;	/* Time when hw_ptr is updated */
	unsigned long hw_ptr_buffer_jiffies; /* buffer time in jiffies */
	snd_pcm_sframes_t delay;	/* extra delay; typically FIFO size */
	u64 hw_ptr_wrap;                /* offset for hw_ptr due to boundary wrap-around */
	/* -- HW params -- */
	snd_pcm_access_t access;	/* access mode */
	snd_pcm_format_t format;	/* SNDRV_PCM_FORMAT_* */
	snd_pcm_subformat_t subformat;	/* subformat */
	unsigned int rate;		/* rate in Hz */
	unsigned int channels;		/* channels */
	snd_pcm_uframes_t period_size;	/* period size */
	unsigned int periods;		/* periods */
	snd_pcm_uframes_t buffer_size;	/* buffer size */
	snd_pcm_uframes_t min_align;	/* Min alignment for the format */
	size_t byte_align
};
#endif
      result->format = "name[%s]id[%d]dev[%p]";
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                       NULL);

        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
        sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL",
                       NULL);
      } else {
        assert(false);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
#endif
    /* TODO */
  } else if (strcmp(result->type, "snd_pcm_hardware") == 0) {
#if 0
struct snd_pcm_hardware {
  unsigned int info;		/* SNDRV_PCM_INFO_* */
  u64 formats;			/* SNDRV_PCM_FMTBIT_* */
  unsigned int rates;		/* SNDRV_PCM_RATE_* */
  unsigned int rate_min;		/* min rate */
  unsigned int rate_max;		/* max rate */
  unsigned int channels_min;	/* min channels */
  unsigned int channels_max;	/* max channels */
  size_t buffer_bytes_max;	/* max buffer size */
  size_t period_bytes_min;	/* min period size */
  size_t period_bytes_max;	/* max period size */
  unsigned int periods_min;	/* min # of periods */
  unsigned int periods_max;	/* max # of periods */
  size_t fifo_size;		/* fifo size in bytes */
};
#endif
    result->format =
      "info[%u]formats[%llu]rates[%u]rate_min[%u]rate_max[%u]channels_min[%u]"
      "channels_max[%u]buffer_bytes_max[%zu]period_bytes_min[%zu]period_"
      "bytes_max[%zu]periods_min[%u]periods_max[%u]fifo_size[%zu]}";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->info : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->formats : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->rates : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->rate_min : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->rate_max : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->channels_min : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->channels_max : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->buffer_bytes_max : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->period_bytes_min : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->period_bytes_max : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->periods_min : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->periods_max : 1337, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->fifo_size : 1337",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_pcm_ops") == 0) {
#if 0
struct snd_pcm_ops {
  int (*open)(struct snd_pcm_substream *substream);
  int (*close)(struct snd_pcm_substream *substream);
  int (*ioctl)(struct snd_pcm_substream * substream, unsigned int cmd, void *arg);
  int (*hw_params)(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params);
  int (*hw_free)(struct snd_pcm_substream *substream);
  int (*prepare)(struct snd_pcm_substream *substream);
  int (*trigger)(struct snd_pcm_substream *substream, int cmd);
  snd_pcm_uframes_t (*pointer)(struct snd_pcm_substream *substream);
  int (*get_time_info)(struct snd_pcm_substream *substream, struct timespec *system_ts, struct timespec *audio_ts, struct snd_pcm_audio_tstamp_config *audio_tstamp_config, struct snd_pcm_audio_tstamp_report *audio_tstamp_report);
  int (*copy)(struct snd_pcm_substream *substream, int channel, snd_pcm_uframes_t pos, void __user *buf, snd_pcm_uframes_t count);
  int (*silence)(struct snd_pcm_substream *substream, int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t count); struct page *(*page)(struct snd_pcm_substream *substream, unsigned long offset);
  int (*mmap)(struct snd_pcm_substream *substream, struct vm_area_struct *vma);
  int (*ack)(struct snd_pcm_substream *substream);
};
#endif
    result->format = "open[%pF]close[%pF]ioctl[%pF]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->open : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->close : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->ioctl : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_pcm_ops") == 0) {
#if 0
struct snd_soc_card {
	const char *name;
	const char *long_name;
	const char *driver_name;
	struct device *dev;
	struct snd_card *snd_card;
	struct module *owner;
	struct mutex mutex;
	struct mutex dapm_mutex;
	bool instantiated;
	int (*probe)(struct snd_soc_card *card);
	int (*late_probe)(struct snd_soc_card *card);
	int (*remove)(struct snd_soc_card *card);
};
#endif
    result->format = "name[%s]long_name[%s]driver_name[%s]dev[%p]snd_card[%p]"
                     "probe[%pF]late_probe[%pF]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->long_name : \"\", ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->driver_name : \"\", ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->snd_card : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->late_probe : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_card") == 0) {
#if 0
struct snd_card {
  int number;			/* number of soundcard (index to snd_cards) */
  char id[16];			/* id string of this card */
  char driver[16];		/* driver name */
  char shortname[32];		/* short name of this soundcard */
  char longname[80];		/* name of this soundcard */
  char irq_descr[32];		/* Interrupt description */
  char mixername[80];		/* mixer name */
  char components[128];		/* card components delimited with space */
  struct module *module;		/* top-level module */
  void *private_data;		/* private data for soundcard */
  void (*private_free) (struct snd_card *card); /* callback for freeing of private data */
  struct list_head devices;	/* devices */
  struct device ctl_dev;		/* control device */
  unsigned int last_numid;	/* last used numeric ID */
  struct rw_semaphore controls_rwsem;	/* controls list lock */
  rwlock_t ctl_files_rwlock;	/* ctl_files list lock */
  int controls_count;		/* count of all controls */
  int user_ctl_count;		/* count of all user controls */
  struct list_head controls;	/* all controls for this card */
  struct list_head ctl_files;	/* active control files */
  struct mutex user_ctl_lock;	/* protects user controls against concurrent access */
  struct snd_info_entry *proc_root;	/* root for soundcard specific files */
  struct snd_info_entry *proc_id;	/* the card id */
  struct proc_dir_entry *proc_root_link;	/* number link to real id */
  struct list_head files_list;	/* all files associated to this card */
  struct snd_shutdown_f_ops *s_f_ops; /* file operations in the shutdown state */
  spinlock_t files_lock;		/* lock the files for this card */
  int shutdown;			/* this card is going down */
  struct completion *release_completion;
  struct device *dev;		/* device assigned to this card */
  struct device card_dev;		/* cardX object for sysfs */
};
#endif
    result->format = "number[%d]id[%s]driver[%s]shortname[%s]longname[%s]irq_"
                     "descr[%s]mixername[%s]components[%s]dev[%p]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->number : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->driver : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->shortname : \"\", ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->longname : \"\", ",
                     NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->irq_descr : \"\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->mixername : \"\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->components : \"\", ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL", NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_component") == 0) {
#if 0
struct snd_soc_component {
  const char *name;
  int id;
  const char *name_prefix;
  struct device *dev;
  struct snd_soc_card *card;
  unsigned int active;
  unsigned int ignore_pmdown_time:1; /* pmdown_time is ignored at stop */
  unsigned int registered_as_component:1;
  unsigned int auxiliary:1; /* for auxiliary component of the card */
  unsigned int suspended:1; /* is in suspend PM state */
  struct list_head list;
  struct list_head card_aux_list; /* for auxiliary bound components */
  struct list_head card_list;
  struct snd_soc_dai_driver *dai_drv;
  int num_dai;
  const struct snd_soc_component_driver *driver;
  struct list_head dai_list;
  int (*read)(struct snd_soc_component *, unsigned int, unsigned int *);
  int (*write)(struct snd_soc_component *, unsigned int, unsigned int);
};
#endif
    result->format =
      "name[%s]id[%d]name_prefix[%s]dev[%p]card[%p]read[%pF]write[%pF]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->id : 1337, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->name_prefix : \"\", ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->card : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->read : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->write : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_platform") == 0) {
#if 0
struct snd_soc_platform {
  struct device *dev;
  const struct snd_soc_platform_driver *driver;
  struct list_head list;
  struct snd_soc_component component;
};
#endif
    result->format = "dev[%p]driver[%p]component[%p]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->driver : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? &", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->component : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_platform_driver") == 0) {
#if 0
struct snd_soc_platform_driver {
  int (*probe)(struct snd_soc_platform *);
  int (*remove)(struct snd_soc_platform *);
  struct snd_soc_component_driver component_driver;
  /* pcm creation and destruction */
  int (*pcm_new)(struct snd_soc_pcm_runtime *);
  void (*pcm_free)(struct snd_pcm *);
};
#endif
    result->format =
      "probe[%pF]remove[%pF]component_driver[%pF]pcm_new[%pF]pcm_free[%pF]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->remove : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? &", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     "->component_driver : NULL, ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm_new : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm_free : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_component_driver") == 0) {
#if 0
struct snd_soc_component_driver {
  const char *name;
  const struct snd_kcontrol_new *controls;
  unsigned int num_controls;
  const struct snd_soc_dapm_widget *dapm_widgets;
  unsigned int num_dapm_widgets;
  const struct snd_soc_dapm_route *dapm_routes;
  unsigned int num_dapm_routes;
  int (*probe)(struct snd_soc_component *);
  void (*remove)(struct snd_soc_component *)
};
#endif
    result->format = "name[%s]probe[%pF]remove[%pF]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->name : \"\", ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->probe : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->remove : NULL",
                     NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "snd_soc_pcm_runtime") == 0) {
#if 0
struct snd_soc_pcm_runtime {
  struct device *dev;
  struct snd_soc_card *card;
  struct snd_soc_dai_link *dai_link;
  struct mutex pcm_mutex;
  enum snd_soc_pcm_subclass pcm_subclass;
  struct snd_pcm_ops ops;
  unsigned int dev_registered:1;
  /* Dynamic PCM BE runtime data */
  struct snd_soc_dpcm_runtime dpcm[2];
  int fe_compr;
  long pmdown_time;
  unsigned char pop_wait:1;
  /* runtime devices */
  struct snd_pcm *pcm;
  struct snd_compr *compr;
  struct snd_soc_codec *codec;
  struct snd_soc_platform *platform;
  struct snd_soc_dai *codec_dai;
  struct snd_soc_dai *cpu_dai;
  struct snd_soc_component *component; /* Only valid for AUX dev rtds */
};
#endif
    result->format = "dev[%p]card[%p]pcm[%p]";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->dev : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->card : NULL, ",
                     NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->pcm : NULL", NULL);
    } else {
      assert(false);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else {
    return false;
  }
  return true;
}

static bool
__format_glib(struct sp_ts_Context *ctx,
              struct arg_list *result,
              const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "GObject") == 0) {
#if 0
typedef struct _GObject                  GObject;
struct  _GObject {
  GTypeInstance  g_type_instance;
  /*< private >*/
  volatile guint ref_count;
  GData         *qdata;
};

TODO typedef struct _GTypeQuery		GTypeQuery;
struct _GTypeQuery {
  GType		type;
  const gchar  *type_name;
  guint		class_size;
  guint		instance_size;
};
#endif
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " && ", //
                     pprefix, result->variable, "->g_type_instance.g_class",
                     " ? g_type_name(", pprefix, result->variable,
                     "->g_type_instance.g_class->g_type)", //
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     ".g_type_instance.g_class", //
                     " ? g_type_name(", pprefix, result->variable,
                     ".g_type_instance.g_class->g_type)", " : \"(NULL)\"",
                     NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GObjectClass") == 0) {
#if 0
struct  _GObjectClass {
  GTypeClass   g_type_class;
  /*< private >*/
  GSList      *construct_properties;
  /*< public >*/
  /* seldom overridden */
  GObject*   (*constructor)     (GType                  type, guint                  n_construct_properties, GObjectConstructParam *construct_properties);
  /* overridable methods */
  void       (*set_property)		(GObject        *object, guint           property_id, const GValue   *value, GParamSpec     *pspec);
  void       (*get_property)		(GObject        *object, guint           property_id, GValue         *value, GParamSpec     *pspec);
  void       (*dispose)			(GObject        *object);
  void       (*finalize)		(GObject        *object);
  /* seldom overridden */
  void       (*dispatch_properties_changed) (GObject      *object, guint	   n_pspecs, GParamSpec  **pspecs);
  /* signals */
  void	     (*notify)			(GObject	*object, GParamSpec	*pspec);
  /* called when done constructing */
  void	     (*constructed)		(GObject	*object);
  /*< private >*/
  gsize		flags;
};
#endif
  } else if (strcmp(result->type, "GTypeInstance") == 0 ||
             strcmp(result->type, "_GTypeInstance") == 0) {
#if 0
typedef struct _GTypeInstance           GTypeInstance;
struct _GTypeInstance {
  /*< private >*/
  GTypeClass *g_class;
};
#endif
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " && ", //
                     pprefix, result->variable, "->g_class", //
                     " ? g_type_name(", pprefix, result->variable,
                     "->g_class->g_type)", //
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, "->g_class",
                     " ? g_type_name(", pprefix, result->variable,
                     ".g_class->g_type)", //
                     " : \"(NULL)\"", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GTypeClass") == 0 ||
             strcmp(result->type, "_GTypeClass") == 0) {
#if 0
typedef struct _GTypeClass              GTypeClass;
struct _GTypeClass {
  /*< private >*/
  GType g_type;
};
#endif
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ?", //
                     " g_type_name(", pprefix, result->variable, "->g_type)",
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable,
                     ".g_type)", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GValue") == 0 ||
             strcmp(result->type, "_GValue") == 0) {
#if 0
struct _GValue {
  GType		g_type;
  union {
    gint	v_int;
    guint	v_uint;
    glong	v_long;
    gulong	v_ulong;
    gint64      v_int64;
    guint64     v_uint64;
    gfloat	v_float;
    gdouble	v_double;
    gpointer	v_pointer;
  } data[2];
};
#endif
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "type:%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ?", //
                     " g_type_name(", pprefix, result->variable, "->g_type)",
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable,
                     ".g_type)", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GType") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ?",
                     " g_type_name(*", pprefix, result->variable, ")",
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_type_name(", pprefix, result->variable, ")",
                     NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GError") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->message", //
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".message", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "XmlNode") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", //
                     pprefix, result->variable, "->name", //
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GIOChannel") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%d";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "g_io_channel_unix_get_fd(", pprefix, result->variable,
                     ")", //
                     " : ", "-1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_io_channel_unix_get_fd(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GKeyFile") == 0 ||
             strcmp(result->type, "GVariantBuilder") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ?", //
                     " \"SOME\" : \"(NULL)\"", NULL);
    } else {
      sp_str_append(&buf_tmp, "\"SOME\"");
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GVariant") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "g_variant_print(", pprefix, result->variable, ", FALSE)",
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_variant_print(&", pprefix, result->variable,
                     ", FALSE)", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GVariantIter") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      result->format = "children[%zu]%s";
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "g_variant_iter_n_children(", pprefix, result->variable,
                     ")", " : 0", NULL);
      sp_str_appends(&buf_tmp, ", ", pprefix, result->variable, " ? ", "\"\"",
                     " : \"(NULL)\"", NULL);
    } else {
      result->format = "children[%zu]";
      sp_str_appends(&buf_tmp, "g_variant_iter_n_children(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GArray") == 0 ||
             strcmp(result->type, "GPtrArray") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      result->format = "len[%u]%s";
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->len", " : 0", NULL);
      sp_str_appends(&buf_tmp, ", ", pprefix, result->variable, " ? ", "\"\"",
                     " : \"(NULL)\"", NULL);
    } else {
      result->format = "len[%u]";
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".len", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GList") == 0) {
    result->format = "%p";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    sp_str_append(&buf_tmp, "(const void*)");
    if (result->pointer) {
    } else {
      sp_str_append(&buf_tmp, "&");
    }
    sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GDBusMethodInvocation") == 0) {
    /* https://www.freedesktop.org/software/gstreamer-sdk/data/docs/2012.5/gio/GDBusMethodInvocation.html#g-dbus-method-invocation-get-sender */
    result->format = "%s";
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? g_dbus_method_invocation_get_sender(", pprefix,
                     result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_dbus_method_invocation_get_sender(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GHashTable") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    if (result->pointer) {
      result->format = "len[%u]";
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? g_hash_table_size(", pprefix, result->variable, ")",
                     " : 1337", NULL);
    } else {
      result->format = "len[%u]";
      sp_str_appends(&buf_tmp, "g_hash_table_size(&", pprefix, result->variable,
                     ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GDBusConnection") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    /* ALTERNATILVY g_dbus_connection_get_guid() instead */
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? g_dbus_connection_get_unique_name(", pprefix,
                     result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_dbus_connection_get_unique_name(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GPrivate") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    if (result->pointer) {
      result->format = "%p%s";
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? (const void*)g_private_get(", pprefix,
                     result->variable, ")", " : \"(NULL)\",", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? \"\" : \"(NULL)\"", NULL);
    } else {
      result->format = "%p";
      sp_str_appends(&buf_tmp, "(const void*)g_private_get(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GFile") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? g_file_get_path(",
                     pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_file_get_path(&", pprefix, result->variable,
                     ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GString") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable,
                     "->str"
                     " : \"(NULL)\"",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".str", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GDBusProxy") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s:%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? g_dbus_proxy_get_object_path(", pprefix,
                     result->variable, ")", " : \"(NULL)\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? g_dbus_proxy_get_interface_name(", pprefix,
                     result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_dbus_proxy_get_object_path(&", pprefix,
                     result->variable, "), ", NULL);
      sp_str_appends(&buf_tmp, "g_dbus_proxy_get_interface_name(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GDir") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? g_dir_read_name(",
                     pprefix, result->variable, ")", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "g_dir_read_name(&", pprefix, result->variable,
                     ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "GParamSpec") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->name", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "gpointer") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%p";
    sp_str_appends(&buf_tmp, "(const void*)", pprefix, result->variable, NULL);
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else {
    return false;
  }

  return true;
}

static bool
__format_sd(struct sp_ts_Context *ctx,
            struct arg_list *result,
            const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "sd_bus_message") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " ? sd_bus_message_get_signature(", pprefix,
                     result->variable, ", true)", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "sd_bus_message_get_signature(&", pprefix,
                     result->variable, ")", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "sd_bus_error") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s:%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->name", " : \"(NULL)\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->message", " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".name, ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".message", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "sd_bus") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

#if 0
      result->format = "%p:open[%s]ready[%s]";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, "(void *)&", pprefix, result->variable, NULL);
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_open(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\" : \"NULL\"");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_open(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\"");
      }
      sp_str_append(&buf_tmp, ", ");

      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, " ? sd_bus_is_ready(");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\" : \"NULL\"");
      } else {
        sp_str_append(&buf_tmp, "sd_bus_is_ready(&");
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
        sp_str_append(&buf_tmp, ") ? \"TRUE\" : \"FALSE\"");
      }
#else
    result->format = "%p:open[%d]ready[%d]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
    } else {
      sp_str_appends(&buf_tmp, "(void *)&", pprefix, result->variable, NULL);
    }
    sp_str_append(&buf_tmp, ", ");

    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, " ? sd_bus_is_open(");
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, ") : 1337");
    } else {
      sp_str_append(&buf_tmp, "sd_bus_is_open(&");
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, ")");
    }
    sp_str_append(&buf_tmp, ", ");

    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, " ? sd_bus_is_ready(");
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, ") : 1337");
    } else {
      sp_str_append(&buf_tmp, "sd_bus_is_ready(&");
      sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, ")");
    }

#endif
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;

    sp_str_free(&buf_tmp);
  } else {
    return false;
  }

  return true;
}

static bool
__format_libc(struct sp_ts_Context *ctx,
              struct arg_list *result,
              const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "mode_t") == 0) {
    if (result->pointer) {
    } else {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%c%c%c%c%c%c%c%c%c";

      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IRUSR ? 'r' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IWUSR ? 'w' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IXUSR ? 'x' : '-', ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IRGRP ? 'r' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IWGRP ? 'w' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IXGRP ? 'x' : '-', ", NULL);

      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IROTH ? 'r' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IWOTH ? 'w' : '-', ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable,
                     " & S_IXOTH ? 'x' : '-'", NULL);

      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    }
  } else if (strcmp(result->type, "gid_t") == 0 ||
             strcmp(result->type, "uid_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%u";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "(unsigned int)", pprefix, result->variable, " : 1337",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                     NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "pid_t") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%u";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "(unsigned int)", pprefix, result->variable, " : 1337",
                     NULL);
    } else {
      sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                     NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "passwd") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "name[%s]uid[%u]gid[%u]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->pw_name : \"\",", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                     pprefix, result->variable, "->pw_uid : 1337,", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                     pprefix, result->variable, "->pw_gid : 1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".pw_name, ", NULL);
      sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                     ".pw_uid, ", NULL);
      sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                     ".pw_gid", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "group") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "name[%s]gid[%u]";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->gr_name : \"\", ", NULL);
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? (unsigned int)",
                     pprefix, result->variable, "->gr_gid : 1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".gr_name, ", NULL);
      sp_str_appends(&buf_tmp, "(unsigned int)", pprefix, result->variable,
                     ".gr_gid", NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "pollfd") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "fd[%d]";
    if (result->pointer) {
      /* TODO https://man7.org/linux/man-pages/man2/poll.2.html */
      /* add enum of possible events */
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", pprefix,
                     result->variable, "->fd", " : -1337", NULL);
    } else {
      sp_str_appends(&buf_tmp, pprefix, result->variable, ".fd", NULL);
    }

    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "FILE") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%p";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, "(void *)", pprefix, result->variable, NULL);
    } else {
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else if (strcmp(result->type, "time_t") == 0) {
    sp_str buf_tmp;
    result->format = "%s(%jd)";

    sp_str_init(&buf_tmp, 0);
    sp_str_appends(&buf_tmp, //
                   "asctime(", //
                   "gmtime(&", pprefix, result->variable, ")", "), (intmax_t)",
                   pprefix, result->variable, NULL);
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;

    sp_str_free(&buf_tmp);
  } else {
    return false;
  }

  return true;
}

static bool
__format_libcpp(struct sp_ts_Context *ctx,
                struct arg_list *result,
                const char *pprefix)
{
  //TODO namespace check
  (void)ctx;
  if (strcmp(result->type, "string") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    /* Example: string */
    result->format = "%s";
    sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
    if (result->pointer) {
      sp_str_append(&buf_tmp, "->c_str()");
    } else {
      sp_str_append(&buf_tmp, ".c_str()");
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;

    sp_str_free(&buf_tmp);
    return true;
  } else if (strcmp(result->type, "vector") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);

    sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
    if (result->pointer) {
      result->format = "%ld";
      sp_str_appends(&buf_tmp, " ? (long)", pprefix, result->variable, NULL);
      sp_str_append(&buf_tmp, "->size() : -1");
    } else {
      result->format = "%zu";
      sp_str_append(&buf_tmp, ".size()");
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;

    sp_str_free(&buf_tmp);
    return true;
  }

  return false;
}

static bool
__format_cutil(struct sp_ts_Context *ctx,
               struct arg_list *result,
               const char *pprefix)
{
  (void)ctx;
  if (strcmp(result->type, "sp_str") == 0) {
    sp_str buf_tmp;
    sp_str_init(&buf_tmp, 0);
    result->format = "%s";
    if (result->pointer) {
      sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ",
                     "sp_str_c_str(", pprefix, result->variable, ")",
                     " : \"(NULL)\"", NULL);
    } else {
      sp_str_appends(&buf_tmp, "sp_str_c_str(&", pprefix, result->variable, ")",
                     NULL);
    }
    free(result->complex_raw);
    result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
    result->complex_printf = true;
    sp_str_free(&buf_tmp);
  } else {
    return false;
  }

  return true;
}

void
__format(struct sp_ts_Context *ctx,
         struct arg_list *result,
         const char *pprefix)
{
  (void)ctx;
  /* https://developer.gnome.org/glib/stable/glib-Basic-Types.html */
  /* https://en.cppreference.com/w/cpp/types/integer */
  //TODO strdup
  if (result->dead) {
    return;
  }

  /* fprintf(stderr, "- %s\n", type); */
  if (result) {
    if (result->rec) {
      struct arg_list *it = result->rec;
      while (it) {
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        fprintf(stderr, "__%s:%s\n", it->variable, it->type);
        sp_str_appends(&buf_tmp, result->variable, ".", it->variable, NULL);

        free(it->variable);
        it->variable = strdup(sp_str_c_str(&buf_tmp));
        sp_str_free(&buf_tmp);

        if (!it->next) {
          break;
        }
        it = it->next;
      } //while
      it->next     = result->next;
      result->next = result->rec;
      result->dead = true;
      return;
    }
  }
  if (result->function_pointer) {
    if (ctx->domain == LINUX_KERNEL_DOMAIN && result->pointer == 1) {
      result->format = "%pF";
    } else {
      result->format = "%p";
    }
  } else if (result->type) {
    if (result->pointer > 1) {
      if (strcmp(result->type, "GError") == 0) {
      } else {
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);
        result->format = "%p";
        sp_str_appends(&buf_tmp, "(const void*)", pprefix, result->variable,
                       NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      }
    } else if (strcmp(result->type, "gboolean") == 0 || //
               strcmp(result->type, "bool") == 0 || //
               strcmp(result->type, "boolean") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%s";

      if (result->pointer) {
        sp_str_appends(&buf_tmp, "!", pprefix, result->variable,
                       " ? \"(NULL)\" : *", pprefix, result->variable, NULL);
      } else {
        sp_str_appends(&buf_tmp, pprefix, result->variable, NULL);
      }
      sp_str_appends(&buf_tmp, " ? \"TRUE\" : \"FALSE\"", NULL);
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;

      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "void") == 0) {
      if (result->pointer) {
        result->format = "%p";
      }
    } else if (strcmp(result->type, "char") == 0 || //
               strcmp(result->type, "gchar") == 0 || //
               strcmp(result->type, "gint8") == 0 || //
               strcmp(result->type, "int8") == 0 || //
               strcmp(result->type, "int8_t") == 0 ||
               strcmp(result->type, "xmlChar") == 0) {
      if (result->is_array) {
        sp_str buf_tmp;
        result->format = "%.*s";

        sp_str_init(&buf_tmp, 0);
        /* char $field_identifier[$array_len] */
        sp_str_appends(&buf_tmp, "(int)", result->variable_array_length, ", ",
                       pprefix, result->variable, NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else if (result->pointer) {
        result->format = "%s";
      } else {
        result->format = "%c";
      }
    } else if (strcmp(result->type, "spinlock_t") == 0 ||
               strcmp(result->type, "pthread_spinlock_t") == 0 ||
               strcmp(result->type, "pthread_mutex_t") == 0 ||
               strcmp(result->type, "mutex_t") == 0 ||
               strcmp(result->type, "mutex") == 0 ||
               strcmp(result->type, "GMutex") == 0 ||
               strcmp(result->type, "GMutexLocker") == 0 ||
               strcmp(result->type, "GThreadPool") == 0 ||
               strcmp(result->type, "GRecMutex") == 0 ||
               strcmp(result->type, "GRWLock") == 0 ||
               strcmp(result->type, "GCond") == 0 ||
               strcmp(result->type, "GOnce") == 0 ||
               strcmp(result->type, "struct mutex") == 0) {
      /* fprintf(stderr, "type[%s]\n", type); */
    } else if (strcmp(result->type, "IMFIX") == 0) {
      sp_str buf_tmp;
      sp_str_init(&buf_tmp, 0);
      result->format = "%f";
      if (result->pointer) {
        sp_str_appends(&buf_tmp, pprefix, result->variable, " ? ", "IMFIX2F(*",
                       pprefix, result->variable, ") : 0", NULL);
      } else {
        sp_str_appends(&buf_tmp, //
                       "IMFIX2F(", pprefix, result->variable, ")", NULL);
      }
      free(result->complex_raw);
      result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
      result->complex_printf = true;
      sp_str_free(&buf_tmp);
    } else if (strcmp(result->type, "uchar") == 0 || //
               strcmp(result->type, "unsigned char") == 0 || //
               strcmp(result->type, "guchar") == 0 || //
               strcmp(result->type, "guint8") == 0 || //
               strcmp(result->type, "uint8") == 0 || //
               strcmp(result->type, "u8") == 0 || //
               strcmp(result->type, "uint8_t") == 0) {
      if (result->pointer) {
        /* TODO if pointer hex? */
        result->format = "%p";
      } else if (result->is_array) {
        /* TODO if pointer hex? */
      } else {
        result->format = "%d";
      }
    } else if (strcmp(result->type, "short") == 0 || //
               strcmp(result->type, "gshort") == 0 || //
               strcmp(result->type, "gint16") == 0 || //
               strcmp(result->type, "int16") == 0 || //
               strcmp(result->type, "i16") == 0 || //
               strcmp(result->type, "int16_t") == 0) {
      __format_numeric(result, pprefix, "%d");
    } else if (strcmp(result->type, "unsigned short") == 0 || //
               strcmp(result->type, "gushort") == 0 || //
               strcmp(result->type, "guint16") == 0 || //
               strcmp(result->type, "uint16") == 0 || //
               strcmp(result->type, "u16") == 0 || //
               strcmp(result->type, "uint16_t") == 0) {
      __format_numeric(result, pprefix, "%u");
    } else if (strcmp(result->type, "int") == 0 || //
               strcmp(result->type, "signed int") == 0 || //
               strcmp(result->type, "gint") == 0 || //
               strcmp(result->type, "gint32") == 0 || //
               strcmp(result->type, "i32") == 0 || //
               strcmp(result->type, "int32") == 0 || //
               strcmp(result->type, "int32_t") == 0) {
      __format_numeric(result, pprefix, "%d");
    } else if (strcmp(result->type, "unsigned") == 0 || //
               strcmp(result->type, "unsigned int") == 0 || //
               strcmp(result->type, "guint") == 0 || //
               strcmp(result->type, "guint32") == 0 || //
               strcmp(result->type, "uint32") == 0 || //
               strcmp(result->type, "u32") == 0 || //
               strcmp(result->type, "uint32_t") == 0) {
      __format_numeric(result, pprefix, "%u");
    } else if (strcmp(result->type, "long") == 0 || //
               strcmp(result->type, "long int") == 0 || //
               strcmp(result->type, "snd_pcm_sframes_t") == 0 || //
               strcmp(result->type, "signed long") == 0) {
      __format_numeric(result, pprefix, "%ld");
    } else if (strcmp(result->type, "unsigned long int") == 0 || //
               strcmp(result->type, "long unsigned int") == 0 || //
               strcmp(result->type, "snd_pcm_uframes_t") == 0 || //
               strcmp(result->type, "unsigned long") == 0 || //
               strcmp(result->type, "gulong") == 0) {
      __format_numeric(result, pprefix, "%lu");
    } else if (strcmp(result->type, "long long") == 0 || //
               strcmp(result->type, "long long int") == 0) {
      __format_numeric(result, pprefix, "%lld");
    } else if (strcmp(result->type, "unsigned long long") == 0 || //
               strcmp(result->type, "unsigned long long int") == 0 ||
               strcmp(result->type, "u64") == 0) {
      __format_numeric(result, pprefix, "%llu");
    } else if (strcmp(result->type, "off_t") == 0) {
      __format_numeric(result, pprefix, "%jd");
    } else if (strcmp(result->type, "goffset") == 0) {
      result->format = "%\"G_GOFFSET_FORMAT\"";
    } else if (strcmp(result->type, "size_t") == 0) {
      __format_numeric(result, pprefix, "%zu");
    } else if (strcmp(result->type, "gsize") == 0) {
      result->format = "%\"G_GSIZE_FORMAT\"";
    } else if (strcmp(result->type, "ssize_t") == 0) {
      __format_numeric(result, pprefix, "%zd");
    } else if (strcmp(result->type, "gssize") == 0) {
      result->format = "%\"G_GSSIZE_FORMAT\"";
    } else if (strcmp(result->type, "int64_t") == 0) {
      result->format = "%\"PRId64\"";
    } else if (strcmp(result->type, "uint64_t") == 0) {
      result->format = "%\"PRIu64\"";
    } else if (strcmp(result->type, "gint64") == 0) {
      result->format = "%\"G_GINT64_FORMAT\"";
    } else if (strcmp(result->type, "guint64") == 0) {
      result->format = "%\"G_GUINT64_FORMAT\"";
    } else if (strcmp(result->type, "uintptr_t") == 0) {
      result->format = "%\"PRIuPTR\"";
    } else if (strcmp(result->type, "guintptr") == 0) {
      result->format = "%\"G_GUINTPTR_FORMAT\"";
    } else if (strcmp(result->type, "iintptr_t") == 0) {
      result->format = "%\"PRIiPTR\"";
    } else if (strcmp(result->type, "gintptr") == 0) {
      result->format = "%\"G_GINTPTR_FORMAT\"";
    } else if (strcmp(result->type, "float") == 0 || //
               strcmp(result->type, "gfloat") == 0 || //
               strcmp(result->type, "double") == 0 || //
               strcmp(result->type, "gdouble") == 0) {
      __format_numeric(result, pprefix, "%f");
    } else if (strcmp(result->type, "long double") == 0) {
      __format_numeric(result, pprefix, "%Lf");
    } else if (__format_libxml2(ctx, result, pprefix)) {
    } else if (__format_alsa(ctx, result, pprefix)) {
    } else if (__format_glib(ctx, result, pprefix)) {
    } else if (__format_sd(ctx, result, pprefix)) {
    } else if (__format_libc(ctx, result, pprefix)) {
    } else if (__format_libcpp(ctx, result, pprefix)) {
    } else if (__format_cutil(ctx, result, pprefix)) {
    } else {
      if (strchr(result->type, ' ') == NULL) {
        const char *prefix = "&";
        sp_str buf_tmp;
        sp_str_init(&buf_tmp, 0);

        /* Example: type_t */
        result->format = "%s";
        if (result->pointer) {
          prefix = "";
        }
        sp_str_appends(&buf_tmp, "sp_debug_", result->type, "(", NULL);
        sp_str_appends(&buf_tmp, prefix, pprefix, result->variable, NULL);
        sp_str_appends(&buf_tmp, ")", NULL);
        free(result->complex_raw);
        result->complex_raw    = strdup(sp_str_c_str(&buf_tmp));
        result->complex_printf = true;

        sp_str_free(&buf_tmp);
      } else {
        assert(false);
      }
    }
  }
}
