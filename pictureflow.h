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

#ifndef PICTUREFLOW_H
#define PICTUREFLOW_H

#include <QWidget>
#include <QBasicTimer>
#include <QCache>
#include <QImage>
#include <QKeyEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QVector>


#include "math.hh"
#include "album.hh"

// uncomment this to enable bilinear filtering for texture mapping
// gives much better rendering, at the cost of memory space
// #define PICTUREFLOW_BILINEAR_FILTER


struct SlideInfo {
    int slideIndex;
    int angle;
    PFreal cx;
    PFreal cy;
};



class PictureFlowPrivate;

/*!
  Class PictureFlow implements an image show widget with animation effect
  like Apple's CoverFlow (in iTunes and iPod). Images are arranged in form
  of slides, one main slide is shown at the center with few slides on
  the left and right sides of the center slide. When the next or previous
  slide is brought to the front, the whole slides flow to the right or
  the right with smooth animation effect; until the new slide is finally
  placed at the center.

 */

class PictureFlow : public QWidget {
    Q_OBJECT;

    Q_PROPERTY(int currentSlide READ currentSlide WRITE setCurrentSlide);
    Q_PROPERTY(QSize slideSize READ slideSize WRITE setSlideSize);
    Q_PROPERTY(int zoomFactor READ zoomFactor WRITE setZoomFactor);

 public:
    /*!
      Creates a new PictureFlow widget.
    */
    PictureFlow(QWidget* parent = 0);

    /*!
      Destroys the widget.
    */
    ~PictureFlow();

    /*!
      Returns the total number of slides.
    */
    int slideCount() const;

    /*!
      Returns the dimension of each slide (in pixels).
    */
    QSize slideSize() const;

    /*!
      Sets the dimension of each slide (in pixels).
    */
    void setSlideSize(QSize size);

    /*!
      Sets the zoom factor (in percent).
    */
    void setZoomFactor(int zoom);

    /*!
      Returns the zoom factor (in percent).
    */
    int zoomFactor() const;

    /*!
      Returns QImage of specified slide.
      This function will be called only whenever necessary, e.g. the 100th slide
      will not be retrived when only the first few slides are visible.
    */
    virtual QImage slide(int index) const;

    virtual void addSlide(const QPixmap& pixmap);
    virtual void addSlide(const QImage& image);

    /*!
      Returns the index of slide currently shown in the middle of the viewport.
    */
    int currentSlide() const;

    public slots:

    void showSlide(int index);
    void setCurrentSlide(int index);

    /*!
      Rerender the widget. Normally this function will be automatically invoked
      whenever necessary, e.g. during the transition animation.
    */
    void renderBrowse();

    void showPrevious();
    void showNext();
    void clear();


 protected:
    void paintEvent(QPaintEvent *event);
    void keyPressEvent(QKeyEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void resizeEvent(QResizeEvent* event);
    void timerEvent(QTimerEvent* event);

 private:
    PictureFlowPrivate* d;
};



class PictureFlowPrivate {

 private:
    PictureFlow* widget;

    int slideWidth, slideHeight, zoom;

    QList<QImage> slideImages;

    int centerIndex;
    SlideInfo centerSlide;
    QVector<SlideInfo> leftSlides;
    QVector<SlideInfo> rightSlides;

    QVector<PFreal> rays;
    int itilt;
    int spacing;
    PFreal offsetX;
    PFreal offsetY;

    QImage blankSurface;
    QCache<int, QImage> surfaceCache;


    int slideFrame, step, target, fade;

    void recalc(int w, int h);
    QRect renderSlide(const SlideInfo &slide, int alpha=256, int col1=-1, int col=-1);
    QImage* surface(int slideIndex);
    void triggerBrowse();
    void resetSlides();

 public:
    PictureFlowPrivate(PictureFlow* widget);

    QImage buffer;

    int slideCount() const;

    void clearSlides() {
        slideImages.clear();
        surfaceCache.clear();
    };

    QSize slideSize() const;
    void setSlideSize(QSize size);

    int zoomFactor() const;
    void setZoomFactor(int z);

    QImage slide(int index) const;
    void addSlide(QImage const& image);

    int currentSlide() const;
    void setCurrentSlide(int index);

    void showPrevious();
    void showNext();
    void showSlide(int index);

    void resize(int w, int h);

    /*
     * Album browsing
     */

    QBasicTimer animateTimer;
    QTimer browseTimer;

    void startBrowse();   // triggers animation
    void animateBrowse(); // triggers render when done
    void renderBrowse();

    /*
     * NEW: Song browsing
     */

    void displayCurrentAlbum();



};

#endif // PICTUREFLOW_H
