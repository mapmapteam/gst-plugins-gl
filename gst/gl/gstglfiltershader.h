/*
 * glshader gstreamer plugin
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 * Copyright (C) 2009 Luc Deschenaux <luc.deschenaux@freesurf.ch>
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

#ifndef _GST_GL_FILTERSHADER_H_
#define _GST_GL_FILTERSHADER_H_

#include <gstglfilter.h>

#define GST_TYPE_GL_FILTERSHADER            (gst_gl_filtershader_get_type())
#define GST_GL_FILTERSHADER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_FILTERSHADER,GstGLFilterShader))
#define GST_IS_GL_FILTERSHADER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_FILTERSHADER))
#define GST_GL_FILTERSHADER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_FILTERSHADER,GstGLFilterShaderClass))
#define GST_IS_GL_FILTERSHADER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_FILTERSHADER))
#define GST_GL_FILTERSHADER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_FILTERSHADER,GstGLFilterShaderClass))

typedef struct _GstGLFilterShader GstGLFilterShader;
typedef struct _GstGLFilterShaderClass GstGLFilterShaderClass;

struct _GstGLFilterShader
{
  GstGLFilter filter;
  GstGLShader *shader0;
  int compiled;
  gchar *filename;
  gchar *presetfile;
  int texSet;

};

struct _GstGLFilterShaderClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_glfiltershader_get_type (void);

#endif /* _GST_GL_FILTERSHADER_H_ */
