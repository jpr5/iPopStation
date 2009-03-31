/*
 * $Id$
 *
 * TODO: add QMutex/QMutexLocker for thread-safety (eventually).
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "logger.hh"


char const *const levels[LOG_ALL] = {
    "FATAL",
    "ALERT",
    "CRITICAL",
    "ERROR",
    "WARN",
    "NOTICE",
    "INFO",
    "DEBUG",
    "PUKE",
};

CLogger LOG;


CLogger::CLogger(void) {
    memset(buf, 0, sizeof(buf));
    progName = NULL;
    logLevel = LOG_ALL;

    memset(timestamp, 0, sizeof(timestamp));
    memset(buf, 0, sizeof(buf));
}

CLogger::~CLogger(void) {
    if (progName)
        delete[] progName;
}

bool CLogger::program(const char *progName_) {
    if (!progName_ || !*progName_)
        return false;

    if (progName)
        delete[] progName;

    progName = new char[strlen(progName_)+1];
    strcpy(progName, progName_);

    progPID = getpid();

    return true;
}

char const *const CLogger::program(void) const {
    return progName ? progName : "";
}

void CLogger::level(uint8_t level_) {
    logLevel = level_;
}

const uint8_t CLogger::level(void) const {
    return logLevel;
}

char const *const CLogger::ts(void) {
    time_t tt;
    struct tm *tv;

    tt = time(NULL);
    tv = localtime(&tt);

    strftime(timestamp, sizeof(timestamp)-1, "%b %d %H:%M:%S ", tv);

    return timestamp;
}

void CLogger::vlog(uint8_t level, const char *format, va_list args) {
    if (logLevel < level)
        return;

    vsnprintf(buf, sizeof(buf)-1, format, args);

    writeLog(level, buf);
}

void CLogger::puke(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_PUKE, format, args);
    va_end(args);
}

void CLogger::debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_DEBUG, format, args);
    va_end(args);
}

void CLogger::info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_INFO, format, args);
    va_end(args);
}

void CLogger::notice(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_NOTICE, format, args);
    va_end(args);
}

void CLogger::warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_WARN, format, args);
    va_end(args);
}

void CLogger::error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vlog(LOG_ERROR, format, args);
    va_end(args);
}

/*
 * For now, just emit to stdout.  We'll add file support later.
 */

void CLogger::writeLog(uint8_t level, const char *str) {
    fprintf(stdout, "%s%s[%i] %s: %s\n",
            ts(), progName, progPID, levels[level], str);
}
