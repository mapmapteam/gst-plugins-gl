/*
 * GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglwindow.h"

#include <GL/glx.h>

#define GST_GL_WINDOW_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE((o), GST_GL_TYPE_WINDOW, GstGLWindowPrivate))

enum
{
  ARG_0,
  ARG_DISPLAY
};

struct _GstGLWindowPrivate
{
  gchar *display_name;
  Display *device;
  gint screen;
  Window root;
  gint device_width;
  gint device_height;
  gint connection;
  Atom atom_delete_window;
  Atom atom_custom;
  XVisualInfo *visual_info;

  Window internal_win_id;
  GLXContext gl_context;

  GstGLWindowCB draw_cb;
  gpointer draw_data;
  GstGLWindowCB2 resize_cb;
  gpointer resize_data;
  GstGLWindowCB close_cb;
  gpointer close_data;

  gboolean is_closed;

  GMutex *mutex;
  GCond *cond_send_message;
};

G_DEFINE_TYPE (GstGLWindow, gst_gl_window, G_TYPE_OBJECT);

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "GstGLWindow"

gboolean _gst_gl_window_debug = FALSE;

/* Must be called in the gl thread */
static void
gst_gl_window_finalize (GObject * object)
{
  GstGLWindow *window = GST_GL_WINDOW (object);
  GstGLWindowPrivate *priv = window->priv;

  XUnmapWindow (priv->device, priv->internal_win_id);

  glXMakeCurrent (priv->device, None, NULL);

  glXDestroyContext (priv->device, priv->gl_context);

  XDestroyWindow (priv->device, priv->internal_win_id);

  XCloseDisplay (priv->device);


  g_cond_signal (priv->cond_send_message);

  if (priv->cond_send_message)
    g_cond_free (priv->cond_send_message);

  if (priv->mutex)
    g_mutex_free (priv->mutex);

  G_OBJECT_CLASS (gst_gl_window_parent_class)->finalize (object);
}

static void
gst_gl_window_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLWindow *window;
  GstGLWindowPrivate *priv;

  g_return_if_fail (GST_GL_IS_WINDOW (object));

  window = GST_GL_WINDOW (object);

  priv = window->priv;

  switch (prop_id) {
    case ARG_DISPLAY:
      priv->display_name = g_strdup (g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_window_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLWindow *window;
  GstGLWindowPrivate *priv;

  g_return_if_fail (GST_GL_IS_WINDOW (object));

  window = GST_GL_WINDOW (object);

  priv = window->priv;

  switch (prop_id) {
    case ARG_DISPLAY:
      g_value_set_string (value, priv->display_name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_window_log_handler (const gchar *domain, GLogLevelFlags flags,
                           const gchar *message, gpointer user_data)
{
  if (_gst_gl_window_debug) {
    g_log_default_handler (domain, flags, message, user_data);
  }
}

static void
gst_gl_window_class_init (GstGLWindowClass * klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstGLWindowPrivate));

  obj_class->finalize = gst_gl_window_finalize;
  obj_class->set_property = gst_gl_window_set_property;
  obj_class->get_property = gst_gl_window_get_property;

  g_object_class_install_property (obj_class, ARG_DISPLAY,
      g_param_spec_string ("display", "Display", "X Display name", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_window_init (GstGLWindow *window)
{
  window->priv = GST_GL_WINDOW_GET_PRIVATE (window);

  if (g_getenv ("GST_GL_WINDOW_DEBUG") != NULL)
    _gst_gl_window_debug = TRUE;

  g_log_set_handler ("GstGLWindow", G_LOG_LEVEL_DEBUG,
    gst_gl_window_log_handler, NULL);
}

/* Must be called in the gl thread */
GstGLWindow *
gst_gl_window_new (gint width, gint height)
{
  GstGLWindow *window = g_object_new (GST_GL_TYPE_WINDOW, NULL);
  GstGLWindowPrivate *priv = window->priv;

  gint attrib[] = {
    GLX_RGBA,
    GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1, GLX_BLUE_SIZE, 1,
    GLX_DOUBLEBUFFER,
    GLX_DEPTH_SIZE, 1,
    None
  };

  XSetWindowAttributes win_attr;
  XTextProperty text_property;
  XSizeHints size_hints;
  XWMHints wm_hints;
  unsigned long mask;
  const gchar *title = "OpenGL renderer";

  Bool ret = FALSE;

  static gint x = 0;
  static gint y = 0;

  x += 20;
  y += 20;

  priv->device = XOpenDisplay (priv->display_name);
  priv->screen = DefaultScreen (priv->device);
  priv->root = RootWindow (priv->device, priv->screen);

  priv->device_width = DisplayWidth (priv->device, priv->screen);
  priv->device_height = DisplayHeight (priv->device, priv->screen);

  priv->connection = ConnectionNumber (priv->device);
  priv->atom_delete_window = XInternAtom (priv->device, "WM_DELETE_WINDOW", FALSE);
  priv->atom_custom = XInternAtom (priv->device, "WM_GSTGLWINDOW", FALSE);

  priv->visual_info = glXChooseVisual (priv->device, priv->screen, attrib);

  win_attr.event_mask =
    StructureNotifyMask | SubstructureNotifyMask | ExposureMask |
    ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
    VisibilityChangeMask | EnterWindowMask | LeaveWindowMask |
    PointerMotionMask | ButtonMotionMask;

  win_attr.background_pixmap = None;
  win_attr.background_pixel = 0;
  win_attr.border_pixel = 0;

  win_attr.colormap = XCreateColormap(priv->device, priv->root, priv->visual_info->visual, AllocNone);

  mask = CWBackPixmap | CWBorderPixel | CWColormap | CWEventMask;

  priv->internal_win_id = XCreateWindow (priv->device, priv->root, x, y,
    width, height, 0, priv->visual_info->depth, InputOutput,
    priv->visual_info->visual, mask, &win_attr);

  g_debug ("gl window id: %lld\n", (guint64) priv->internal_win_id);

  priv->gl_context = glXCreateContext (priv->device, priv->visual_info, NULL, TRUE);

  g_debug ("gl context id: %ld\n", (gulong) priv->gl_context);

  if (!glXIsDirect(priv->device, priv->gl_context))
    g_debug ("direct rendering failed\n");

  size_hints.flags = USPosition | USSize;
  size_hints.x = x;
  size_hints.y = y;
  size_hints.width = width;
  size_hints.height= height;

  wm_hints.flags = StateHint;
  wm_hints.initial_state = NormalState;

  XStringListToTextProperty ((char**)&title, 1, &text_property);

  XSetWMProperties (priv->device, priv->internal_win_id, &text_property, &text_property, 0, 0,
    &size_hints, &wm_hints, NULL);

  XFree (text_property.value);

  XSetWMProtocols (priv->device, priv->internal_win_id, &priv->atom_delete_window, 1);

  ret = glXMakeCurrent (priv->device, priv->internal_win_id, priv->gl_context);

  if (!ret)
    g_debug ("failed to make opengl context current\n");

  priv->mutex = g_mutex_new ();
  priv->cond_send_message = g_cond_new ();
  priv->is_closed = FALSE;

  XResizeWindow (priv->device, priv->internal_win_id, width, height);

  return window;
}

GQuark
gst_gl_window_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-window-error");
}

void
gst_gl_window_set_external_window_id (GstGLWindow *window, guint64 id)
{
  /*stGLWindowPrivate *priv = window->priv;
  WNDPROC window_parent_proc = (WNDPROC) (guint64) GetWindowLongPtr((HWND)id, GWL_WNDPROC);
  RECT rect;

  SetProp (priv->internal_win_id, "gl_window_parent_id", (HWND)id);
  SetProp ((HWND)id, "gl_window_id", priv->internal_win_id);
  SetProp ((HWND)id, "gl_window_parent_proc", (WNDPROC) window_parent_proc);
  SetWindowLongPtr ((HWND)id, GWL_WNDPROC, (DWORD) (guint64) sub_class_proc);

  SetWindowLongPtr (priv->internal_win_id, GWL_STYLE, WS_CHILD | WS_MAXIMIZE);
  SetParent (priv->internal_win_id, (HWND)id);

  //take changes into account: SWP_FRAMECHANGED
  GetClientRect ((HWND)id, &rect);
  SetWindowPos (priv->internal_win_id, HWND_TOP, rect.left, rect.top, rect.right, rect.bottom,
    SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
  MoveWindow (priv->internal_win_id, rect.left, rect.top, rect.right, rect.bottom, FALSE);*/
}

void
gst_gl_window_set_external_gl_context (GstGLWindow *window, guint64 context)
{
  g_warning ("gst_gl_window_set_external_gl_context: not implemented\n");
}

/* Must be called in the gl thread */
void
gst_gl_window_set_draw_callback (GstGLWindow *window, GstGLWindowCB callback, gpointer data)
{
  GstGLWindowPrivate *priv = window->priv;

  priv->draw_cb = callback;
  priv->draw_data = data;
}

/* Must be called in the gl thread */
void
gst_gl_window_set_resize_callback (GstGLWindow *window, GstGLWindowCB2 callback , gpointer data)
{
  GstGLWindowPrivate *priv = window->priv;

  priv->resize_cb = callback;
  priv->resize_data = data;
}

/* Must be called in the gl thread */
void
gst_gl_window_set_close_callback (GstGLWindow *window, GstGLWindowCB callback, gpointer data)
{
  GstGLWindowPrivate *priv = window->priv;

  priv->close_cb = callback;
  priv->close_data = data;
}

/* Thread safe */
void
gst_gl_window_visible (GstGLWindow *window, gboolean visible)
{
  GstGLWindowPrivate *priv = window->priv;

  g_debug ("set visible %lld\n", (guint64) priv->internal_win_id);

  if (visible)
    XMapWindow (priv->device, priv->internal_win_id);
  else
    XUnmapWindow (priv->device, priv->internal_win_id);

  XSync(priv->device, FALSE);
}

/* Thread safe */
void
gst_gl_window_draw (GstGLWindow *window)
{
  GstGLWindowPrivate *priv = window->priv;
  XEvent event;
  XWindowAttributes attr;

  XGetWindowAttributes (priv->device, priv->internal_win_id, &attr);

  event.xexpose.type = Expose;
  event.xexpose.send_event = TRUE;
  event.xexpose.display = priv->device;
  event.xexpose.window = priv->internal_win_id;
  event.xexpose.x = attr.x;
  event.xexpose.y = attr.y;
  event.xexpose.width = attr.width;
  event.xexpose.height = attr.height;
  event.xexpose.count = 0;
  XSendEvent (priv->device, priv->internal_win_id, FALSE, NoEventMask, &event);
}

void
gst_gl_window_run_loop (GstGLWindow *window)
{
  GstGLWindowPrivate *priv = window->priv;
  gboolean running = TRUE;
  XEvent event;

  g_debug ("begin loop\n");

  while (running)
  {
    XNextEvent(priv->device, &event);

    switch (event.type)
    {
      case ClientMessage:
      {

        if (event.xclient.message_type == priv->atom_custom)
        {

          if (priv->is_closed && priv->close_cb)
          {
            priv->close_cb (priv->close_data);
            g_debug ("is closed\n");
          }
          else
          {
            GstGLWindowCB custom_cb = (GstGLWindowCB) event.xclient.data.l[0];
            gpointer custom_data = (gpointer) event.xclient.data.l[1];
            custom_cb (custom_data);
          }

          g_cond_signal (priv->cond_send_message);
        }
        else if ( (Atom) event.xclient.data.l[0] == priv->atom_delete_window)
        {
          g_debug ("Close\n");
          priv->is_closed = TRUE;
          running = FALSE;
        }
        break;
      }

      case CreateNotify:
      case ConfigureNotify:
      {
        gint width = event.xconfigure.width;
        gint height = event.xconfigure.height;
        if (priv->resize_cb)
          priv->resize_cb (priv->resize_data, width, height);
        break;
      }

      case DestroyNotify:
        g_debug ("DestroyNotify\n");
        break;

      case Expose:
        if (priv->draw_cb)
          priv->draw_cb (priv->draw_data);
        break;

      case VisibilityNotify:
      {
        switch (event.xvisibility.state)
        {
          case VisibilityUnobscured:
            if (priv->draw_cb)
              priv->draw_cb (priv->draw_data);
            break;

          case VisibilityPartiallyObscured:
            if (priv->draw_cb)
              priv->draw_cb (priv->draw_data);
            break;

          case VisibilityFullyObscured:
            break;

          default:
            g_debug("unknown xvisibility event: %d\n", event.xvisibility.state);
            break;
        }
        break;
      }

      default:
        break;

    }
  }

  g_debug ("end loop\n");
}

/* Thread safe */
void
gst_gl_window_quit_loop (GstGLWindow *window)
{
  if (window)
  {
    GstGLWindowPrivate *priv = window->priv;
    XEvent event;

    event.xclient.type = ClientMessage;
    event.xclient.send_event = TRUE;
    event.xclient.display = priv->device;
    event.xclient.window = priv->internal_win_id;
    event.xclient.message_type = 0;
    event.xclient.format = 32;
    event.xclient.data.l[0] = priv->atom_delete_window;

    XSendEvent (priv->device, priv->internal_win_id, FALSE, NoEventMask, &event);
    XFlush (priv->device);
  }
}

/* Thread safe */
void
gst_gl_window_send_message (GstGLWindow *window, GstGLWindowCB callback, gpointer data)
{
  if (window)
  {

    GstGLWindowPrivate *priv = window->priv;

    if (!priv->is_closed)
    {
      XEvent event;

      g_mutex_lock (priv->mutex);

      event.xclient.type = ClientMessage;
      event.xclient.send_event = TRUE;
      event.xclient.display = priv->device;
      event.xclient.window = priv->internal_win_id;
      event.xclient.message_type = priv->atom_custom;
      event.xclient.format = 32;
      event.xclient.data.l[0] = (long) callback;
      event.xclient.data.l[1] = (long) data;

      XSendEvent (priv->device, priv->internal_win_id, FALSE, NoEventMask, &event);
      XFlush (priv->device);

      g_cond_wait (priv->cond_send_message, priv->mutex);

      g_mutex_unlock (priv->mutex);
    }
  }
}