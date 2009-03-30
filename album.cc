/*
 * $Id$
 */

#include "album.hh"

#include <QPainter>
#include <QResizeEvent>


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
 *       processed
 */

void AlbumCover::process(uint16_t c_width, uint16_t c_height) {
    printf("** process(%u, %u)\n", c_width, c_height);

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
    for (uint16_t x = 0; x < c_width; x++)
        for (uint16_t y = padding+c_height; y < stop; y++) {
            QRgb c = out.pixel(x, y);

            uint8_t r = qRed(c)   * (stop - y) / stop;
            uint8_t g = qGreen(c) * (stop - y) / stop;
            uint8_t b = qBlue(c)  * (stop - y) / stop;

            out.setPixel(x, y, qRgb(r, g, b));
        }

    image = out.transformed(QMatrix().rotate(270));
}

/* ------------- */
/* ------------- */
/* ------------- */
/* ------------- */

AlbumBrowser::AlbumBrowser(QWidget *parent) : AsyncBrowser(parent) {
    printf("** albumBrowser\n");
    c_zoom   = 100;
    c_width  = 135;
    c_height = 175;

    f_fade = 256;
    f_frame = 0;
    f_direction = 0;
}

AlbumBrowser::~AlbumBrowser(void) {
}


void AlbumBrowser::arrangeCovers(int32_t factor) {
    /*
     * NOTE: may be able to do these calculations once and just assign
     * the negatives of the left to the right.  [ middle+1 = -(middle-1) ]
     */

    for (int16_t i = c_focus - 1; i != -1; i--) {
        AlbumCover &a = covers[i];
        a.angle = tilt_factor;
        a.cx    = -(r_offsetX + (spacing_offset*(c_focus-1-i)*PFREAL_ONE) + factor);
        a.cy    = r_offsetY;

        printf("cover[%u] = %i, %i, %i\n", i, a.angle, a.cx, a.cy);
    }

    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        AlbumCover &a = covers[i];
        a.angle = -tilt_factor;
        a.cx    = r_offsetX + (spacing_offset*(i-c_focus-1)*PFREAL_ONE) - factor;
        a.cy    = r_offsetY;

        printf("cover[%u] = %i, %i, %i\n", i, a.angle, a.cx, a.cy);
    }
}


void AlbumBrowser::prepRender(void) {
    printf("** prepRender()\n");

    uint16_t width  = (buffer.size().width()  + 1) / 2;
    uint16_t height = (buffer.size().height() + 1) / 2;

    rays.resize(width * 2);
    uint16_t i;
    for (i = 0; i < width; i++) {
        PFreal gg = (PFREAL_HALF + i * PFREAL_ONE) / (2 * height);
        rays[width-i-1] = -gg;
        rays[width+i]   =  gg;
    }

    r_offsetX =
        ((c_width / 2) * (PFREAL_ONE - fcos(tilt_factor))) +
        (c_width * PFREAL_ONE);

    r_offsetY =
        ((c_width / 2) * fsin(tilt_factor)) +
        (c_width * PFREAL_ONE / 4);

    c_focus = covers.size()/2;
    f_frame = c_focus << 16;

    arrangeCovers();
}

void AlbumBrowser::render(void) {
    printf("** render\n");

    /*
     * Clean out the off-screen buffer and start with the in-focus
     * cover.
     */

    buffer.fill(Qt::black);

    printf("** c_focus = %u (of %u)\n", c_focus, covers.size());

    uint16_t x_bound;
    QRect r, rs;

    r = renderCover(covers[c_focus]);
    printf("** initial bound: [%u, %u]\n", r.left(), r.right());

    x_bound = r.left();
    for (int16_t i = c_focus - 1; i != -1; i--) {
        printf("-> cover %i\n", i);
        rs = renderCover(covers[i], 0, x_bound-1);
        if (rs.isEmpty()) {
            printf("** didn't render cover %u, stopping\n", i);
            break;
        }

        x_bound = rs.left();
    }

    x_bound = r.right();
    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        printf("-> cover %i\n", i);
        rs = renderCover(covers[i], x_bound+1, buffer.width());
        if (rs.isEmpty()) {
            printf("** didn't render cover %u, stopping\n", i);
            break;
        }

        x_bound = rs.right();
    }

    QWidget::update();
}

QRect AlbumBrowser::renderCover(AlbumCover &a, int16_t lb, int16_t rb) {
    printf("** renderCover(%i, %i)\n", lb, rb);

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
        printf("!! not rendering invisible slide\n");
        return rect;
    }

    int distance = h * 100 / c_zoom;
    PFreal sdx = fcos(a.angle);
    PFreal sdy = fsin(a.angle);
    PFreal xs = a.cx - c_width * sdx/2;
    PFreal ys = a.cy - c_width * sdy/2;
    PFreal dist = distance * PFREAL_ONE;

    //                      start from middle  +/- distance
    int xi = qMax((PFreal)0, (w*PFREAL_ONE/2) + fdiv(xs*h, dist+ys) >> PFREAL_SHIFT);
    if (xi >= w)
        return rect;

    printf("lb = %i, rb = %i, ( xi ) = %i\n", lb, rb, xi);

    bool flag = false;
    rect.setLeft(xi);

    for (int x = qMax(xi, (int)lb); x <= rb; x++) {
        PFreal hity = 0;
        PFreal fk = rays[x];
        if (sdy) {
            fk = fk - fdiv(sdx,sdy);
            hity = -fdiv((rays[x]*distance - a.cx + a.cy*sdx/sdy), fk);
        }

        dist = distance*PFREAL_ONE + hity;
        if (dist < 0)
            continue;

        PFreal hitx = fmul(dist, rays[x]);
        PFreal hitdist = fdiv(hitx - a.cx, sdx);

        int column = sw/2 + (hitdist >> PFREAL_SHIFT);
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
        QRgb* pixel1 = (QRgb*)(buffer.scanLine(y1)) + x;
        QRgb* pixel2 = (QRgb*)(buffer.scanLine(y2)) + x;
        QRgb pixelstep = pixel2 - pixel1;

        int center = (sh/2);
        int dy = dist / h;
        int p1 = center*PFREAL_ONE - dy/2;
        int p2 = center*PFREAL_ONE + dy/2;

        const QRgb *ptr = (const QRgb*)(src.scanLine(column));
        //        if (alpha == 256)
            while((y1 >= 0) && (y2 < h) && (p1 >= 0)) {
                *pixel1 = ptr[p1 >> PFREAL_SHIFT];
                *pixel2 = ptr[p2 >> PFREAL_SHIFT];
                p1 -= dy;
                p2 += dy;
                y1--;
                y2++;
                pixel1 -= pixelstep;
                pixel2 += pixelstep;
            }
#if 0
       else
            while((y1 >= 0) && (y2 < h) && (p1 >= 0)) {
                QRgb c1 = ptr[p1 >> PFREAL_SHIFT];
                QRgb c2 = ptr[p2 >> PFREAL_SHIFT];

                int r1 = qRed(c1) * alpha/256;
                int g1 = qGreen(c1) * alpha/256;
                int b1 = qBlue(c1) * alpha/256;
                int r2 = qRed(c2) * alpha/256;
                int g2 = qGreen(c2) * alpha/256;
                int b2 = qBlue(c2) * alpha/256;

                *pixel1 = qRgb(r1, g1, b1);
                *pixel2 = qRgb(r2, g2, b2);
                p1 -= dy;
                p2 += dy;
                y1--;
                y2++;
                pixel1 -= pixelstep;
                pixel2 += pixelstep;
            }
#endif
    }

    rect.setTop(0);
    rect.setBottom(h-1);

    return rect;
}

void AlbumBrowser::animate(void) {
    printf("** animate\n");

    if (f_direction == 0) {
        printf("!! animate: no directional change\n");
        doAnimate(false);
        return;
    }

    int16_t c_target = c_focus + f_direction;

    if (c_target < 0 || c_target >= covers.size()) {
        printf("!! animate: asked to go beyond bounds\n");
        f_direction = 0;
        return;
    }

    int32_t f_idx = f_frame - (c_target << 16);
    if (f_idx < 0)
        f_idx = -f_idx;
    f_idx = qMin((int)f_idx, (int)f_max);

    // with f_idx bounded by f_max)
    // IANGLE_MAX * (f_idx - half of max) / (twice the max)
    int32_t angle  = IANGLE_MAX * (f_idx-f_max/2) / (f_max*2);
    uint32_t speed = 512 + (16384 * (PFREAL_ONE+fsin(angle))/PFREAL_ONE);

    f_frame += speed * f_direction;

    printf("** animate: angle = %i, speed = %u, f_frame = %i (dir = %i)\n",
           angle, speed, f_frame, f_direction);

    int32_t c_idx = f_frame >> 16;
    int32_t pos   = f_frame & 0xffff;
    int32_t neg   = 65536 - pos;
    int tick      = (f_direction < 0) ? neg : pos;
    PFreal ftick  = (tick * PFREAL_ONE) >> 16;

    printf("__ c_idx = %i, pos = %i, neg = %i, tick = %i, ftick = %i\n", c_idx, pos, neg, tick, ftick);

    // track left- and right-most alpha fade
    f_fade = pos / 256;

    if (f_direction < 0)
        c_idx++;

    printf("__ c_focus = %i (%i) [%i], c_target = %i\n", c_focus, c_idx, f_direction, c_target);

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
        f_fade      = 256;
        f_direction = 0;

        arrangeCovers();

        doAnimate(false);

    } else {

        /*
         * Otherwise, we're still transitioning.  Update cover angles and
         * fade the end-most slides.
         *
         * TODO: do we need to fade those end slides?
         */

        int32_t factor = f_direction * spacing_offset * ftick;

        printf("** animating (factor = %i)\n", factor);

        arrangeCovers(factor);

        if (f_direction > 0) {
            a        = &(covers[c_idx+1]);
            a->angle = -(neg * tilt_factor) >> 16;
            ftick    = (neg * PFREAL_ONE) >> 16;
            a->cx    = fmul(r_offsetX, ftick);
            a->cy    = fmul(r_offsetY, ftick);
        } else {
            a        = &(covers[c_idx-1]);
            a->angle = (pos * tilt_factor) >> 16;
            ftick    = (pos * PFREAL_ONE) >> 16;
            a->cx    = -fmul(r_offsetX, ftick);
            a->cy    = fmul(r_offsetY, ftick);
        }
    }

    doRender();
    return;

#if 0
    if (step < 0)
        index++;

    if (centerIndex != index) {
        centerIndex = index;
        slideFrame = index << 16;
        centerSlide.slideIndex = centerIndex;
        for (int i = 0; i < leftSlides.count(); i++)
            leftSlides[i].slideIndex = centerIndex-1-i;
        for (int i = 0; i < rightSlides.count(); i++)
            rightSlides[i].slideIndex = centerIndex+1+i;
    }

    centerSlide.angle = (step * tick * itilt) >> 16;
    centerSlide.cx = -step * fmul(offsetX, ftick);
    centerSlide.cy = fmul(offsetY, ftick);

    if (centerIndex == target) {
        resetSlides();
        animateTimer.stop();
        step = 0;
        fade = 256;
        triggerBrowse();
        return;
    }

    for (int i = 0; i < leftSlides.count(); i++) {
        SlideInfo& si = leftSlides[i];
        si.angle = itilt;
        si.cx = -(offsetX + spacing*i*PFREAL_ONE + step*spacing*ftick);
        si.cy = offsetY;
    }

    for (int i = 0; i < rightSlides.count(); i++) {
        SlideInfo& si = rightSlides[i];
        si.angle = -itilt;
        si.cx = offsetX + spacing*i*PFREAL_ONE - step*spacing*ftick;
        si.cy = offsetY;
    }

    if (step > 0) {
        PFreal ftick = (neg * PFREAL_ONE) >> 16;
        rightSlides[0].angle = -(neg * itilt) >> 16;
        rightSlides[0].cx = fmul(offsetX, ftick);
        rightSlides[0].cy = fmul(offsetY, ftick);
    } else {
        PFreal ftick = (pos * PFREAL_ONE) >> 16;
        leftSlides[0].angle = (pos * itilt) >> 16;
        leftSlides[0].cx = -fmul(offsetX, ftick);
        leftSlides[0].cy = fmul(offsetY, ftick);
    }

    // must change direction ?
    if (target < index && step > 0)
        step = -1;
    else if (target > index && step < 0)
        step = 1;

    triggerBrowse();
#endif
}

bool AlbumBrowser::addCover(const QString &path_) {
    QImage image_;

    if (!image_.load(path_)) {
        printf("!! unable to load %s", (char*)path_.toAscii().data());
        return false;
    }

    addCover(image_, path_);

    return true;
}

void AlbumBrowser::addCover(const QImage &image_, const QString &path_) {
    AlbumCover a(image_, path_);
    a.process(c_width, c_height);
    covers.push_back(a);
}

void AlbumBrowser::loadCovers(QList<QString> covers) {
    resetCovers();

    QList<QString>::iterator i;
    QImage image;

    for (i = covers.begin(); i != covers.end(); i++) {
        QString &filename = (*i);

        if (image.load(filename)) {
            printf("** Loaded %s", filename.toAscii().data());
            addCover(image, filename);
        }
    }
}

void AlbumBrowser::resetCovers(void) {
    covers.clear();
}

void AlbumBrowser::setCoverSize(QSize s) {
    printf("** setCoverSize(%u, %u)\n", s.width(), s.height());

    if (s.width() == c_width && s.height() == c_height)
        return;

    c_width  = s.width();
    c_height = s.height();
}

/*
 * (Re)size.  Guaranteed one of these on startup.
 */

void AlbumBrowser::resizeView(const QSize &s) {
    printf("** resizeView(%u, %u)\n", s.width(), s.height());

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

    prepRender();
    render();
}


void AlbumBrowser::resizeEvent(QResizeEvent *e) {
    printf("** resizeEvent: (%i:%i) -> (%i:%i)\n",
           e->oldSize().width(), e->oldSize().height(),
           e->size().width(),    e->size().height());

    this->resizeView(e->size());

    QWidget::resizeEvent(e);
}

void AlbumBrowser::paintEvent(QPaintEvent *e) {
    printf("** paintEvent\n");
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QPoint(0, 0), this->buffer);
}

void AlbumBrowser::mousePressEvent(QMouseEvent *e) {
    printf("** mousePressEvent\n");

    uint16_t third = size().width() / 3;

    if (e->x() <= third)
        f_direction = -1;
    else if (e->x() >= third * 2)
        f_direction = 1;
    else
        ;

    doAnimate();
}
