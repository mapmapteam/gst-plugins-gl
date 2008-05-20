#ifndef QRENDERER_H
#define QRENDERER_H

#include <QtGui/QWidget>

class QRenderer : public QWidget
{
    Q_OBJECT

public:
    QRenderer(QWidget *parent = 0, Qt::WFlags flags = 0);
    ~QRenderer();
    void paintEvent(QPaintEvent* event);

signals:
    void exposeRequested();
};

#endif // QRENDERER_H