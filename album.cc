
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
    foo = 0;

    c_zoom   = 100;
    c_width  = 135;
    c_height = 175;

    //    slideFrame = step = target = 0;
    //    fade = 256;
}

AlbumBrowser::~AlbumBrowser(void) {
}


void AlbumBrowser::animate(void) {
    printf("** animate\n");

    if (foo++ == 5) {
        doAnimate(false);
        foo = 0;
    }

    doRender();
}

void AlbumBrowser::render(void) {
    printf("** render\n");

#if 0
    AlbumCover &a = covers[covers.size()/2];
    uint16_t at_x = (buffer.size().width()  - a.image.size().width())/2;
    uint16_t at_y = (buffer.size().height() - a.image.size().height())/2;

    QPainter p(&buffer);

    p.translate(at_x+1, at_y+1);
    //    p.shear(0.F, 0.4F);
    //    p.scale(.5F, 1);
    p.drawImage(0, 0, a.image);
#endif

    buffer.fill(Qt::black);

    /* TODO: here a decision should be made about how many covers to
     * render in either direction.
     *
     * NOTE: when doing animation and updating slide positions,
     * angles, cx & cy -- there should be an opportunity to figure out
     * where they would land inside buffer.size().width().  look at
     * (xi) calc which is figuring out x drawing index.
     *
     * IDEA: maybe when rs.isEmpty()?
     */

    printf("** c_focus = %u (of %u)\n", c_focus, covers.size());

    /*
     * Start with middle slide.
     */

    QRect r = renderCover(covers[c_focus]);

    uint16_t l_bound = r.left();
    uint16_t r_bound = r.right();
    printf("initial l_bound, r_bound = %u, %u\n", l_bound, r_bound);

    for (int16_t i = c_focus - 1; i != -1; i--) {
        printf("-> cover %i\n", i);
        QRect rs = renderCover(covers[i], 0, l_bound-1);
        if (rs.isEmpty()) {
            printf("** didn't render cover %u, stopping\n", i);
            break;
        }

        l_bound = rs.left();
    }

    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        printf("-> cover %i\n", i);
        QRect rs = renderCover(covers[i], r_bound+1, buffer.width());
        if (rs.isEmpty()) {
            printf("** didn't render cover %u, stopping\n", i);
            break;
        }

        r_bound = rs.right();
    }

}

QRect AlbumBrowser::renderCover(AlbumCover &a, int16_t col1, int16_t col2) {
    printf("** renderCover(%i, %i)\n", col1, col2);

    QRect rect(0, 0, 0, 0);
    QImage &src = a.image;

    int sw = src.height();
    int sh = src.width();
    int h = buffer.height();
    int w = buffer.width();

    if (col1 > col2)
        qSwap(col1, col2);

    col1 = (col1 >= 0) ? col1 : 0;
    col2 = (col2 >= 0) ? col2 : w-1;
    col1 = qMin((int)col1, w-1);
    col2 = qMin((int)col2, w-1);

    if (col1 - col2 == 0) {
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

    printf("col1 = %i, col2 = %i, ( xi ) = %i\n", col1, col2, xi);

    bool flag = false;
    rect.setLeft(xi);

    for (int x = qMax(xi, (int)col1); x <= col2; x++) {
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
 * TODO: Consider dumping param; just use buffer size and let
 * resizeEvent handle it.
 */

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

    noCover = QImage();

    c_focus = covers.size()/2;

    /*
     * NOTE: may be able to do these calculations once and just assign
     * the negatives of the left to the right.  [ middle+1 = -(middle-1) ]
     */

    for (int16_t i = c_focus - 1; i != -1; i--) {
        AlbumCover &a = covers[i];
        a.angle = tilt_factor;
        a.cx    = -(r_offsetX + (spacing_offset*(c_focus-1-i)*PFREAL_ONE));
        a.cy    = r_offsetY;
        printf("cover[%u] = %i, %i, %i\n", i, a.angle, a.cx, a.cy);

    }

    for (uint16_t i = c_focus + 1; i < covers.size(); i++) {
        AlbumCover &a = covers[i];
        a.angle = -tilt_factor;
        a.cx    = r_offsetX + (spacing_offset*(i-c_focus-1)*PFREAL_ONE);
        a.cy    = r_offsetY;

        printf("cover[%u] = %i, %i, %i\n", i, a.angle, a.cx, a.cy);
    }

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
    printf("** resizeEvent: (%u:%u) -> (%u:%u)\n",
           e->oldSize().width(), e->oldSize().height(),
           e->size().width(),    e->size().height());

    this->resizeView(e->size());

    QWidget::resizeEvent(e);
}

void AlbumBrowser::paintEvent(QPaintEvent *e) {
    Q_UNUSED(e);
    printf("** paintEvent\n");

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawImage(QPoint(0, 0), this->buffer);
}

void AlbumBrowser::mousePressEvent(QMouseEvent *e) {
    Q_UNUSED(e);
    printf("** mousePressEvent\n");
    QWidget::update();
}
