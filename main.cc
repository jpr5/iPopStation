/*
 * $Id$
 */


#include <string.h>

#include <QApplication>

#include "ps.hh"
#include "logger.hh"
#include "album.hh"


int main(int argc, char **argv) {

    char const *const arg = "-qws";
    argv[argc++] = strdup(arg);

    QApplication app(argc, argv);
#if !TEST
    app.changeOverrideCursor(QCursor(Qt::BlankCursor));
    app.setOverrideCursor(QCursor(Qt::BlankCursor));
#endif

    /*
     * Initialize logger and grab keys, etc.
     */

    LOG.program("ps");
    LOG.level(LOG_DEBUG);
    app.installEventFilter(&LOG);

    /*
     * Initialize the main widget (AlbumBrowser), and show it.
     */

    AlbumBrowser ab;
    if (!ab.init()) {
        LOG.error("unable to initialize album browser, bailing");
        return 1;
    }

#if TEST
    ab.show();
#else
    ab.showFullScreen();
#endif

    LOG.info("running", LOG.program());

    return app.exec();
}
