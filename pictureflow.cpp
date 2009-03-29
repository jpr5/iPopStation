/*
  PictureFlow - animated image show widget
  http://pictureflow.googlecode.com

  Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/


#include <QDebug>

#include "math.hh"
#include "pictureflow.h"


PictureFlowPrivate::PictureFlowPrivate(PictureFlow* w) {
    widget = w;

    slideWidth = 130;
    slideHeight = 175;
    zoom = 100;

    centerIndex = 0;

    slideFrame = 0;
    step = 0;
    target = 0;
    fade = 256;

    /*
     * Schedule a render as soon as the event loop starts running
     * (qApp::exec()).
     *
     * To call EMIT here would pre-empt the event loop and probably
     * not work.
     */

    browseTimer.setSingleShot(true);
    browseTimer.setInterval(0);
    QObject::connect(&browseTimer, SIGNAL(timeout()), widget, SLOT(renderBrowse()));

    recalc(320, 240);
    resetSlides();

}

int PictureFlowPrivate::slideCount() const {
    return slideImages.count();
}

QSize PictureFlowPrivate::slideSize() const {
    return QSize(slideWidth, slideHeight);
}

void PictureFlowPrivate::setSlideSize(QSize size) {
    printf("setSlideSize(%u, %u)\n", size.width(), size.height());

    if (slideWidth == size.width() &&
        slideHeight == size.height())
        return;

    slideWidth = size.width();
    slideHeight = size.height();
    recalc(buffer.width(), buffer.height());
    triggerBrowse();
}

int PictureFlowPrivate::zoomFactor() const {
    return zoom;
}

void PictureFlowPrivate::setZoomFactor(int z) {
    if (z <= 0)
        return;

    zoom = z;
    recalc(buffer.width(), buffer.height());
    triggerBrowse();
}

QImage PictureFlowPrivate::slide(int index) const {
    return slideImages[index];
}

void PictureFlowPrivate::addSlide(QImage const& image) {
    /* Test to see what the blank slide looks like */
    if (slideImages.empty())
        slideImages.push_back(blankSurface);

    slideImages.push_back(image);
    surfaceCache.remove(slideImages.size() - 1);
    triggerBrowse();
}

int PictureFlowPrivate::currentSlide() const {
    return centerIndex;
}

void PictureFlowPrivate::setCurrentSlide(int index) {
    step = 0;
    centerIndex = qBound(index, 0, slideImages.count()-1);
    target = centerIndex;
    slideFrame = index << 16;
    resetSlides();
    triggerBrowse();
}

void PictureFlowPrivate::showPrevious() {
    if (step >= 0 && centerIndex > 0) {

        target = centerIndex - 1;
        startBrowse();

    } else

        target = qMax(0, centerIndex - 2);
}

void PictureFlowPrivate::showNext() {
    if (step <= 0 && (centerIndex < slideImages.count()-1)) {

        target = centerIndex + 1;
        startBrowse();

    } else

        target = qMin(centerIndex + 2, slideImages.count()-1);
}



void PictureFlowPrivate::showSlide(int index) {
    index = qMax(index, 0);
    index = qMin(slideImages.count()-1, index);

    if (index == centerSlide.slideIndex)
        return;

    target = index;
    startBrowse();
}

void PictureFlowPrivate::resize(int w, int h) {
    printf("resize(%u, %u)\n", w, h);
    if (buffer.size().width() == w &&
        buffer.size().height() == h)
        return;

    recalc(w, h);
    resetSlides();
    triggerBrowse();
}

// adjust slides so that they are in "steady state" position
void PictureFlowPrivate::resetSlides() {
    centerSlide.angle = 0;
    centerSlide.cx = 0;
    centerSlide.cy = 0;
    centerSlide.slideIndex = centerIndex;

    leftSlides.clear();
    leftSlides.resize(6);
    for (int i = 0; i < leftSlides.count(); i++) {
        SlideInfo& si = leftSlides[i];
        si.angle = itilt;
        si.cx = -(offsetX + spacing*i*PFREAL_ONE);
        si.cy = offsetY;
        si.slideIndex = centerIndex-1-i;

        printf("cover[%u] = %i, %i, %i\n", si.slideIndex, si.angle, si.cx, si.cy);
    }

    rightSlides.clear();
    rightSlides.resize(6);
    for (int i = 0; i < rightSlides.count(); i++) {
        SlideInfo& si = rightSlides[i];
        si.angle = -itilt;
        si.cx = offsetX + spacing*i*PFREAL_ONE;
        si.cy = offsetY;
        si.slideIndex = centerIndex+1+i;
        printf("cover[%u] = %i, %i, %i\n", si.slideIndex, si.angle, si.cx, si.cy);
    }
}

#define BILINEAR_STRETCH_HOR 4
#define BILINEAR_STRETCH_VER 4


static QImage prepareSurface(QImage img, int w, int h) {
    printf("prepareSurface(%u, %u)\n", w, h);

    img = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).mirrored(true, false);

    QImage out(w, h*2, QImage::Format_RGB32);
    QPainter p(&out);

    int padding = h/3;

    p.drawImage(QPoint(0, padding),   img);
    p.drawImage(QPoint(0, padding+h), img.mirrored(false, true));
    p.end();

    int stop = out.size().height();
    for (int x = 0; x < w; x++)
        for (int y = padding+h; y < stop; y++) {
            QRgb c = out.pixel(x, y);

            /*
             * Faux transparency/blend.  Don't fuck with this syntax,
             * the transient float (% strength) will stop working. :-(
             */

            uint8_t r = qRed(c)   * (stop - y) / stop;
            uint8_t g = qGreen(c) * (stop - y) / stop;
            uint8_t b = qBlue(c)  * (stop - y) / stop;

            out.setPixel(x, y, qRgb(r, g, b));
        }

    return out.transformed(QMatrix().rotate(270));
}


// get transformed image for specified slide
// if it does not exist, create it and place it in the cache
QImage* PictureFlowPrivate::surface(int slideIndex) {
    if (slideIndex < 0)
        return 0;

    if (slideIndex >= slideImages.count())
        return 0;

    if (surfaceCache.contains(slideIndex))
        return surfaceCache[slideIndex];

    QImage img = widget->slide(slideIndex);

    if(img.isNull()) {
        if(blankSurface.isNull()) {
            blankSurface = QImage(slideWidth, slideHeight, QImage::Format_RGB32);

            QPainter painter(&blankSurface);
            QPoint p1(slideWidth*4/10, 0);
            QPoint p2(slideWidth*6/10, slideHeight);
            QLinearGradient linearGrad(p1, p2);
            linearGrad.setColorAt(0, Qt::black);
            linearGrad.setColorAt(1, Qt::white);
            painter.setBrush(linearGrad);
            painter.fillRect(0, 0, slideWidth, slideHeight, QBrush(linearGrad));

            painter.setPen(QPen(QColor(64,64,64), 4));
            painter.setBrush(QBrush());
            painter.drawRect(2, 2, slideWidth-3, slideHeight-3);
            painter.end();
            blankSurface = prepareSurface(blankSurface, slideWidth, slideHeight);
        }

        return &blankSurface;
    }

    surfaceCache.insert(slideIndex, new QImage(prepareSurface(img, slideWidth, slideHeight)));

    return surfaceCache[slideIndex];
}

// Schedules rendering the slides. Call this function to avoid immediate
// render and thus cause less flicker.
void PictureFlowPrivate::triggerBrowse() {
    browseTimer.start();
}

// Render the slides. Updates only the offscreen buffer.
void PictureFlowPrivate::renderBrowse() {
    printf("renderBrowse\n");
    buffer.fill(Qt::black);

    int nleft = leftSlides.count();
    int nright = rightSlides.count();

    QRect r = renderSlide(centerSlide);
    int c1 = r.left();
    int c2 = r.right();

    printf("initial c1, c2 = %u, %u\n", c1, c2);

    if (step == 0) {

        // no animation, boring plain rendering
        for (int index = 0; index < nleft-1; index++) {
            int alpha = (index < nleft-2) ? 256 : 128;
            QRect rs = renderSlide(leftSlides[index], alpha, 0, c1-1);
            if (!rs.isEmpty())
                c1 = rs.left();
        }

        for (int index = 0; index < nright-1; index++) {
            int alpha = (index < nright-2) ? 256 : 128;
            QRect rs = renderSlide(rightSlides[index], alpha, c2+1, buffer.width());
            if (!rs.isEmpty())
                c2 = rs.right();
        }

    } else {

        // the first and last slide must fade in/fade out
        for (int index = 0; index < nleft; index++) {
            int alpha = 256;
            if (index == nleft-1)
                alpha = (step > 0) ? 0 : 128-fade/2;
            if (index == nleft-2)
                alpha = (step > 0) ? 128-fade/2 : 256-fade/2;
            if (index == nleft-3)
                alpha = (step > 0) ? 256-fade/2 : 256;
            QRect rs = renderSlide(leftSlides[index], alpha, 0, c1-1);
            if (!rs.isEmpty())
                c1 = rs.left();
        }

        for (int index = 0; index < nright; index++) {
            int alpha = (index < nright-2) ? 256 : 128;
            if (index == nright-1)
                alpha = (step > 0) ? fade/2 : 0;
            if (index == nright-2)
                alpha = (step > 0) ? 128+fade/2 : fade/2;
            if (index == nright-3)
                alpha = (step > 0) ? 256 : 128+fade/2;
            QRect rs = renderSlide(rightSlides[index], alpha, c2+1, buffer.width());
            if (!rs.isEmpty())
                c2 = rs.right();
        }

    }
}

// Renders a slide to offscreen buffer. Returns a rect of the rendered area.
// alpha=256 means normal, alpha=0 is fully black, alpha=128 half transparent
// col1 and col2 limit the column for rendering.
QRect PictureFlowPrivate::renderSlide(const SlideInfo &slide, int alpha, int col1, int col2) {
    printf("renderSlide(%i, %i, %i, %i)\n", slide.slideIndex, alpha, col1, col2);
    QImage* src = surface(slide.slideIndex);
    if (!src)
        return QRect();

    QRect rect(0, 0, 0, 0);

#ifdef PICTUREFLOW_BILINEAR_FILTER
    int sw = src->height() / BILINEAR_STRETCH_HOR;
    int sh = src->width() / BILINEAR_STRETCH_VER;
#else
    int sw = src->height();
    int sh = src->width();
#endif
    int h = buffer.height();
    int w = buffer.width();

    if(col1 > col2) {
        int c = col2;
        col2 = col1;
        col1 = c;
    }

    if (col1 > 0 && col2 > 0 && col1 == col2) {
        //        printf("!! not rendering invisible slide\n");
        return QRect();
    }

    col1 = (col1 >= 0) ? col1 : 0;
    col2 = (col2 >= 0) ? col2 : w-1;
    //    printf("-- col1 = %i, col2 = %i\n", col1, col2);
    col1 = qMin(col1, w-1);
    col2 = qMin(col2, w-1);

    int distance = h * 100 / zoom;
    PFreal sdx = fcos(slide.angle);
    PFreal sdy = fsin(slide.angle);
    PFreal xs = slide.cx - slideWidth * sdx/2;
    PFreal ys = slide.cy - slideWidth * sdy/2;
    PFreal dist = distance * PFREAL_ONE;

    //    printf("\tcol1 = %i, col2 = %i, sdx = %i, sdy = %i, xs = %i, ys = %i, dist = %i, angle = %i\n",
    //           col1, col2, sdx, sdy, xs, ys, dist, slide.angle);

    int xi = qMax((PFreal)0, (w*PFREAL_ONE/2) + fdiv(xs*h, dist+ys) >> PFREAL_SHIFT);
    if (xi >= w)
        return rect;

    bool flag = false;
    rect.setLeft(xi);

    printf("\t[%i]\n", slide.angle);
    printf("\t%i\t%i\t%i\t%i\t%i\n", w, xs, h, dist, ys);
    printf("\t%i\t%i\t%i\t%i\n", (w*PFREAL_ONE/2), fdiv(xs*h, dist+ys), (w*PFREAL_ONE/2) + fdiv(xs*h, dist+ys), (w*PFREAL_ONE/2) + fdiv(xs*h, dist+ys)  >> PFREAL_SHIFT);

    printf("__ xi = %i, col1 = %i, col2 = %i\n", xi, col1, col2);
    for (int x = qMax(xi, col1); x <= col2; x++) {
        PFreal hity = 0;
        PFreal fk = rays[x];
        if(sdy) {
            fk = fk - fdiv(sdx,sdy);
            hity = -fdiv((rays[x]*distance - slide.cx + slide.cy*sdx/sdy), fk);
        }

        dist = distance*PFREAL_ONE + hity;
        if (dist < 0)
            continue;

        PFreal hitx = fmul(dist, rays[x]);
        PFreal hitdist = fdiv(hitx - slide.cx, sdx);

#ifdef PICTUREFLOW_BILINEAR_FILTER
        int column = sw*BILINEAR_STRETCH_HOR/2 + (hitdist*BILINEAR_STRETCH_HOR >> PFREAL_SHIFT);
        if (column >= sw*BILINEAR_STRETCH_HOR)
            break;
#else
        int column = sw/2 + (hitdist >> PFREAL_SHIFT);
        if (column >= sw)
            break;
#endif
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

#ifdef PICTUREFLOW_BILINEAR_FILTER
        int center = (sh*BILINEAR_STRETCH_VER/2);
        int dy = dist*BILINEAR_STRETCH_VER / h;
#else
        int center = (sh/2);
        int dy = dist / h;
#endif
        int p1 = center*PFREAL_ONE - dy/2;
        int p2 = center*PFREAL_ONE + dy/2;

        const QRgb *ptr = (const QRgb*)(src->scanLine(column));
        if (alpha == 256)
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
    }

    rect.setTop(0);
    rect.setBottom(h-1);
    return rect;
}

// Updates look-up table and other stuff necessary for the rendering.
// Call this when the viewport size or slide dimension is changed.
void PictureFlowPrivate::recalc(int ww, int wh) {
    printf("recalc(%u, %u)\n", ww, wh);

    if (buffer.size().width() == ww &&
        buffer.size().height() == wh)
        return;

    int w = (ww+1)/2;
    int h = (wh+1)/2;
    buffer = QImage(ww, wh, QImage::Format_RGB32);
    buffer.fill(0);

    rays.resize(w*2);

    for (int i = 0; i < w; i++) {
        PFreal gg = (PFREAL_HALF + i * PFREAL_ONE) / (2*h);
        rays[w-i-1] = -gg;
        rays[w+i] = gg;
    }

    itilt = 70 * IANGLE_MAX / 360;  // approx. 70 degrees tilted

    offsetX = slideWidth/2 * (PFREAL_ONE-fcos(itilt));
    offsetY = slideWidth/2 * fsin(itilt);
    offsetX += slideWidth * PFREAL_ONE;
    offsetY += slideWidth * PFREAL_ONE / 4;
    spacing = 40;

    surfaceCache.clear();
    blankSurface = QImage();
}

void PictureFlowPrivate::startBrowse() {
    if (animateTimer.isActive())
        return;

    step = (target < centerSlide.slideIndex) ? -1 : 1;
    animateTimer.start(30, widget);
}

// Updates the animation effect. Call this periodically from a timer.
void PictureFlowPrivate::animateBrowse() {
    printf("animateBrowse\n");
    if (!animateTimer.isActive())
        return;

    if (step == 0)
        return;

    int speed = 16384;

    // deaccelerate when approaching the target
    if (true) {
        const int max = 2 * 65536;

        int fi = slideFrame;
        fi -= (target << 16);
        if(fi < 0)
            fi = -fi;
        fi = qMin(fi, max);

        int ia = IANGLE_MAX * (fi-max/2) / (max*2);
        speed = 512 + 16384 * (PFREAL_ONE+fsin(ia))/PFREAL_ONE;
    }

    slideFrame += speed*step;

    int index = slideFrame >> 16;
    int pos = slideFrame & 0xffff;
    int neg = 65536 - pos;
    int tick = (step < 0) ? neg : pos;
    PFreal ftick = (tick * PFREAL_ONE) >> 16;

    // the leftmost and rightmost slide must fade away
    fade = pos / 256;

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
}

// -----------------------------------------

PictureFlow::PictureFlow(QWidget* parent): QWidget(parent)
{
    d = new PictureFlowPrivate(this);

    setAttribute(Qt::WA_StaticContents, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
}

PictureFlow::~PictureFlow() {
    delete d;
}

int PictureFlow::slideCount() const {
    return d->slideCount();
}


QSize PictureFlow::slideSize() const {
    return d->slideSize();
}

void PictureFlow::setSlideSize(QSize size) {
    d->setSlideSize(size);
}

int PictureFlow::zoomFactor() const {
    return d->zoomFactor();
}

void PictureFlow::setZoomFactor(int z) {
    d->setZoomFactor(z);
}

QImage PictureFlow::slide(int index) const {
    return d->slide(index);
}

void PictureFlow::addSlide(QImage const &image) {
    d->addSlide(image);
}

void PictureFlow::addSlide(QPixmap const &pixmap) {
    d->addSlide(pixmap.toImage());
}


int PictureFlow::currentSlide() const {
    return d->currentSlide();
}

void PictureFlow::setCurrentSlide(int index) {
    d->setCurrentSlide(index);
}

void PictureFlow::clear() {
    d->clearSlides();
}

void PictureFlow::renderBrowse() {
    d->renderBrowse();
    update();
}

void PictureFlow::showPrevious() {
    d->showPrevious();
}

void PictureFlow::showNext() {
    d->showNext();
}

void PictureFlow::showSlide(int index) {
    d->showSlide(index);
}

void PictureFlow::keyPressEvent(QKeyEvent* event) {
    event->ignore();
}

void PictureFlow::paintEvent(QPaintEvent* event) {
    printf("paintEvent\n");
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.drawImage(QPoint(0,0), d->buffer);
}

void PictureFlow::resizeEvent(QResizeEvent* event) {
    d->resize(width(), height());
    QWidget::resizeEvent(event);
}

void PictureFlow::timerEvent(QTimerEvent* event) {
    if (event->timerId() == d->animateTimer.timerId())
        d->animateBrowse();
    else
        QWidget::timerEvent(event);
}

void PictureFlow::mousePressEvent(QMouseEvent* event) {
    int third = size().width() / 3;

    if (event->x() <= third)
        showPrevious();
    else if (event->x() >= third*2)
        showNext();
    else
        ;
}
