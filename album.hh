#ifndef PS_ALBUM_HH
#define PS_ALBUM_HH

/*
 * $Id$
 */

#include "browser.hh"

#include <QList>
#include <QVector>
#include <QCache>

/*
 * Possible problems: int16_t might need to be int32_t;
 */

class AlbumCover {

public:
    int16_t angle;
    PFreal cx, cy;
    QImage image;
    QString path;

    AlbumCover(void);
    AlbumCover(const QImage &, const QString & = "");
    AlbumCover(const AlbumCover &);

    void process(uint16_t, uint16_t);

    const AlbumCover &operator=(const AlbumCover &);

};


/* ---------- */

class AlbumBrowser : public AsyncBrowser {
    Q_OBJECT;

 private:
    unsigned foo;

    QList<AlbumCover> covers;

    // cover
    uint8_t  c_zoom;
    uint16_t c_width, c_height;

    uint16_t c_focus;
    //    uint32_t slideFrame, step, target, fade;

    PFreal   r_offsetX, r_offsetY;
    QVector<PFreal> rays;

    QImage buffer, noCover;

 protected slots:

    virtual void animate(void);
    virtual void render(void);

 public:

    AlbumBrowser(QWidget * = 0);
    ~AlbumBrowser(void);

    /* Methods to respond to as a QWidget */
    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *);

    void loadCovers(QList<QString>);
    void resetCovers(void);

    void addCover(const QImage &, const QString & = "");
    QRect renderCover(AlbumCover &, int16_t = -1, int16_t = -1);

    void setCoverSize(QSize);

    /* Utility */
    void resizeView(const QSize &);
    void prepRender(void);

};



/*
 * Things we need:
 *
 *   State = Blown (UP, DOWN)
 *
 * KEY: we only trigger animate()s; render is triggered as a result.
 * UNLESS: we just want to repaint.

 Click
 showAlbum -> trigger animate (repeated, 30ms)
 animate Blowup or Blowdown -> trigger render (singleshot, immediate)

*/

#endif
