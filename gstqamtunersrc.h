/*
 * Copyright 2014 RDK Management
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation, version 2
 * of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_QAM_TUNER_SRC_H__
#define __GST_QAM_TUNER_SRC_H__

#include <sys/types.h>

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
/** HDHOMERUN specific */

#define MAX_HD_HOME_RUN_DEVICES 4
#include "hdhomerun.h" /** @brief Function prototypes for HDHOMERUN */

#ifdef QAM_LIVE_RPI
#include <gio/gio.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_QAM_TUNER_SRC gst_QamTuner_src_get_type()
#define GST_QAM_TUNER_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_QAM_TUNER_SRC,QamTunerSrc))
#define GST_QAM_TUNER_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_QAM_TUNER_SRC,QamTunerSrcClass))
#define GST_IS_QAM_TUNER_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_QAM_TUNER_SRC))
#define GST_IS_QAM_TUNER_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_QAM_TUNER_SRC))
#define GST_QAM_TUNER_SRC_CAST(obj) ((QamTunerSrc*) obj)

typedef struct _QamTunerSrc QamTunerSrc;
typedef struct _QamTunerSrcClass QamTunerSrcClass;

/**
 * QamTunerSrc:
 *
 * Opaque #QamTunerSrc structure.
 */
struct _QamTunerSrc {
  GstBaseSrc element;
  gchar *linearloc;			
  gchar *linearpath;
  gint qamfd;				

  /** Store Frequency Read from sidb.xml file */
  guint frequency;
  /** Store Program Number Read from sidb.xml file */
  gint program_number;
  guint64 read_position;	
  gboolean touch;	
  gboolean sequential;                 
  gboolean Is_seekable;                   
  guint64 start_time;
  gboolean IsStart;
  gdouble  m_bitRate;
  /** store hdhomerun device id */
  gulong hdhomerun_device_id;                                     
  /** store hdhomerun tuner number */                             
  gint hdhomerun_tuner;
  /** store hdhomerun channel */
  gchar hdhomerun_channel_map[15];                                
  /**   Pointer to the structure to be used for the HDHOMERUN Device */                                                              
  struct hdhomerun_device_t *pDevice; 

#ifdef QAM_LIVE_RPI
  /* properties */
  gchar     *address;
  gint       port;
  GSocket   *socket;
  gboolean   close_socket;
  gboolean   reuse;
  guint      max_size;

  /* sockets */
  GSocket   *used_socket;
  GInetSocketAddress *adr;
  gboolean   external_socket;

  /* memory management */
  GstAllocator *allocator;
  GstAllocationParams params;

  /* memory references */
  GstMemory   *mem;
  GstMapInfo   map;
  GstMemory   *mem_max;
  GstMapInfo   map_max;
  GInputVector vec[2];
#endif
};

struct _QamTunerSrcClass {
  GstBaseSrcClass parent_class;
  void (*qamtunersrcstatuscb)( QamTunerSrc* gstqamtunersrc, gint status, gpointer userdata);
};

GType gst_QamTuner_src_get_type (void);

G_END_DECLS

#endif /* __GST_QAM_TUNER_SRC_H__ */

