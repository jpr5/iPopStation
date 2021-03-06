#ifndef PF_RENDER_HH
#define PF_RENDER_HH

/*
 * $Id$
 */

#include <stdint.h>

#include <QWidget>
#include <QTimer>

/*
 * Generic object for asynchronous animation.
 */

class AsyncRender : public QWidget {
    Q_OBJECT;

 private:

    QTimer _animateTimer, _renderTimer;

 protected:

    /*
     * Video drawing double-buffer.
     */

    QImage buffer;

    /*
     * QWidget events hooks.
     */

    virtual void paintEvent(QPaintEvent *);

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

    bool animating(void) const;
};

/* --------- */
/* --------- */
/* --------- */




#endif
