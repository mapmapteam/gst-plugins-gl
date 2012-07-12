/*
 * GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2002,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-gltestsrc
 *
 * <refsect2>
 * <para>
 * The gltestsrc element is used to produce test video texture.
 * The video test produced can be controlled with the "pattern"
 * property.
 * </para>
 * <title>Example launch line</title>
 * <para>
 * <programlisting>
 * gst-launch -v gltestsrc pattern=smpte ! glimagesink
 * </programlisting>
 * Shows original SMPTE color bars in a window.
 * </para>
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgltestsrc.h"
#include "gltestsrc.h"
#include <gst/gst-i18n-plugin.h>

#define USE_PEER_BUFFERALLOC

GST_DEBUG_CATEGORY_STATIC (gl_test_src_debug);
#define GST_CAT_DEFAULT gl_test_src_debug

enum
{
  PROP_0,
  PROP_PATTERN,
  PROP_TIMESTAMP_OFFSET,
  PROP_IS_LIVE
      /* FILL ME */
};

#define gst_gl_test_src_parent_class parent_class
G_DEFINE_TYPE (GstGLTestSrc, gst_gl_test_src, GST_TYPE_PUSH_SRC);

static void gst_gl_test_src_set_pattern (GstGLTestSrc * gltestsrc,
    int pattern_type);
static void gst_gl_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_gl_test_src_src_query (GstPad * pad, GstObject * object,
    GstQuery * query);

static gboolean gst_gl_test_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);
static GstCaps *gst_gl_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

static gboolean gst_gl_test_src_is_seekable (GstBaseSrc * psrc);
static gboolean gst_gl_test_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static gboolean gst_gl_test_src_query (GstBaseSrc * bsrc, GstQuery * query);

static void gst_gl_test_src_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static GstFlowReturn gst_gl_test_src_fill (GstPushSrc * psrc,
    GstBuffer * buffer);
static gboolean gst_gl_test_src_start (GstBaseSrc * basesrc);
static gboolean gst_gl_test_src_stop (GstBaseSrc * basesrc);
static gboolean gst_gl_test_src_decide_allocation (GstBaseSrc * basesrc,
    GstQuery * query);

static void gst_gl_test_src_callback (gint width, gint height, guint texture,
    gpointer stuff);

#define GST_TYPE_GL_TEST_SRC_PATTERN (gst_gl_test_src_pattern_get_type ())
static GType
gst_gl_test_src_pattern_get_type (void)
{
  static GType gl_test_src_pattern_type = 0;
  static const GEnumValue pattern_types[] = {
    {GST_GL_TEST_SRC_SMPTE, "SMPTE 100% color bars", "smpte"},
    {GST_GL_TEST_SRC_SNOW, "Random (television snow)", "snow"},
    {GST_GL_TEST_SRC_BLACK, "100% Black", "black"},
    {GST_GL_TEST_SRC_WHITE, "100% White", "white"},
    {GST_GL_TEST_SRC_RED, "Red", "red"},
    {GST_GL_TEST_SRC_GREEN, "Green", "green"},
    {GST_GL_TEST_SRC_BLUE, "Blue", "blue"},
    {GST_GL_TEST_SRC_CHECKERS1, "Checkers 1px", "checkers-1"},
    {GST_GL_TEST_SRC_CHECKERS2, "Checkers 2px", "checkers-2"},
    {GST_GL_TEST_SRC_CHECKERS4, "Checkers 4px", "checkers-4"},
    {GST_GL_TEST_SRC_CHECKERS8, "Checkers 8px", "checkers-8"},
    {GST_GL_TEST_SRC_CIRCULAR, "Circular", "circular"},
    {GST_GL_TEST_SRC_BLINK, "Blink", "blink"},
    {0, NULL, NULL}
  };

  if (!gl_test_src_pattern_type) {
    gl_test_src_pattern_type =
        g_enum_register_static ("GstGLTestSrcPattern", pattern_types);
  }
  return gl_test_src_pattern_type;
}

static void
gst_gl_test_src_class_init (GstGLTestSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;
  GstElementClass *element_class;

  GST_DEBUG_CATEGORY_INIT (gl_test_src_debug, "gltestsrc", 0,
      "Video Test Source");

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_gl_test_src_set_property;
  gobject_class->get_property = gst_gl_test_src_get_property;

  g_object_class_install_property (gobject_class, PROP_PATTERN,
      g_param_spec_enum ("pattern", "Pattern",
          "Type of test pattern to generate", GST_TYPE_GL_TEST_SRC_PATTERN,
          GST_GL_TEST_SRC_SMPTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_TIMESTAMP_OFFSET, g_param_spec_int64 ("timestamp-offset",
          "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, 0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple (element_class, "Video test source",
      "Source/Video", "Creates a test video stream",
      "David A. Schleef <ds@schleef.org>");

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          gst_caps_from_string (GST_GL_VIDEO_CAPS)));

  gstbasesrc_class->set_caps = gst_gl_test_src_setcaps;
  gstbasesrc_class->is_seekable = gst_gl_test_src_is_seekable;
  gstbasesrc_class->do_seek = gst_gl_test_src_do_seek;
  gstbasesrc_class->query = gst_gl_test_src_query;
  gstbasesrc_class->get_times = gst_gl_test_src_get_times;
  gstbasesrc_class->start = gst_gl_test_src_start;
  gstbasesrc_class->stop = gst_gl_test_src_stop;
  gstbasesrc_class->fixate = gst_gl_test_src_fixate;
  gstbasesrc_class->decide_allocation = gst_gl_test_src_decide_allocation;

  gstpushsrc_class->fill = gst_gl_test_src_fill;
}

static void
gst_gl_test_src_init (GstGLTestSrc * src)
{
  GstPad *pad = GST_BASE_SRC_PAD (src);

  gst_gl_test_src_set_pattern (src, GST_GL_TEST_SRC_SMPTE);

  src->timestamp_offset = 0;

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), FALSE);

  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_gl_test_src_src_query));
}

static GstCaps *
gst_gl_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *structure;

  GST_DEBUG ("fixate");

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);

  caps = GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);

  return caps;
}

static void
gst_gl_test_src_set_pattern (GstGLTestSrc * gltestsrc, gint pattern_type)
{
  gltestsrc->pattern_type = pattern_type;

  GST_DEBUG_OBJECT (gltestsrc, "setting pattern to %d", pattern_type);

  switch (pattern_type) {
    case GST_GL_TEST_SRC_SMPTE:
      gltestsrc->make_image = gst_gl_test_src_smpte;
      break;
    case GST_GL_TEST_SRC_SNOW:
      gltestsrc->make_image = gst_gl_test_src_snow;
      break;
    case GST_GL_TEST_SRC_BLACK:
      gltestsrc->make_image = gst_gl_test_src_black;
      break;
    case GST_GL_TEST_SRC_WHITE:
      gltestsrc->make_image = gst_gl_test_src_white;
      break;
    case GST_GL_TEST_SRC_RED:
      gltestsrc->make_image = gst_gl_test_src_red;
      break;
    case GST_GL_TEST_SRC_GREEN:
      gltestsrc->make_image = gst_gl_test_src_green;
      break;
    case GST_GL_TEST_SRC_BLUE:
      gltestsrc->make_image = gst_gl_test_src_blue;
      break;
    case GST_GL_TEST_SRC_CHECKERS1:
      gltestsrc->make_image = gst_gl_test_src_checkers1;
      break;
    case GST_GL_TEST_SRC_CHECKERS2:
      gltestsrc->make_image = gst_gl_test_src_checkers2;
      break;
    case GST_GL_TEST_SRC_CHECKERS4:
      gltestsrc->make_image = gst_gl_test_src_checkers4;
      break;
    case GST_GL_TEST_SRC_CHECKERS8:
      gltestsrc->make_image = gst_gl_test_src_checkers8;
      break;
    case GST_GL_TEST_SRC_CIRCULAR:
      gltestsrc->make_image = gst_gl_test_src_circular;
      break;
    case GST_GL_TEST_SRC_BLINK:
      gltestsrc->make_image = gst_gl_test_src_black;
      break;
    default:
      g_assert_not_reached ();
  }
}

static void
gst_gl_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (object);

  switch (prop_id) {
    case PROP_PATTERN:
      gst_gl_test_src_set_pattern (src, g_value_get_enum (value));
      break;
    case PROP_TIMESTAMP_OFFSET:
      src->timestamp_offset = g_value_get_int64 (value);
      break;
    case PROP_IS_LIVE:
      gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
      break;
    default:
      break;
  }
}

static void
gst_gl_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (object);

  switch (prop_id) {
    case PROP_PATTERN:
      g_value_set_enum (value, src->pattern_type);
      break;
    case PROP_TIMESTAMP_OFFSET:
      g_value_set_int64 (value, src->timestamp_offset);
      break;
    case PROP_IS_LIVE:
      g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_test_src_src_query (GstPad * pad, GstObject * object, GstQuery * query)
{
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CUSTOM:
    {
      const GstStructure *structure = gst_query_get_structure (query);
      res =
          g_strcmp0 (gst_element_get_name (object),
          gst_structure_get_name (structure)) == 0;
      break;
    }
    default:
      res = gst_pad_query_default (pad, object, query);
      break;
  }

  return res;
}

static gboolean
gst_gl_test_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  gboolean res = FALSE;
  GstVideoInfo vinfo;
  GstGLTestSrc *gltestsrc = GST_GL_TEST_SRC (bsrc);

  GST_DEBUG ("setcaps");

  if (!gst_video_info_from_caps (&vinfo, caps))
    goto wrong_caps;

  gltestsrc->out_info = vinfo;
  gltestsrc->negotiated = TRUE;

  if (!(res = gst_gl_display_gen_fbo (gltestsrc->display,
              GST_VIDEO_INFO_WIDTH (&gltestsrc->out_info),
              GST_VIDEO_INFO_HEIGHT (&gltestsrc->out_info),
              &gltestsrc->fbo, &gltestsrc->depthbuffer)))
    GST_ELEMENT_ERROR (gltestsrc, RESOURCE, NOT_FOUND,
        GST_GL_DISPLAY_ERR_MSG (gltestsrc->display), (NULL));

  return res;

wrong_caps:
  {
    GST_WARNING ("wrong caps");
    return FALSE;
  }
}


static gboolean
gst_gl_test_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  gboolean res;
  GstGLTestSrc *src;

  src = GST_GL_TEST_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      res =
          gst_video_info_convert (&src->out_info, src_fmt, src_val, dest_fmt,
          &dest_val);
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
  }

  return res;
}

static void
gst_gl_test_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration))
        *end = timestamp + duration;
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_gl_test_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstClockTime time;
  GstGLTestSrc *src;

  src = GST_GL_TEST_SRC (bsrc);

  segment->time = segment->start;
  time = segment->position;

  /* now move to the time indicated */
  if (src->out_info.fps_n) {
    src->n_frames = gst_util_uint64_scale (time,
        src->out_info.fps_n, src->out_info.fps_d * GST_SECOND);
  } else
    src->n_frames = 0;

  if (src->out_info.fps_n) {
    src->running_time = gst_util_uint64_scale (src->n_frames,
        src->out_info.fps_d * GST_SECOND, src->out_info.fps_n);
  } else {
    /* FIXME : Not sure what to set here */
    src->running_time = 0;
  }

  g_return_val_if_fail (src->running_time <= time, FALSE);

  return TRUE;
}

static gboolean
gst_gl_test_src_is_seekable (GstBaseSrc * psrc)
{
  /* we're seekable... */
  return TRUE;
}

static GstFlowReturn
gst_gl_test_src_fill (GstPushSrc * psrc, GstBuffer * buffer)
{
  GstGLTestSrc *src;
  GstGLMeta *gl_meta;
  GstVideoMeta *v_meta;
  GstClockTime next_time;
  gint width, height;

  src = GST_GL_TEST_SRC (psrc);

  if (G_UNLIKELY (!src->negotiated))
    goto not_negotiated;

  width = GST_VIDEO_INFO_WIDTH (&src->out_info);
  height = GST_VIDEO_INFO_HEIGHT (&src->out_info);

  /* 0 framerate and we are at the second frame, eos */
  if (G_UNLIKELY (GST_VIDEO_INFO_FPS_N (&src->out_info) == 0
          && src->n_frames == 1))
    goto eos;

  if (src->pattern_type == GST_GL_TEST_SRC_BLINK) {
    if (src->n_frames & 0x1)
      src->make_image = gst_gl_test_src_white;
    else
      src->make_image = gst_gl_test_src_black;
  }

  src->buffer = gst_buffer_ref (buffer);
  gl_meta = gst_buffer_get_gl_meta (src->buffer);
  v_meta = gst_buffer_get_video_meta (src->buffer);

  if (!gl_meta || !v_meta) {
    GST_ERROR
        ("Output buffer does not contain required GstVideoMeta or GstGLMeta");
    goto eos;
  }
  //blocking call, generate a FBO
  if (!gst_gl_display_use_fbo (src->display, width, height, src->fbo,
          src->depthbuffer, gl_meta->memory->tex_id, gst_gl_test_src_callback,
          0, 0, 0, 0, width, 0, height, GST_GL_DISPLAY_PROJECTION_ORTHO2D,
          (gpointer) src)) {
    goto eos;
  }

  GST_BUFFER_TIMESTAMP (buffer) = src->timestamp_offset + src->running_time;
  GST_BUFFER_OFFSET (buffer) = src->n_frames;
  src->n_frames++;
  GST_BUFFER_OFFSET_END (buffer) = src->n_frames;
  if (src->out_info.fps_n) {
    next_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
        src->out_info.fps_d, src->out_info.fps_n);
    GST_BUFFER_DURATION (buffer) = next_time - src->running_time;
  } else {
    next_time = src->timestamp_offset;
    /* NONE means forever */
    GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  }

  src->running_time = next_time;

  return GST_FLOW_OK;

not_negotiated:
  {
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        (_("format wasn't negotiated before get function")));
    return GST_FLOW_NOT_NEGOTIATED;
  }
eos:
  {
    GST_DEBUG_OBJECT (src, "eos: 0 framerate, frame %d", (gint) src->n_frames);
    return GST_FLOW_EOS;
  }
}

static gboolean
gst_gl_test_src_start (GstBaseSrc * basesrc)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (basesrc);
  GstElement *parent = GST_ELEMENT (gst_element_get_parent (src));
  GstStructure *structure = NULL;
  GstQuery *query = NULL;
  gboolean isPerformed = FALSE;

  if (!parent) {
    GST_ELEMENT_ERROR (src, CORE, STATE_CHANGE, (NULL),
        ("A parent bin is required"));
    return FALSE;
  }

  structure = gst_structure_new_empty (gst_element_get_name (src));
  query = gst_query_new_custom (GST_QUERY_CUSTOM, structure);

  isPerformed = gst_element_query (parent, query);

  if (isPerformed) {
    const GValue *id_value =
        gst_structure_get_value (structure, "gstgldisplay");
    if (G_VALUE_HOLDS_POINTER (id_value))
      /* at least one gl element is before in our gl chain */
      src->display =
          g_object_ref (GST_GL_DISPLAY (g_value_get_pointer (id_value)));
    else {
      /* this gl filter is a sink in terms of the gl chain */
      src->display = gst_gl_display_new ();
      isPerformed = gst_gl_display_create_context (src->display, 0);

      if (!isPerformed)
        GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
            GST_GL_DISPLAY_ERR_MSG (src->display), (NULL));
    }
  }

  gst_query_unref (query);
  gst_object_unref (GST_OBJECT (parent));

  src->running_time = 0;
  src->n_frames = 0;
  src->negotiated = FALSE;

  return isPerformed;
}

static gboolean
gst_gl_test_src_stop (GstBaseSrc * basesrc)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (basesrc);

  if (src->display) {
    //blocking call, delete the FBO
    gst_gl_display_del_fbo (src->display, src->fbo, src->depthbuffer);
    g_object_unref (src->display);
    src->display = NULL;
  }

  return TRUE;
}

static gboolean
gst_gl_test_src_decide_allocation (GstBaseSrc * basesrc, GstQuery * query)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (basesrc);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;

  gst_query_parse_allocation (query, &caps, NULL);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool)
    pool = gst_gl_buffer_pool_new (src->display);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_GL_META);
  gst_buffer_pool_set_config (pool, config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

//opengl scene
static void
gst_gl_test_src_callback (gint width, gint height, guint texture,
    gpointer stuff)
{
  GstGLTestSrc *src = GST_GL_TEST_SRC (stuff);

  src->make_image (src, src->buffer, GST_VIDEO_INFO_WIDTH (&src->out_info),
      GST_VIDEO_INFO_HEIGHT (&src->out_info));
}
