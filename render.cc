/*
 * $Id$
 */

#include <stdio.h>

#include <QWidget>
#include <QTimer>

#include "logger.hh"
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
    LOG.puke("doAnimate(%u)", (char)doit);
    if (doit)
        _animateTimer.start();
    else
        _animateTimer.stop();
}

void AsyncRender::doRender(void) {
    LOG.puke("** doRender");
   _renderTimer.start();
}

bool AsyncRender::animating(void) const {
    bool b = _animateTimer.isActive();
    LOG.puke("** animating: %u", b);
    return b;
}


