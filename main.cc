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
#include <string.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSplashScreen>

#include "album.hh"
#include "pictureflow.h"

#define MINE 1

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

        if (pixmap.load(file)) {
            printf("Loaded %s\n", (char*)file.toAscii().data());
            msg.sprintf("Loaded %s", (char*)file.toAscii().data());
            splash.showMessage(msg, Qt::AlignLeft, Qt::white);

#if MINE
            albumBrowser->addCover(pixmap.toImage(), file);
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
