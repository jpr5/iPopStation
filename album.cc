/*
 * $Id$
 */

#include <QPainter>
#include <QResizeEvent>

#include "ps.hh"
#include "logger.hh"
#include "album.hh"

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

    image = image.scaled(c_width, c_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(true, false);
    uint16_t padding = c_height/3;

    QImage out(c_width, c_height*2, QImage::Format_RGB32);
    QPainter p(&out);

    p.translate(0, padding);
    p.drawImage(0, 0, image);
    p.translate(0, c_height);
    p.drawImage(0, 0, image.mirrored(false, true));

    p.end();

    /*
     * Pull out RGB values and make a faux transparency.
     *
     * Don't fuck with this syntax, the transient float (% strength)
     * will stop working. :-(
     */

    uint16_t stop = out.size().height();

    uint32_t *px;
    uint8_t r, g, b;

    for (uint16_t y = padding+c_height; y < stop; y++) {
        px = (uint32_t*)out.scanLine(y);
        for (uint16_t x = 0; x < c_width; x++) {
            r = qRed(px[x])   * (stop - y) / stop;
            g = qGreen(px[x]) * (stop - y) / stop;
            b = qBlue(px[x])  * (stop - y) / stop;

            px[x] = qRgb(r, g, b);
        }
    }

    image = out.transformed(QMatrix().rotate(270));
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

    f_frame = 0;
    f_direction = 0;
}

AlbumBrowser::~AlbumBrowser(void) {
}

void AlbumBrowser::arrangeCovers(int32_t factor) {

    /*
     * TODO: Optimize by calculating these values once and assigning
     * +/- value outward from center (c_focus).  Not a big win on
     * large displays + large lists.
     */

    for (int16_t i = c_focus - 1; i != -1; i--) {
        AlbumCover &a = covers[i];
        a.angle = tilt_factor;
        a.cx    = -(r_offsetX + (spacing_offset*(c_focus-1-i)*FPreal_ONE) + factor);
        a.cy    = r_offsetY;

        LOG.puke("cover[%u] = %i, %i, %i", i, a.angle, a.cx, a.cy);
    }

    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        AlbumCover &a = covers[i];
        a.angle = -tilt_factor;
        a.cx    = r_offsetX + (spacing_offset*(i-c_focus-1)*FPreal_ONE) - factor;
        a.cy    = r_offsetY;

        LOG.puke("cover[%u] = %i, %i, %i", i, a.angle, a.cx, a.cy);
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

    if (reset) {
        c_focus = covers.size()/2;
        f_frame = c_focus << 16;
    }

    arrangeCovers();
}

void AlbumBrowser::render(void) {
    LOG.puke("render");

    /*
     * Clean out the off-screen buffer and start with the in-focus
     * cover.
     */

    buffer.fill(Qt::black);

    LOG.debug("c_focus = %u (of %u) [%i]", c_focus, covers.size(), f_frame);

    uint16_t x_bound;
    QRect r, rs;

    r = renderCover(covers[c_focus]);
    LOG.puke("initial bound: [%u, %u]", r.left(), r.right());

    /*
     * Then render all remaining covers, left-side right-to-left, and
     * right-side left-to-right.
     */

    x_bound = r.left();
    for (int16_t i = c_focus - 1; i != -1; i--) {
        LOG.puke("rendering cover %i", i);
        rs = renderCover(covers[i], 0, x_bound-1);
        if (rs.isEmpty()) {
            LOG.puke("didn't render cover %u, stopping", i);
            break;
        }

        x_bound = rs.left();
    }

    x_bound = r.right();
    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        LOG.puke("rendering cover %i", i);
        rs = renderCover(covers[i], x_bound+1, buffer.width());
        if (rs.isEmpty()) {
            LOG.puke("didn't render cover %u, stopping", i);
            break;
        }

        x_bound = rs.right();
    }

    /*
     * Then tell the Widget to update itself at next opportunity.
     */

    QWidget::update();
}

QRect AlbumBrowser::renderCover(AlbumCover &a, int16_t lb, int16_t rb) {
    LOG.puke("renderCover(%i, %i)", lb, rb);

    QRect rect(0, 0, 0, 0);
    QImage &src = a.image;

    int sw = src.height();
    int sh = src.width();
    int h = buffer.height();
    int w = buffer.width();

    if (lb > rb)
        qSwap(lb, rb);

    lb = (lb >= 0) ? lb : 0;
    rb = (rb >= 0) ? rb : w-1;
    lb = qMin((int)lb, w-1);
    rb = qMin((int)rb, w-1);

    if (lb - rb == 0) {
        LOG.puke("not rendering invisible slide");
        return rect;
    }

    int distance = h * 100 / c_zoom;
    FPreal_t sdx = fcos(a.angle);
    FPreal_t sdy = fsin(a.angle);
    FPreal_t xs = a.cx - c_width * sdx/2;
    FPreal_t ys = a.cy - c_width * sdy/2;
    FPreal_t dist = distance * FPreal_ONE;

    //                      start from middle  +/- distance
    int xi = qMax((FPreal_t)0, FPreal_CAST((w*FPreal_ONE/2) + fdiv(xs*h, dist+ys)));
    if (xi >= w)
        return rect;

    LOG.puke("[ %i ]   %i   [ %i ]", lb, xi, rb);

    bool flag = false;
    rect.setLeft(xi);

    for (int x = qMax(xi, (int)lb); x <= rb; x++) {
        FPreal_t hity = 0;
        FPreal_t fk = rays[x];
        if (sdy) {
            fk = fk - fdiv(sdx,sdy);
            hity = -fdiv((rays[x]*distance - a.cx + a.cy*sdx/sdy), fk);
        }

        dist = distance*FPreal_ONE + hity;
        if (dist < 0)
            continue;

        FPreal_t hitx = fmul(dist, rays[x]);
        FPreal_t hitdist = fdiv(hitx - a.cx, sdx);

        int column = sw/2 + FPreal_CAST(hitdist);
        if (column >= sw)
            break;
        if (column < 0)
            continue;

        rect.setRight(x);
        if (!flag)
            rect.setLeft(x);
        flag = true;

        int y1 = h/2;
        int y2 = y1+ 1;
        QRgb *pixel1 = (QRgb*)(buffer.scanLine(y1)) + x;
        QRgb *pixel2 = (QRgb*)(buffer.scanLine(y2)) + x;
        QRgb pixelstep = pixel2 - pixel1;

        int center = (sh/2);
        int dy = dist / h;
        int p1 = center*FPreal_ONE - dy/2;
        int p2 = center*FPreal_ONE + dy/2;

        const QRgb *ptr = (const QRgb*)(src.scanLine(column));
        while((y1 >= 0) && (y2 < h) && (p1 >= 0)) {
            *pixel1 = ptr[FPreal_CAST(p1)];
            *pixel2 = ptr[FPreal_CAST(p2)];
            p1 -= dy;
            p2 += dy;
            y1--;
            y2++;
            pixel1 -= pixelstep;
            pixel2 += pixelstep;
        }
    }

    rect.setTop(0);
    rect.setBottom(h-1);

    return rect;
}

void AlbumBrowser::animate(void) {
    LOG.puke("animate");

    if (f_direction == 0) {
        LOG.warn("no directional change, bailing");
        doAnimate(false);
        return;
    }

    int16_t c_target = c_focus + f_direction;

    /*
     * If target is beyond bounds (e.g. kept clicking left), then let
     * the animation finish by setting the target cover to whichever
     * is the current focus, and set the current frame to it
     * (effectively a non-animated render of c_focus).
     */

    if (c_target < 0 || c_target >= covers.size()) {
        c_target = c_focus;
        f_frame  = c_target << 16;
    }

    int32_t f_idx = f_frame - (c_target << 16);
    if (f_idx < 0)
        f_idx = -f_idx;
    f_idx = qMin((int)f_idx, (int)f_max);

    // with f_idx bounded by f_max
    // IANGLE_MAX * (f_idx - half of max) / (twice the max)
    int32_t angle  = IANGLE_MAX * (f_idx-f_max/2) / (f_max*2);
    uint32_t speed = 512 + (16384 * (FPreal_ONE+fsin(angle))/FPreal_ONE);

    f_frame += speed * f_direction;

    LOG.puke("angle = %i, speed = %u, f_frame = %i (dir = %i)", angle, speed, f_frame, f_direction);

    int32_t c_idx  = f_frame >> 16;
    int32_t pos    = f_frame & 0xffff;
    int32_t neg    = 65536 - pos;
    int tick       = (f_direction < 0) ? neg : pos;
    FPreal_t ftick = (tick * FPreal_ONE) >> 16;

    LOG.puke("c_idx = %i, pos = %i, neg = %i, tick = %i, ftick = %i", c_idx, pos, neg, tick, ftick);

    if (f_direction < 0)
        c_idx++;

    LOG.puke("c_focus = %i (%i) [%i], c_target = %i", c_focus, c_idx, f_direction, c_target);

    AlbumCover *a = &(covers[c_idx]);
    a->angle = (f_direction * tick * tilt_factor) >> 16;
    a->cx    = -f_direction * fmul(r_offsetX, ftick);
    a->cy    = fmul(r_offsetY, ftick);

    /*
     * If we have arrived, then just reset everything and display.
     */

    if (c_idx == c_target) {

        c_focus     = c_idx;
        f_frame     = c_idx << 16;
        f_direction = 0;

        arrangeCovers();

        doAnimate(false);

    } else {

        /*
         * Otherwise, we're still transitioning.  Update cover angles.
         */

        int32_t factor = f_direction * spacing_offset * ftick;

        LOG.debug("animating (factor = %i)", factor);

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

    return;
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
    resetCovers();

    QImage image;
    foreach (QString filename, covers) {
        if (image.load(filename)) {
            LOG.debug("loaded cover %s", (const char *)filename.toAscii());
            addCover(image, filename);
        }
    }
}

void AlbumBrowser::resetCovers(void) {
    covers.clear();
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
    LOG.puke("currentCover\n");

    return covers[c_focus];
}

void AlbumBrowser::displayAlbum(void) {

    AlbumDisplay *ad = new AlbumDisplay(this);

    ad->setWindowTitle("Album Display");
    ad->resize(size());

    hide();

#if TEST
    ad->show();
#else
    ad->showFullScreen();
#endif
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

    /*
     * We'll get a paint event after this; we need to trigger our
     * double-buffered render before it comes, otherwise the first
     * paint will just show the empty buffer.
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

void AlbumBrowser::paintEvent(QPaintEvent *e) {
    LOG.puke("@@ paintEvent");
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QPoint(0, 0), this->buffer);
}

void AlbumBrowser::mousePressEvent(QMouseEvent *e) {
    LOG.debug("@@ mousePressEvent[%u:%u]", e->x(), e->y());

    /*
     * TODO: These values should be pre-calc'd.
     */

    uint16_t center_lb = (size().width() / 2) - (c_width / 2);
    uint16_t center_rb = center_lb + c_width;

    if (e->x() <= center_lb) {

        if (c_focus > 0)
            if (f_direction < 0 && animating())
                c_focus--;
            else
                f_direction = -1;

    } else if (e->x() >= center_rb) {

        if (c_focus < covers.size()-1)

            if (f_direction > 0 && animating())
                c_focus++;
            else
                f_direction = 1;

    } else {

        /*
         * FIXME: is all this necessary?  Still leaves browser's
         * mid-render display when browser is re-show()'d.
         */

        if (animating()) {
            doAnimate(0);
            c_focus += f_direction;
            f_frame = c_focus << 16;
            f_direction = 0;
            render();
        }

        displayAlbum();

    }

    if (f_direction)
        doAnimate();
}

const QImage &AlbumBrowser::displayBuffer(void) {
    return buffer;
}

/* -------------------------- */
/* -------------------------- */


AlbumDisplay::AlbumDisplay(AlbumBrowser *browser_) : AsyncRender(NULL) {
    LOG.debug("albumDisplay");

    QWidget::setAttribute(Qt::WA_DeleteOnClose);

    browser = browser_;
    bg      = browser->displayBuffer();
    cover   = browser->currentCover().image.transformed(QMatrix().rotate(-90)).mirrored(false, true);
    buffer  = bg.copy();

    /*
     * Calculate initial position on-screen, which we'll eventually
     * relocate to upper-left offset.
     */

    QSize cs = browser->coverSize();

    album_x = orig_x = (buffer.size().width() - cs.width()) / 2;
    album_y = orig_y = (buffer.size().height() - cs.height()) / 2;

    LOG.info("album_y = %u, A = %u, B = %u", album_x, buffer.size().height(), cover.size().height());
}

AlbumDisplay::~AlbumDisplay(void) {
    LOG.debug("~albumDisplay");
}

/*
 * Main animation here is to take the original background buffer (with
 * the albums) and psuedo-alpha-fade them out, while moving the
 * primary album into focus.
 */

void AlbumDisplay::animate(void) {
    //    LOG.debug("+ animate");

    /*
     * Calculate the incremental movements, which right now takes off
     * 10% of distance to destination each time through.
     */

    /*
    uint32_t incr_x = qMax(album_x / 10, orig_x / 100);
    uint32_t incr_y = qMax(album_y / 10, orig_y / 100);
    */

    uint32_t incr_x = qMax(album_x / 10, (uint32_t)1);
    uint32_t incr_y = qMax(album_y / 10, (uint32_t)1);

    LOG.debug("album_x = %u (-%u), album_y = %u (-%u)", album_x, incr_x, album_y, incr_y);

    album_x        -= qMin(album_x, incr_x);
    album_y        -= qMin(album_y, incr_y);

    if (album_x == 0 || album_y == 0)
        doAnimate(0);

    doRender();
}

void AlbumDisplay::render(void) {
    /*
     * Calculate % fade as % distance covered by album to final point:
     *     (orig_x - album_x) / album_x
     *
     * Faux-fade bg
     * copy bg onto buffer
     * start moving the album
     *
     * TODO: we may need to scale the image down on smaller interfaces
     * to leave room for other stuff.
     */

    uint32_t *px;
    uint8_t r, g, b;

    for (uint16_t y = 0; y < bg.size().height(); y++) {
        px = (uint32_t*)bg.scanLine(y);
        for (uint16_t x = 0; x < bg.size().width(); x++) {
            r = qRed(px[x])   * album_x / orig_x;
            g = qGreen(px[x]) * album_x / orig_x;
            b = qBlue(px[x])  * album_x / orig_x;
            px[x] = qRgb(r,g,b);
        }
    }


    QPainter p(&buffer);
    p.drawImage(0, 0, bg);

    QRect rect(QPoint(0, cover.size().height()/2/3),
            cover.size());

    p.drawImage(QPoint(album_x, album_y), cover, rect);

    //    LOG.debug("drawing cover @ %u:%u [%u:%u:%u:%u]", album_x, album_y, r.left(), r.top(), r.right(), r.bottom());


    QWidget::update();
}



void AlbumDisplay::resizeEvent(QResizeEvent *e) {
    LOG.debug("-@ resizeEvent: [%i:%i] -> [%i:%i]",
           e->oldSize().width(), e->oldSize().height(),
           e->size().width(),    e->size().height());

    /*
    bool firstTime = (e->oldSize().width() == -1 && e->oldSize().height() == -1);
    if (firstTime) {
        buffer = QImage(e->size(), QImage::Format_RGB32);
        buffer.fill(Qt::black);
    }
    */

    doAnimate();

    QWidget::resizeEvent(e);
}

void AlbumDisplay::paintEvent(QPaintEvent *e) {
    LOG.debug("-@ paintEvent");
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QPoint(0, 0), this->buffer);
}

void AlbumDisplay::mousePressEvent(QMouseEvent *e) {
    LOG.debug("-@ mousePressEvent");
    Q_UNUSED(e);

    /*
     * Capture the "click on X" here and call this->close().
     */

    if (album_x == 0)
        QWidget::close();
}

void AlbumDisplay::closeEvent(QCloseEvent *e) {

    /*
     * Stop any animation maybe going on and transfer control back to browser.
     */

    doAnimate(0);
    browser->show();

    /*
     * We set QA_DeleteOnClose so Qt will delete us after this.
     */

    e->accept();
}

