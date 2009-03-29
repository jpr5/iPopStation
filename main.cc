/*
 * $Id$
 */

#define MINE 1

#include <string.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSplashScreen>

#if MINE
#include "album.hh"
#else
#include "pictureflow.h"
#endif

int main(int argc, char **argv) {

    char const *const arg = "-qws";
    argv[argc++] = strdup(arg);

    QApplication a(argc, argv);
    //    QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));

    QPixmap pixmap(QSize(320, 240));
    pixmap.fill(Qt::black);

    QSplashScreen splash(pixmap);
    splash.show();
    splash.showMessage("Booting...", Qt::AlignLeft, Qt::white);

#if MINE
    AlbumBrowser *albumBrowser = new AlbumBrowser;
#else
    PictureFlow *w = new PictureFlow;
#endif

    QDir dir = QDir::current();
    if (!dir.cd("pics")) {
        printf("couldn't cd to pics dir\n");
        return 1;
    }
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList list = dir.entryInfoList();
    QFileInfoList::iterator i;
    for (i = list.begin(); i != list.end(); i++) {
        QString msg, file = (*i).absoluteFilePath();

#if MINE
        if (albumBrowser->addCover(file)) {
#else
        if (pixmap.load(file)) {
#endif
            printf("Loaded %s\n", (char*)file.toAscii().data());
            msg.sprintf("Loaded %s", (char*)file.toAscii().data());
            splash.showMessage(msg, Qt::AlignLeft, Qt::white);

#if MINE
#else
            w->addSlide(pixmap);
#endif

        }
    }

#if MINE
    albumBrowser->setWindowTitle("PopStation");
    albumBrowser->setCoverSize(QSize(130,175));
    albumBrowser->resize(QSize(800,400));
    //    albumBrowser->resize(QSize(320,240));
    albumBrowser->show();
    //    albumBrowser->showFullScreen();
    splash.finish(albumBrowser);
#else
    w->setWindowTitle("PictureFlow test on Chumby");
    w->setCurrentSlide(w->slideCount()/2);
    w->setSlideSize(QSize(130,175)); // old: 100,135
    //    w->showFullScreen();
    w->show();
    splash.finish(w);
#endif

    printf("running\n");

    return a.exec();
}
