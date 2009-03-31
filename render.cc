/*
 * $Id$
 */

#include <stdio.h>

#include <QWidget>
#include <QTimer>

#include "render.hh"


/* ---------- */

AsyncRender::AsyncRender(QWidget *parent) : QWidget(parent) {
    _renderTimer.setSingleShot(true);
    _renderTimer.setInterval(0);
    QObject::connect(&_renderTimer, SIGNAL(timeout()), this, SLOT(render()));

    _animateTimer.setInterval(30);
    QObject::connect(&_animateTimer, SIGNAL(timeout()), this, SLOT(animate()));
}

AsyncRender::~AsyncRender(void) {
    _renderTimer.stop();
    _animateTimer.stop();
}

void AsyncRender::doAnimate(bool doit) {
    printf("** doAnimate(%u)\n", (char)doit);
    if (doit)
        _animateTimer.start();
    else
        _animateTimer.stop();
}

void AsyncRender::doRender(void) {
    printf("** doRender\n");
   _renderTimer.start();
}



