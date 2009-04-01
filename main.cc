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

    /*
     * Initialize logger and grab keys, etc.
     */

    LOG.program("ps");
    LOG.level(LOG_DEBUG);
    a.installEventFilter(&LOG);

    /*
     * Get the splash screen going.
     */

#if TEST
    QSize screenSize(800, 400);
#else
    QSize screenSize(320, 240);
#endif

    QPixmap pixmap(screenSize);
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
        QString msg = "Loaded %1", file = (*i).absoluteFilePath();

        if (albumBrowser->addCover(file)) {
            LOG.puke("loaded %s", (char*)file.toAscii().data());
            splash.showMessage(msg.arg(file), Qt::AlignLeft, Qt::white);
        }
    }

    albumBrowser->setWindowTitle("PopStation");
    albumBrowser->setCoverSize(QSize(130,175));
    albumBrowser->resize(screenSize);

#if TEST
    albumBrowser->show();
#else
    albumBrowser->showFullScreen();
#endif

    splash.finish(albumBrowser);

    LOG.info("%s running", LOG.program());

    return a.exec();
}
