/*
 * $Id$
 */

#include <QDir>
#include <QFileInfo>
#include <QSplashScreen>
#include <QPainter>
#include <QResizeEvent>

#include "ps.hh"
#include "logger.hh"
#include "album.hh"

/*
 * TODO [WISHLIST]
 *
 *   - mousePressEvent/whatever: the active rect for detecting album
 *     display clicks should be updated as the (target) album is be
 *     animated.
 */

/*
 * Angle of non-focused covers.

 *   Size of viewport impacts visibility, and resizing of viewport
 *   only matters in test environment.
 *
 *   80 is cool; 60 is too flat, 110/120 is too much.
 */

const uint16_t tilt_factor    = 80 * IANGLE_MAX / 360;
const uint16_t spacing_offset = 60;                     // space between each angled cover
const uint32_t f_max          = 2 * 65536;

/* ---------- */

AlbumCover::AlbumCover(void) {
    angle = 0;
    cx = cy = 0;
}

AlbumCover::AlbumCover(const QImage &image_, const QString &path_) {
    angle = 0;
    cx = cy = 0;
    image = image_;
    path = path_;
}

AlbumCover::AlbumCover(const AlbumCover &a) {
    image = a.image;
    path  = a.path;
    angle = a.angle;
    cx    = a.cx;
    cy    = a.cy;
}

const AlbumCover &AlbumCover::operator=(const AlbumCover &a) {
    image = a.image;
    path  = a.path;
    angle = a.angle;
    cx    = a.cx;
    cy    = a.cy;
    return *this;
}

/*
 * Process the image: scale the image to the cover size, and calculate
 * its reflection.  Angle data should be set by collection owner.
 *
 * TODO: optimize memory usage and speed
 * TODO: figure a better way to always maximize the album view-size;
 *       eliminate top-padding hack
 * TODO: (in collection) redraw entire group every time an album is
 *       processed/added? (assumes delayed processing, which is not
 *       the case right now)
 */

void AlbumCover::process(uint16_t c_width, uint16_t c_height) {
    LOG.puke("process(%u, %u)", c_width, c_height);

    image = image.scaled(c_width, c_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    /*
     * Calculate the minumum size(height) of the reflection we want to
     * put into the image and allocate.
     */

    uint16_t reflection_height = c_height*2/3;
    uint16_t total_height      = c_height + 1 + reflection_height;

    QImage out(c_width, total_height, QImage::Format_RGB32);
    QPainter p(&out);

    p.drawImage(0, 0, image);
    p.translate(0, c_height+1);
    p.drawImage(0, 0, image.mirrored(false, true), 0, 0, c_width, reflection_height);

    p.end();

    /*
     * Faux alpha blend the bottom vertically-flipped portion.
     */

    uint16_t stop = out.size().height();
    uint32_t *px;
    uint8_t r, g, b, f;

    for (uint16_t y = c_height; y < stop; y++) {
        px = (uint32_t*)out.scanLine(y);
        f = (stop-y)*100/stop;

        for (uint16_t x = 0; x < c_width; x++) {
            r = qRed(px[x])   * f / 100;
            g = qGreen(px[x]) * f / 100;
            b = qBlue(px[x])  * f / 100;
            px[x] = qRgb(r, g, b);
        }
    }

    image = out;
}

/* ------------- */
/* ------------- */
/* ------------- */
/* ------------- */

AlbumBrowser::AlbumBrowser(QWidget *parent) : AsyncRender(parent) {
    LOG.puke("albumBrowser");
    c_zoom   = 100;
    c_width  = 135;
    c_height = 175;

    f_frame     = 0;
    f_direction = 0;

    d_mode = M_BROWSE;
}

AlbumBrowser::~AlbumBrowser(void) {
}

bool AlbumBrowser::init(void) {
#if TEST
    QSize screenSize(800, 400);
#else
    QSize screenSize(320, 240);
#endif

    /*
     * Splash screen.
     */

    QPixmap pixmap(screenSize);
    pixmap.fill(Qt::black);

    QSplashScreen splash(pixmap);
    splash.show();
    splash.showMessage("Booting...", Qt::AlignLeft, Qt::white);

    QDir dir = QDir::current();
    if (!dir.cd("pics"))
        LOG.warn("unable to cd to pics dir, using current");

    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList list = dir.entryInfoList();
    if (list.empty()) {
        LOG.error("no pics in dir %s", dir.path().toAscii().data());
        return false;
    }

    foreach (QFileInfo i, list) {
        QString msg = "Loaded %1", file = i.absoluteFilePath();

        if (addCover(file)) {
            LOG.puke("loaded %s", (const char *)file.toAscii());
            splash.showMessage(msg.arg(file), Qt::AlignLeft, Qt::white);
        }
    }

    splash.finish(this);

    /*
     * Set some dimensions and title.
     */

    setCoverSize(QSize(130,175));
    setWindowTitle("PopStation");
    resize(screenSize);

    return true;
}


void AlbumBrowser::displayAlbum(void) {
    /*
     * Make a smaller copy of the cover's image and un-distort it to
     * simplify drawing.
     */

    bg    = buffer.copy();
    cover = currentCover().image.copy(0, 0, cover.size().width(), cover.size().height());

    /*
     * Calculate initial position on-screen (should mimic whatever
     * renderBrowse() would do for c_focus), and derive x/y slope
     * components + x_incr for each animation transition (10% per
     * iteration).
     */

    QSize cs = coverSize();

    d_albumx = (buffer.size().width() - cs.width())   / 2;
    d_albumy = (buffer.size().height() - cs.height()) / 2;

    /*
     * Initial target is upper-left 3x3 px margin, but the scale/size
     * and placement should be based on the overall size of the buffer
     * and margins eventually around everything.
     */

    d_targetx = d_targety = 5;

    d_sy = d_albumy - d_targety;
    d_sx = d_albumx - d_targetx;
    d_dx = d_sx / 10;
}

void AlbumBrowser::arrangeCovers(int32_t factor) {
    AlbumCover *a;

    /*
     * factor != 0 is a transition state; when 0 there is no
     * transition and we should reset whatever ray info might
     * previously exist.
     */

    if (factor == 0) {
        a = &covers[c_focus];
        a->angle = 0;
        a->cx    = 0;
        a->cy    = 0;

        f_frame  = c_focus << 16;
    }

    for (int16_t i = c_focus - 1; i != -1; i--) {
        a = &covers[i];
        a->angle = tilt_factor;
        a->cx    = -(r_offsetX + (spacing_offset*(c_focus-1-i)*FPreal_ONE) + factor);
        a->cy    = r_offsetY;

        LOG.puke("cover[%u] = %i, %i, %i", i, a->angle, a->cx, a->cy);
    }

    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        a = &covers[i];
        a->angle = -tilt_factor;
        a->cx    = r_offsetX + (spacing_offset*(i-c_focus-1)*FPreal_ONE) - factor;
        a->cy    = r_offsetY;

        LOG.puke("cover[%u] = %i, %i, %i", i, a->angle, a->cx, a->cy);
    }
}

void AlbumBrowser::prepRender(bool reset) {
    LOG.puke("prepRender(%u)", reset);

    uint16_t width  = (buffer.size().width()  + 1) / 2;
    uint16_t height = (buffer.size().height() + 1) / 2;

    rays.resize(width * 2);
    for (uint16_t i = 0; i < width; i++) {
        FPreal_t gg = (FPreal_HALF + i * FPreal_ONE) / (2 * height);
        rays[width-i-1] = -gg;
        rays[width+i]   =  gg;
    }

    r_offsetX =
        ((c_width / 2) * (FPreal_ONE - fcos(tilt_factor))) +
        (c_width * FPreal_ONE);

    r_offsetY =
        ((c_width / 2) * fsin(tilt_factor)) +
        (c_width * FPreal_ONE / 4);

    if (reset)
        c_focus = covers.size()/2;

    arrangeCovers();
}

void AlbumBrowser::render(void) {
    LOG.puke("render");

    switch (d_mode) {
        case M_BROWSE: {
            renderBrowse();
        } break;
        case M_DISPLAY: {
            renderDisplay();
        } break;
    };

    /*
     * Then tell the Widget to update itself at next opportunity.
     */

    QWidget::update();
}

void AlbumBrowser::renderDisplay(void) {
    LOG.puke("** renderDisplay");

    /*
     * Faux-fade the background
     * Draw album cover
     *
     * TODO: we may need to scale the image down on smaller interfaces
     * to leave room for other stuff.  If so, should be some % of
     * current size, maintaining existing aspect ratio.
     */

    QPainter p(&buffer);

    /*
     * Currently transitioning.
     */
    buffer.fill(Qt::black);

#if TEST
    uint16_t x_lim = qMin(buffer.size().width(), bg.size().width());
    uint16_t y_lim = qMin(buffer.size().height(), bg.size().height());

    uint32_t *px_in, *px_out;
    uint8_t r, g, b, f = d_albumx * 100 / d_sx;

    for (uint16_t y = 0; y < y_lim; y++) {
        px_in  = (uint32_t*)bg.scanLine(y);
        px_out = (uint32_t*)buffer.scanLine(y);

        for (uint16_t x = 0; x < x_lim; x++) {
            if (px_in[x] == 0xFF000000)
                continue;

            r = qRed(px_in[x])   * f / 100;
            g = qGreen(px_in[x]) * f / 100;
            b = qBlue(px_in[x])  * f / 100;
            px_out[x] = qRgb(r,g,b);
        }
    }
#endif

    if (d_albumx == d_targetx && d_albumy == d_targety) {

        /*
         * This is our "final" render, animation should be in position
         * so draw the polish & navigation elements.
         *
         * TODO: come up with a better way to "initialize" drawing of
         * static elements that we don't have to worry about being
         * overwritten by the manual animation (wish we had sprites).
         */

        QPen border_pen(Qt::blue, 2, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
        QPen text_pen(Qt::white, 1, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);

        QRect rect(2, 2, buffer.size().width()-3, buffer.size().height()-3);
        p.setPen(border_pen);
        p.drawRect(rect);

        QFont font("Times", 12, QFont::Normal);
        p.setPen(text_pen);
        p.setFont(font);
        p.drawText(c_width + 50, 50, "You all should suckit, bitches.");
    }

    /*
     * Finally, draw the album cover.
     */

    p.drawImage(QPoint(d_albumx, d_albumy), cover);
}

void AlbumBrowser::renderBrowse(void) {
    LOG.puke("** renderBrowse");

    /*
     * Clean out the off-screen buffer and start with the in-focus
     * cover.
     */

    buffer.fill(Qt::black);

    uint16_t x_bound;
    QRect r, rc;

    r = renderCover(covers[c_focus]);
    LOG.puke("initial bound: [%u, %u]", r.left(), r.right());

    /*
     * Then render all remaining covers, left-side right-to-left, and
     * right-side left-to-right.
     */

    x_bound = r.left();
    for (int16_t i = c_focus - 1; i != -1; i--) {
        LOG.puke("rendering cover %i", i);
        rc = renderCover(covers[i], 0, x_bound-1);
        if (rc.isEmpty()) {
            LOG.puke("didn't render cover %u, stopping", i);
            break;
        }

        x_bound = rc.left();
    }

    x_bound = r.right();
    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        LOG.puke("rendering cover %i", i);
        rc = renderCover(covers[i], x_bound+1, buffer.width());
        if (rc.isEmpty()) {
            LOG.puke("didn't render cover %u, stopping", i);
            break;
        }

        x_bound = rc.right();
    }
}

QRect AlbumBrowser::renderCover(AlbumCover &a, int16_t lb, int16_t rb) {
    LOG.puke("renderCover(%i, %i)", lb, rb);

    QRect rect(0, 0, 0, 0);
    QImage &src = a.image;

    int16_t sw = src.width();
    int16_t sh = src.height();
    int16_t h = buffer.height();
    int16_t w = buffer.width();

    if (lb > rb)
        qSwap(lb, rb);

    lb = (lb >= 0) ? lb : 0;
    rb = (rb >= 0) ? rb : w-1;
    lb = qMin(lb, (int16_t)(w-1));
    rb = qMin(rb, (int16_t)(w-1));

    if (lb - rb == 0) {
        LOG.puke("not rendering invisible slide");
        return rect;
    }

    FPreal_t sdx = fcos(a.angle);
    FPreal_t sdy = fsin(a.angle);
    FPreal_t xs = a.cx - sw * sdx/2;
    FPreal_t ys = a.cy - sw * sdy/2;

    int32_t distance = h * 100 / c_zoom;
    FPreal_t dist = distance * FPreal_ONE;

    int32_t xi = qMax((FPreal_t)0, FPreal_CAST((w*FPreal_ONE/2) + fdiv(xs*h, dist+ys)));
    if (xi >= w)
        return rect;

    LOG.puke("** [ %i ]   %i   [ %i ]", lb, xi, rb);

    bool flag = false;
    rect.setLeft(xi);

    for (uint16_t x = qMax(xi, (int)lb); x <= rb; x++) {
        FPreal_t hity = 0;
        FPreal_t fk = rays[x];
        if (sdy) {
            fk  -= fdiv(sdx,sdy);
            hity = -fdiv((rays[x]*distance - a.cx + a.cy*sdx/sdy), fk);
        }

        dist = distance*FPreal_ONE + hity;
        if (dist < 0)
            continue;

        FPreal_t hitx = fmul(dist, rays[x]);
        FPreal_t hitdist = fdiv(hitx - a.cx, sdx);

        int16_t column = sw/2 + FPreal_CAST(hitdist);
        if (column >= sw)
            break;
        if (column < 0)
            continue;

        rect.setRight(x);
        if (!flag)
            rect.setLeft(x);
        flag = true;

        /*
         * Start drawing covers to the middle buffer, with a slight
         * offset (.3 of cover height).
        */

        int16_t out_y1  = h/2;
        int16_t out_y2  = out_y1 + 1;
        QRgb *out_px1   = (QRgb*)(buffer.scanLine(out_y1)) + x;
        QRgb *out_px2   = (QRgb*)(buffer.scanLine(out_y2)) + x;
        QRgb out_pxstep = out_px2 - out_px1;

        /*
         * Start drawning from center of cover size (rather than image
         * size), which assumes no padding but still works if there's
         * other stuff (like a reflection) beneath.
         *
         * dy is a fixed-point fractional "tick" inc/decrement that
         * translates input y coords to output y coords
         * (bendy/stretchy effect).
         */

        int16_t dy     = dist / h;

        int32_t in_x   = column;
        int32_t in_y1  = c_height/2;
        int32_t in_y2  = in_y1 + 1;
        int32_t in_p1  = in_y1*FPreal_ONE - dy/2;
        int32_t in_p2  = in_y2*FPreal_ONE + dy/2;
        QRgb *in_px1   = (QRgb*)(src.scanLine(in_y1)) + in_x;
        QRgb *in_px2   = (QRgb*)(src.scanLine(in_y2)) + in_x;
        QRgb in_pxstep = in_px2 - in_px1;

        /*
         * Loop over drawing, knowing that it's probably that we'll
         * hit one end of a cover's scanlines before the other (top
         * vs. bottom).
         */

        uint16_t tick;
        bool y1_room, y2_room;

        do {
            y1_room = (in_y1 >= 0 && out_y1 >= 0);
            y2_room = (in_y2 < sh && out_y2 < h);

            if (y1_room) {
                *out_px1 = *in_px1;
                out_y1--;
                out_px1 -= out_pxstep;
            }

            if (y2_room) {
                *out_px2 = *in_px2;
                out_y2++;
                out_px2 += out_pxstep;
            }

            in_p1 -= dy;
            in_p2 += dy;

            tick = abs(FPreal_CAST(in_p1) - in_y1);
            if (tick != 0) {
                in_y1  -= tick;
                in_y2  += tick;
                in_px1 -= in_pxstep*tick;
                in_px2 += in_pxstep*tick;
            }

        } while (y1_room || y2_room);
    }

    rect.setTop(0);
    rect.setBottom(h-1);

    return rect;
}

void AlbumBrowser::animate(void) {
    LOG.puke("** animate");

    switch (d_mode) {
        case M_BROWSE: {
            animateBrowse();
        } break;
        case M_DISPLAY: {
            animateDisplay();
        } break;
    };
}

void AlbumBrowser::animateDisplay(void) {
    LOG.puke("** animateDisplay");

    /*
     * For now, transition album in 10% increments on X, using the
     * slope to derive Y.
     */

    d_albumx = qMax(d_albumx - d_dx, (int)d_targetx);
    d_albumy = qMax(d_sy * d_albumx / d_sx, (int)d_targety);

    LOG.debug("animate: [-%u] d_albumx = %u, d_albumy = %u", d_dx, d_albumx, d_albumy);

    if (d_albumx == d_targetx && d_albumy == d_targety)
        doAnimate(false);

    doRender();
}

void AlbumBrowser::animateBrowse(void) {
    LOG.puke("** animateBrowse");

    int16_t c_target = c_focus + f_direction;

    /*
     * If target is beyond bounds (e.g. kept clicking left), then let
     * the animation finish by setting the target cover to whichever
     * is the current focus, and set the current frame to it
     * (effectively a non-animated render of c_focus).
     */

    if (c_target < 0 || c_target == covers.size()) {
        c_target = c_focus;
        f_frame  = c_target << 16;
    }

    /*
     * Calculate the next "frame" increment and update f_frame.
     */

    uint32_t f_diff   = qMin( abs(f_frame - (c_target<<16)), (int)f_max);
    uint32_t f_iangle = IANGLE_MAX * (f_diff-f_max/2) / (f_max*2);
    uint32_t f_incr   = 512 + (16384 * (FPreal_ONE+fsin(f_iangle))/FPreal_ONE);

    f_frame += f_incr * f_direction;

    LOG.puke("[%u] angle = %i (%i), incr = %i (+%i @%i)", c_focus, f_iangle, f_iangle & IANGLE_MASK, f_incr, f_diff, f_frame);

    /*
     * Update the raytrace data for the cover in transition.
     *
     * Frames reference a cover's lb, so (after f_incr'ing) moving
     * left will reference the next-left cover, which would be wrong.
     * When moving right, we don't have to worry about that.
     */

    int32_t c_idx  = (f_frame >> 16) + (f_direction < 0);
    int32_t pos    = f_frame & 0xffff;
    int32_t neg    = 65536 - pos;
    int32_t  tick  = (f_direction < 0) ? neg : pos;
    FPreal_t ftick = (tick * FPreal_ONE) >> 16;

    LOG.puke("[%i -> %i] pos = %i, neg = %i, tick = %i, ftick = %i", c_idx, c_target, pos, neg, tick, ftick);

    AlbumCover *a = &(covers[c_idx]);
    a->angle = (f_direction * tick * tilt_factor) >> 16;
    a->cx    = -f_direction * fmul(r_offsetX, ftick);
    a->cy    = fmul(r_offsetY, ftick);

    /*
     * If we have arrived, then just reset everything and display.
     */

    if (c_idx == c_target) {

        doAnimate(false);

        c_focus     = c_idx;
        f_direction = 0;

        arrangeCovers();

    } else {

        /*
         * Otherwise, we're still transitioning.  Update cover angles.
         */

        int32_t factor = f_direction * spacing_offset * ftick;

        LOG.puke("animating (factor = %i)", factor);

        arrangeCovers(factor);

        if (f_direction > 0) {
            a        = &(covers[c_idx+1]);
            a->angle = -(neg * tilt_factor) >> 16;
            ftick    = (neg * FPreal_ONE) >> 16;
            a->cx    = fmul(r_offsetX, ftick);
            a->cy    = fmul(r_offsetY, ftick);
        } else {
            a        = &(covers[c_idx-1]);
            a->angle = (pos * tilt_factor) >> 16;
            ftick    = (pos * FPreal_ONE) >> 16;
            a->cx    = -fmul(r_offsetX, ftick);
            a->cy    = fmul(r_offsetY, ftick);
        }
    }

    doRender();
}

bool AlbumBrowser::addCover(const QString &path_) {
    QImage image_;

    if (!image_.load(path_)) {
        LOG.error("unable to load %s", (const char*)path_.toAscii());
        return false;
    }

    LOG.puke("loaded cover %s", (const char*)path_.toAscii());
    addCover(image_, path_);

    return true;
}

/*
 * Might want to delay "processing" until the image is shown?
 */

void AlbumBrowser::addCover(const QImage &image_, const QString &path_) {
    AlbumCover a(image_, path_);
    a.process(c_width, c_height);
    covers.push_back(a);
}

void AlbumBrowser::loadCovers(QList<QString> &covers) {
    covers.clear();

    QImage image;
    foreach (QString filename, covers) {
        if (image.load(filename)) {
            LOG.debug("loaded cover %s", (const char *)filename.toAscii());
            addCover(image, filename);
        }
    }
}

void AlbumBrowser::setCoverSize(QSize s) {
    LOG.puke("setCoverSize(%u, %u)", s.width(), s.height());

    if (s.width() == c_width && s.height() == c_height)
        return;

    c_width  = s.width();
    c_height = s.height();
}

QSize AlbumBrowser::coverSize(void) {
    return QSize(c_width, c_height);
}

const AlbumCover &AlbumBrowser::currentCover(void) {
    LOG.puke("currentCover");

    return covers[c_focus];
}

/*
 * (Re)size.  Guaranteed one of these on startup.
 */

void AlbumBrowser::resizeView(const QSize &s, bool reset) {
    LOG.puke("resizeView(%u, %u)", s.width(), s.height());

    /*
     * No point in recalculating anything if the size isn't changing.
     */

    if (buffer.size() == s)
        return;

    buffer = QImage(s, QImage::Format_RGB32);
    buffer.fill(Qt::black);

    d_lb = (size().width() / 2) - (c_width / 2);
    d_rb = d_lb + c_width;

    LOG.puke("d_lb = %u, d_rb = %u", d_lb, d_rb);

    /*
     * Regardless of d_mode, we need to update the ray info --
     * otherwise, if we're displaying an album then returning from
     * that will render with the old size values.
     */

    prepRender(reset);

    render();
}

void AlbumBrowser::resizeEvent(QResizeEvent *e) {
    LOG.puke("@@ resizeEvent: [%i:%i] -> [%i:%i]",
           e->oldSize().width(), e->oldSize().height(),
           e->size().width(),    e->size().height());

    /*
     * If we're being resized for the first time, then "reset" c_focus
     * and f_frame (happens in prepRender()).
     */

    bool reset = (e->oldSize().width() == -1 && e->oldSize().height() == -1);

    this->resizeView(e->size(), reset);

    QWidget::resizeEvent(e);
}

void AlbumBrowser::mousePressEvent(QMouseEvent *e) {
    LOG.debug("@@ mousePressEvent[%u:%u]", e->x(), e->y());

    switch (d_mode) {

        case M_BROWSE: {
            if (e->x() <= d_lb) {

                /*
                 * If we still have room, and we're already moving
                 * left, bring the next left cover into focus.
                 */

                if (c_focus > 0)
                    if (f_direction < 0)
                        c_focus--;
                    else
                        f_direction = -1;

            } else if (e->x() >= d_rb) {

                /*
                 * If we still have room, and we're already moving
                 * right, bring the next right cover into focus.
                 */

                if (c_focus < covers.size()-1)
                    if (f_direction > 0)
                        c_focus++;
                    else
                        f_direction = 1;

            } else {

                /*
                 * Album was clicked.
                 */

                if (animating()) {
                    doAnimate(0);

                    c_focus    += f_direction;
                    f_direction = 0;

                    arrangeCovers();
                }

                d_mode = M_DISPLAY;
                displayAlbum();
            }

            doAnimate();

        } break;

        case M_DISPLAY: {
            d_mode      = M_BROWSE;
            bg = cover = QImage();

            doRender();
        } break;
    };

}

/* -------------------------- */
/* -------------------------- */

