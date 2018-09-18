/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// C/C++ logging functions.  See the logging documentation for API details.
//
// We'd like these to be available from C code (in case we import some from
// somewhere), so this has a C interface.
//
// The output will be correct when the log file is shared between multiple
// threads and/or multiple processes so long as the operating system
// supports O_APPEND.  These calls have mutex-protected data structures
// and so are NOT reentrant.  Do not use LOG in a signal handler.
//
#ifndef _LIBS_LOGKIT_H
#define _LIBS_LOGKIT_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_PTHREADS
#include <pthread.h>
#endif
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * log priority values, in ascending priority order.
 */
typedef enum logkit_LogPriority {
    LOGKIT_LOG_UNKNOWN = 0,
    LOGKIT_LOG_DEFAULT,    /* only for SetMinPriority() */
    LOGKIT_LOG_VERBOSE,
    LOGKIT_LOG_DEBUG,
    LOGKIT_LOG_INFO,
    LOGKIT_LOG_WARN,
    LOGKIT_LOG_ERROR,
    LOGKIT_LOG_FATAL,
    LOGKIT_LOG_SILENT,     /* only for SetMinPriority(); must be last */
} logkit_LogPriority;


void __logkit_assert(const char *cond, const char *tag, const char *fmt, ...);
int __logkit_print(int prio, const char *tag, const char *fmt, ...);
int __logkit_vprint(int prio, const char *tag, const char *fmt, va_list ap);
int __logkit_write(int prio, const char *tag, const char *msg);

// ---------------------------------------------------------------------

/*
 * Normally we strip ALOGV (VERBOSE messages) from release builds.
 * You can modify this (for example with "#define LOG_NDEBUG 0"
 * at the top of your source file) to change that behavior.
 */
#ifndef LOG_NDEBUG
#ifdef NDEBUG
#define LOG_NDEBUG 1
#else
#define LOG_NDEBUG 0
#endif
#endif

/*
 * This is the local tag used for the following simplified
 * logging macros.  You can change this preprocessor definition
 * before using the other macros to change the tag.
 */
#ifndef LOG_TAG
#define LOG_TAG NULL
#endif

// ---------------------------------------------------------------------

/*
 * Simplified macro to send a verbose log message using the current LOG_TAG.
 */
#ifndef ALOGV
#if LOG_NDEBUG
#define ALOGV(...)   ((void)0)
#else
#define ALOGV(...) ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

#ifndef ALOGV_IF
#if LOG_NDEBUG
#define ALOGV_IF(cond, ...)   ((void)0)
#else
#define ALOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

/*
 * Simplified macro to send a debug log message using the current LOG_TAG.
 */
#ifndef ALOGD
#define ALOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGD_IF
#define ALOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an info log message using the current LOG_TAG.
 */
#ifndef ALOGI
#define ALOGI(...) ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGI_IF
#define ALOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send a warning log message using the current LOG_TAG.
 */
#ifndef ALOGW
#define ALOGW(...) ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGW_IF
#define ALOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an error log message using the current LOG_TAG.
 */
#ifndef ALOGE
#define ALOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#ifndef ALOGE_IF
#define ALOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

// ---------------------------------------------------------------------

/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * verbose priority.
 */
#ifndef IF_ALOGV
#if LOG_NDEBUG
#define IF_ALOGV() if (false)
#else
#define IF_ALOGV() IF_ALOG(LOG_VERBOSE, LOG_TAG)
#endif
#endif

/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * debug priority.
 */
#ifndef IF_ALOGD
#define IF_ALOGD() IF_ALOG(LOG_DEBUG, LOG_TAG)
#endif

/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * info priority.
 */
#ifndef IF_ALOGI
#define IF_ALOGI() IF_ALOG(LOG_INFO, LOG_TAG)
#endif

/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * warn priority.
 */
#ifndef IF_ALOGW
#define IF_ALOGW() IF_ALOG(LOG_WARN, LOG_TAG)
#endif

/*
 * Conditional based on whether the current LOG_TAG is enabled at
 * error priority.
 */
#ifndef IF_ALOGE
#define IF_ALOGE() IF_ALOG(LOG_ERROR, LOG_TAG)
#endif


// ---------------------------------------------------------------------

/*
 * Simplified macro to send a verbose system log message using the current LOG_TAG.
 */
#ifndef SLOGV
#if LOG_NDEBUG
#define SLOGV(...)   ((void)0)
#else
#define SLOGV(...) ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

#ifndef SLOGV_IF
#if LOG_NDEBUG
#define SLOGV_IF(cond, ...)   ((void)0)
#else
#define SLOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

/*
 * Simplified macro to send a debug system log message using the current LOG_TAG.
 */
#ifndef SLOGD
#define SLOGD(...) ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef SLOGD_IF
#define SLOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an info system log message using the current LOG_TAG.
 */
#ifndef SLOGI
#define SLOGI(...) ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef SLOGI_IF
#define SLOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send a warning system log message using the current LOG_TAG.
 */
#ifndef SLOGW
#define SLOGW(...) ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef SLOGW_IF
#define SLOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an error system log message using the current LOG_TAG.
 */
#ifndef SLOGE
#define SLOGE(...) ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#ifndef SLOGE_IF
#define SLOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_SYSTEM, LOGKIT_LOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

// ---------------------------------------------------------------------

/*
 * Simplified macro to send a verbose radio log message using the current LOG_TAG.
 */
#ifndef RLOGV
#if LOG_NDEBUG
#define RLOGV(...)   ((void)0)
#else
#define RLOGV(...) ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))
#endif
#endif

#define CONDITION(cond)     (__builtin_expect((cond)!=0, 0))

#ifndef RLOGV_IF
#if LOG_NDEBUG
#define RLOGV_IF(cond, ...)   ((void)0)
#else
#define RLOGV_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif
#endif

/*
 * Simplified macro to send a debug radio log message using the current LOG_TAG.
 */
#ifndef RLOGD
#define RLOGD(...) ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif

#ifndef RLOGD_IF
#define RLOGD_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_DEBUG, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an info radio log message using the current LOG_TAG.
 */
#ifndef RLOGI
#define RLOGI(...) ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_INFO, LOG_TAG, __VA_ARGS__))
#endif

#ifndef RLOGI_IF
#define RLOGI_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_INFO, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send a warning radio log message using the current LOG_TAG.
 */
#ifndef RLOGW
#define RLOGW(...) ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_WARN, LOG_TAG, __VA_ARGS__))
#endif

#ifndef RLOGW_IF
#define RLOGW_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_WARN, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif

/*
 * Simplified macro to send an error radio log message using the current LOG_TAG.
 */
#ifndef RLOGE
#define RLOGE(...) ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#ifndef RLOGE_IF
#define RLOGE_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)__logkit_buf_print(LOG_ID_RADIO, LOGKIT_LOG_ERROR, LOG_TAG, __VA_ARGS__)) \
    : (void)0 )
#endif


// ---------------------------------------------------------------------

/*
 * Log a fatal error.  If the given condition fails, this stops program
 * execution like a normal assertion, but also generating the given message.
 * It is NOT stripped from release builds.  Note that the condition test
 * is -inverted- from the normal assert() semantics.
 */
#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) \
    ( (CONDITION(cond)) \
    ? ((void)logkit_printAssert(#cond, LOG_TAG, ## __VA_ARGS__)) \
    : (void)0 )
#endif

#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(...) \
    ( ((void)logkit_printAssert(NULL, LOG_TAG, ## __VA_ARGS__)) )
#endif

/*
 * Versions of LOG_ALWAYS_FATAL_IF and LOG_ALWAYS_FATAL that
 * are stripped out of release builds.
 */
#if LOG_NDEBUG

#ifndef LOG_FATAL_IF
#define LOG_FATAL_IF(cond, ...) ((void)0)
#endif
#ifndef LOG_FATAL
#define LOG_FATAL(...) ((void)0)
#endif

#else

#ifndef LOG_FATAL_IF
#define LOG_FATAL_IF(cond, ...) LOG_ALWAYS_FATAL_IF(cond, ## __VA_ARGS__)
#endif
#ifndef LOG_FATAL
#define LOG_FATAL(...) LOG_ALWAYS_FATAL(__VA_ARGS__)
#endif

#endif

/*
 * Assertion that generates a log message when the assertion fails.
 * Stripped out of release builds.  Uses the current LOG_TAG.
 */
#ifndef ALOG_ASSERT
#define ALOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ## __VA_ARGS__)
//#define ALOG_ASSERT(cond) LOG_FATAL_IF(!(cond), "Assertion failed: " #cond)
#endif

// ---------------------------------------------------------------------

/*
 * Basic log message macro.
 *
 * Example:
 *  ALOG(LOG_WARN, NULL, "Failed with error %d", errno);
 *
 * The second argument may be NULL or "" to indicate the "global" tag.
 */
#ifndef ALOG
#define ALOG(priority, tag, ...) \
    LOG_PRI(LOGKIT_##priority, tag, __VA_ARGS__)
#endif

/*
 * Log macro that allows you to specify a number for the priority.
 */
#ifndef LOG_PRI
#define LOG_PRI(priority, tag, ...) \
    logkit_printLog(priority, tag, __VA_ARGS__)
#endif

/*
 * Log macro that allows you to pass in a varargs ("args" is a va_list).
 */
#ifndef LOG_PRI_VA
#define LOG_PRI_VA(priority, tag, fmt, args) \
    logkit_vprintLog(priority, NULL, tag, fmt, args)
#endif

/*
 * Conditional given a desired logging priority and tag.
 */
#ifndef IF_ALOG
#define IF_ALOG(priority, tag) \
    if (logkit_testLog(LOGKIT_##priority, tag))
#endif

// ---------------------------------------------------------------------
/*
 * ===========================================================================
 *
 * The stuff in the rest of this file should not be used directly.
 */

#define logkit_printLog(prio, tag, fmt...) \
    __logkit_print(prio, tag, fmt)

#define logkit_vprintLog(prio, cond, tag, fmt...) \
    __logkit_vprint(prio, tag, fmt)

/* XXX Macros to work around syntax errors in places where format string
 * arg is not passed to ALOG_ASSERT, LOG_ALWAYS_FATAL or LOG_ALWAYS_FATAL_IF
 * (happens only in debug builds).
 */

/* Returns 2nd arg.  Used to substitute default value if caller's vararg list
 * is empty.
 */
#define __logkit_second(dummy, second, ...)     second

/* If passed multiple args, returns ',' followed by all but 1st arg, otherwise
 * returns nothing.
 */
#define __logkit_rest(first, ...)               , ## __VA_ARGS__

#define logkit_printAssert(cond, tag, fmt...) \
    __logkit_assert(cond, tag, \
        __logkit_second(0, ## fmt, NULL) __logkit_rest(fmt))

#define logkit_writeLog(prio, tag, text) \
    __logkit_write(prio, tag, text)

#define logkit_bWriteLog(tag, payload, len) \
    __logkit_bwrite(tag, payload, len)
#define logkit_btWriteLog(tag, type, payload, len) \
    __logkit_btwrite(tag, type, payload, len)

// TODO: remove these prototypes and their users
#define logkit_testLog(prio, tag) (1)
#define logkit_writevLog(vec,num) do{}while(0)
#define logkit_write1Log(str,len) do{}while (0)
#define logkit_setMinPriority(tag, prio) do{}while(0)
//#define logkit_logToCallback(func) do{}while(0)
#define logkit_logToFile(tag, file) (0)
#define logkit_logToFd(tag, fd) (0)

typedef enum {
    LOG_ID_MAIN = 0,
    LOG_ID_RADIO = 1,
    LOG_ID_EVENTS = 2,
    LOG_ID_SYSTEM = 3,

    LOG_ID_MAX
} log_id_t;

/*
 * Send a simple string to the log.
 */
int __logkit_buf_write(int bufID, int prio, const char *tag, const char *text);
int __logkit_buf_print(int bufID, int prio, const char *tag, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif // _LIBS_CUTILS_LOG_H
