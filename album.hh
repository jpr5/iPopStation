#ifndef PS_ALBUM_HH
#define PS_ALBUM_HH

/*
 * $Id$
 */

#include <QList>
#include <QVector>
#include <QCache>

#include "render.hh"
#include "fpmath.hh"

/*
 * FIXME: some int16_t might need to be int32_t
 * FIXME: some uint may need to be int
 */

class AlbumCover {

public:
    QImage image;
    QString path;

    int16_t angle;
    FPreal_t cx, cy;

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

    /* display mode (browse, display) */
    typedef enum {
        M_BROWSE = 0, M_DISPLAY
    } displaymode_t;

    displaymode_t d_mode;
    uint16_t d_lb, d_rb;

    /* covers */
    QList<AlbumCover> covers;
    uint8_t  c_zoom, c_focus;
    uint16_t c_width, c_height;

    /* cover display */
    QImage cover, bg;
    uint16_t orig_x, orig_y;
    int16_t album_x, album_y;

    /* raytracing */
    int8_t  f_direction;
    int32_t f_frame;
    FPreal_t r_offsetX, r_offsetY;
    QVector<FPreal_t> rays;

    /* Utility */
    void resizeView(const QSize &, bool reset);

    void renderDisplay(void);
    void renderBrowse(void);
    void animateDisplay(void);
    void animateBrowse(void);

    void  prepRender(bool reset);
    void  arrangeCovers(int32_t = 0);
    QRect renderCover(AlbumCover &, int16_t = -1, int16_t = -1);


 protected slots:

    virtual void animate(void);
    virtual void render(void);

 public:

    AlbumBrowser(QWidget * = 0);
    ~AlbumBrowser(void);

    bool addCover(const QString &);
    void addCover(const QImage &, const QString & = "");
    void loadCovers(QList<QString> &);
    void setCoverSize(QSize);
    QSize coverSize(void);
    const AlbumCover &currentCover(void);

    void displayAlbum(void);

    /* Methods to respond to as a QWidget */
    void resizeEvent(QResizeEvent *);
    void mousePressEvent(QMouseEvent *);

};

#endif
