#ifndef PS_LOGGER_HH
#define PS_LOGGER_HH

/*
 * $Id$
 */

#include <stdint.h>
#include <stdarg.h>

#include <QObject>
#include <QKeyEvent>


#define LOG_ALL       9
#define LOG_PUKE      8
#define LOG_DEBUG     7
#define LOG_INFO      6
#define LOG_NOTICE    5
#define LOG_WARN      4
#define LOG_ERROR     3
#define LOG_CRIT      2
#define LOG_ALERT     1
#define LOG_EMERG     0


class CLogger : public QObject {
    Q_OBJECT;

 private:
    static const uint8_t  MAXDATELEN = 20;
    static const uint8_t  MAXLEN     = 50;
    static const uint16_t MAXBUFLEN  = 980;
    static const uint16_t MAXLOGLEN  = 1000;

    char timestamp[MAXDATELEN];
    char buf[MAXBUFLEN];

    char *progName;
    uint16_t progPID;
    uint8_t logLevel;

    char const *const ts(void);

    void log(uint8_t, const char *, ...);
    void vlog(uint8_t, const char *, va_list);
    void writeLog(uint8_t, const char *);

 protected:

    bool eventFilter(QObject *, QEvent *);

 public:

    CLogger(void);
    ~CLogger(void);

    bool program(const char *progName);
    const char *const program(void) const;

    void level(uint8_t);
    const uint8_t level(void) const;

    void puke(const char *, ...);
    void debug(const char *, ...);
    void info(const char *, ...);
    void notice(const char *, ...);
    void warn(const char *, ...);
    void error(const char *, ...);
};

/*
 * Allocated in logger.cc.
 */

extern ::CLogger LOG;

#endif
