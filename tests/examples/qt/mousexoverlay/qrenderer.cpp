/*
 * GStreamer
 * Copyright (C) 2008-2009 Julien Isorce <julien.isorce@gmail.com>
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

#include "qrenderer.h"

QRenderer::QRenderer(const QString videoLocation, QWidget *parent, Qt::WFlags flags)
    : QWidget(parent, flags),
      m_gt(winId(), videoLocation)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setVisible(false);
    move(20, 10);
    resize(640, 480);

    QObject::connect(&m_gt, SIGNAL(finished()), this, SLOT(close()));
    QObject::connect(this, SIGNAL(exposeRequested()), &m_gt, SLOT(exposeRequested()));
    QObject::connect(this, SIGNAL(closeRequested()), &m_gt, SLOT(stop()), Qt::DirectConnection);
    QObject::connect(&m_gt, SIGNAL(showRequested()), this, SLOT(show()));
    QObject::connect(this, SIGNAL(mouseMoved()), &m_gt, SLOT(onMouseMove()));
    m_gt.start();
}

QRenderer::~QRenderer()
{
}

void QRenderer::paintEvent(QPaintEvent* event)
{
    emit exposeRequested();
}

void QRenderer::mouseMoveEvent(QMouseEvent* event)
{
    emit mouseMoved();
}

void QRenderer::closeEvent(QCloseEvent* event)
{
    emit closeRequested();
    m_gt.wait();
}
