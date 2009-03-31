#ifndef PF_RENDER_HH
#define PF_RENDER_HH

/*
 * $Id$
 */

#include <stdint.h>

#include <QWidget>
#include <QTimer>

#include "math.hh"

/*
 * Generic object for asynchronous animation.
 */

class AsyncRender : public QWidget {
    Q_OBJECT;

 private:

    QTimer _animateTimer, _renderTimer;

 protected slots:

    /*
     * The actual methods that do the work (called asynchronously).
     */

    virtual void animate(void) = 0;
    virtual void render(void) = 0;

 public:

    AsyncRender(QWidget *parent = 0);
    ~AsyncRender(void);

    void doAnimate(bool = true);
    void doRender(void);
};

/* --------- */
/* --------- */
/* --------- */




#endif
