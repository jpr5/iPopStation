#ifndef PS_ALBUM_HH
#define PS_ALBUM_HH

/*
 * $Id$
 */

#include <QList>
#include <QVector>
#include <QCache>

#include "render.hh"


/*
 * FIXME: some int16_t might need to be int32_t
 * FIXME: some uint may need to be int
 */

class AlbumCover {

public:
    int16_t angle;
    FPreal_t cx, cy;
    QImage image;
    QString path;

    AlbumCover(void);
    AlbumCover(const QImage &, const QString & = "");
    AlbumCover(const AlbumCover &);

    void process(uint16_t, uint16_t);

    const AlbumCover &operator=(const AlbumCover &);

};


/* ---------- */

class AlbumBrowser : public AsyncRender {
    Q_OBJECT;

 private:
    QList<AlbumCover> covers;

    // cover
    uint8_t  c_zoom, c_focus;
    uint16_t c_width, c_height;

    uint8_t f_fade;
    int8_t  f_direction;
    int32_t f_frame;

    FPreal_t r_offsetX, r_offsetY;
    QVector<FPreal_t> rays;

    QImage buffer;

    /* Utility */
    void  resizeView(const QSize &);

    void  arrangeCovers(int32_t = 0);
    void  prepRender(void);
    QRect renderCover(AlbumCover &, int16_t = -1, int16_t = -1);

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

    bool addCover(const QString &);
    void addCover(const QImage &, const QString & = "");
    void loadCovers(QList<QString>);
    void resetCovers(void);
    void setCoverSize(QSize);

};



/*
 * Things we need:
 *
 *   State = Blown (UP, DOWN)
 *

 Click
 showAlbum -> trigger animate (repeated, 30ms)
 animate Blowup or Blowdown -> trigger render (singleshot, immediate)

*/

#endif
