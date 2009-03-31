/*
 * $Id$
 */

#define TEST 1

#include <string.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSplashScreen>

#include "logger.hh"
#include "album.hh"


int main(int argc, char **argv) {

    char const *const arg = "-qws";
    argv[argc++] = strdup(arg);

    QApplication a(argc, argv);
#if !TEST
    QApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#endif

    LOG.program("ps");
    LOG.level(LOG_ALL);

    LOG.info("hi mom");

    QPixmap pixmap(QSize(320, 240));
    pixmap.fill(Qt::black);

    QSplashScreen splash(pixmap);
    splash.show();
    splash.showMessage("Booting...", Qt::AlignLeft, Qt::white);

    QDir dir = QDir::current();
    if (!dir.cd("pics"))
        LOG.warn("unable to cd to pics dir, using current");

    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList list = dir.entryInfoList();
    if (list.empty()) {
        LOG.error("no pics in dir %s", dir.path().toAscii().data());
        return 1;
    }

    AlbumBrowser *albumBrowser = new AlbumBrowser;

    for (QFileInfoList::iterator i = list.begin(); i != list.end(); i++) {

        QString msg, file = (*i).absoluteFilePath();

        if (albumBrowser->addCover(file)) {
            printf("Loaded %s\n", (char*)file.toAscii().data());
            msg.sprintf("Loaded %s", (char*)file.toAscii().data());
            splash.showMessage(msg, Qt::AlignLeft, Qt::white);
        } else
            LOG.warn("couldn't load %s", (char*)file.toAscii().data());

    }

    albumBrowser->setWindowTitle("PopStation");
    albumBrowser->setCoverSize(QSize(130,175));

#if TEST
    albumBrowser->resize(QSize(800,400));
    albumBrowser->show();
#else
    albumBrowser->resize(QSize(320,240));
    albumBrowser->showFullScreen();
#endif

    splash.finish(albumBrowser);

    printf("running\n");

    return a.exec();
}
