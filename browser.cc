/*
 * $Id$
 */

#include <stdio.h>

#include <QWidget>
#include <QTimer>

#include "browser.hh"


/* ---------- */

AsyncBrowser::AsyncBrowser(QWidget *parent) : QWidget(parent) {
    _renderTimer.setSingleShot(true);
    _renderTimer.setInterval(0);
    QObject::connect(&_renderTimer, SIGNAL(timeout()), this, SLOT(render()));

    _animateTimer.setInterval(30);
    QObject::connect(&_animateTimer, SIGNAL(timeout()), this, SLOT(animate()));
}

AsyncBrowser::~AsyncBrowser(void) {
    _renderTimer.stop();
    _animateTimer.stop();
}

void AsyncBrowser::doAnimate(bool doit) {
    printf("** doAnimate(%u)\n", (char)doit);
    if (doit)
        _animateTimer.start();
    else
        _animateTimer.stop();
}

void AsyncBrowser::doRender(void) {
    printf("** doRender\n");
   _renderTimer.start();
}



