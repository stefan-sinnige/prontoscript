/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the ProntoScript re-implementation October 4, 2025.
 *
 * The Initial Developer of the Original Code is Stefan Sinnige.
 * Portions created by the Initial Developer are Copyright (C) 2025
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

#include "jslock.h"
#include "psselect.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#define NS_PER_SEC 1000000000
#define NO_TIMEOUT
/*
 * The event structure.
 */
typedef struct _PSSelectEvent
{
    int fd;
    PSFDSet fdsetmask;
    JSObject* obj;
    PSSelectCallback func;
    PSSelectCallback errfunc;
    struct timespec timeout;
    struct _PSSelectEvent* prev;
    struct _PSSelectEvent* next;
} PSSelectEvent;

/*
 * The list of events to be monitored and its access lock.
 */
static PSSelectEvent* ps_Events = NULL;
#ifdef JS_THREADSAFE
static JSLock *ps_EventsLock = NULL;
#else
static void* ps_EventsLock = NULL;
#endif

/*
 * Dealing with no-timeout specification. A no timeout is defined by having
 * both sec and nsec set to '-1'.
 */

int
no_timeout(struct timespec timeout)
{
    return (timeout.tv_sec == -1) && (timeout.tv_nsec == -1);
}

struct timespec
set_no_timeout()
{
    struct timespec ts = {-1, -1};
    return ts;
}

/*
 * Calculate the difference between two timespec's.
 */
struct timespec
diff_timespec(struct timespec start, struct timespec end)
{
    struct timespec diff;
    diff.tv_sec = end.tv_sec - start.tv_sec;
    diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    if (diff.tv_nsec < 0) {
        diff.tv_nsec += NS_PER_SEC;
        --diff.tv_sec;
    }
    return diff;
}

/*
 * Subtract a duration from a timeout.
 */
void
update_timeout(struct timespec duration, struct timespec* timeout)
{
    /* If the timeout is not defined, leave it untouched. */
    if (no_timeout(*timeout)) {
        return;
    }

    /* Subtract */
    timeout->tv_sec -= duration.tv_sec;
    timeout->tv_nsec -= duration.tv_nsec;
    if (timeout->tv_nsec < 0) {
        timeout->tv_nsec += NS_PER_SEC;
        --timeout->tv_sec;
    }

    /* If negative, the timeout will be set to 0. */
    if (timeout->tv_sec < 0) {
        timeout->tv_sec = 0;
        timeout->tv_nsec = 0;
    }
}

/*
 * Return the minimum of two timeouts.
 */
struct timespec
minimum_timeout(struct timespec first, struct timespec second)
{
    /* If one is no-timeout, return the other one. */
    if (no_timeout(first)) {
        return second;
    }
    if (no_timeout(second)) {
        return first;
    }

    /* Otherwise, compare their values */
    if ((first.tv_sec < second.tv_sec) ||
        (first.tv_sec == second.tv_sec) && (first.tv_nsec < second.tv_nsec))
    {
        return first;
    }
    else
    {
        return second;
    }
}

/*
 * Initialise the select mechanism.
 */
JSBool
ps_InitSelect(JSContext *cx)
{
#ifdef JS_THREADSAFE
    if (!ps_EventsLock) {
        ps_EventsLock = JS_NEW_LOCK();
        if (!ps_EventsLock) {
            return JS_FALSE;
        }
    }
#endif
    return JS_TRUE;
}

/*
 * Destroy the select mechanism.
 */
void
ps_DestroySelect(JSContext *cx)
{
#ifdef JS_THREADSAFE
    if (ps_EventsLock) {
        JS_DESTROY_LOCK(ps_EventsLock);
        ps_EventsLock = NULL;
    }
#endif
}

JSBool
ps_HandleSelect(JSContext *cx)
{
    fd_set rdfs, wrfs, erfs;
    int max = 0, result;
    struct timeval tv, *ptv;
    struct timespec timeout, start, end, duration;

    /* If there are no events to monitor, then there is nothing to do. */
    if (ps_Events == NULL) {
        return JS_FALSE;
    }

    /* Set up the select */
    timeout = set_no_timeout();
    FD_ZERO(&rdfs);
    FD_ZERO(&wrfs);
    FD_ZERO(&erfs);
    JS_ACQUIRE_LOCK(ps_EventsLock);
    for (PSSelectEvent* ev = ps_Events; ev != NULL; ev = ev->next) {
        /* Add the file-descriptor */
        if (ev->fdsetmask & PSFDSET_READ) {
            FD_SET(ev->fd, &rdfs);
        }
        if (ev->fdsetmask & PSFDSET_WRITE) {
            FD_SET(ev->fd, &wrfs);
        }
        FD_SET(ev->fd, &erfs);

        /* Keep track of the maximum file-descriptior. */
        if (max < ev->fd) {
            max = ev->fd;
        }

        /* Keep track of the minimum timeout */
        timeout = minimum_timeout(timeout, ev->timeout);
        if ((ev->timeout.tv_sec < tv.tv_sec) ||
           ((ev->timeout.tv_sec == tv.tv_sec) && (ev->timeout.tv_nsec < tv.tv_usec * 1000)))
        {
            tv.tv_sec = ev->timeout.tv_sec;
            tv.tv_usec = ev->timeout.tv_nsec * 1000;
        }
    }
    JS_RELEASE_LOCK(ps_EventsLock);

    /* Calculate the timeout, and use NULL if no timeout is defined */
    if (no_timeout(timeout)) {
        ptv = NULL;
    }
    else {
        tv.tv_sec = timeout.tv_sec;
        tv.tv_usec = timeout.tv_nsec * 1000;
        ptv = &tv;
    }

    /* Perform the selection */
    clock_gettime(CLOCK_MONOTONIC, &start);
    result = select(max+1, &rdfs, &wrfs, &erfs, ptv);
    clock_gettime(CLOCK_MONOTONIC, &end);
    duration = diff_timespec(start, end);

    /* Update all timeouts and copy all triggered, errored and timed-out
     * events */
    PSSelectEvent *triggered = NULL;
    PSSelectEvent *timedout = NULL;
    PSSelectEvent *errored = NULL;
    JS_ACQUIRE_LOCK(ps_EventsLock);
    for (PSSelectEvent* ev = ps_Events; ev != NULL; ev = ev->next) {
        /* Update the timeout of this event. */
        update_timeout(duration, &(ev->timeout));

        /* Check if this file descriptor was triggered */
        if (FD_ISSET(ev->fd, &rdfs) || FD_ISSET(ev->fd, &wrfs)) {
            PSSelectEvent* copy = (PSSelectEvent*)malloc(sizeof(PSSelectEvent));
            copy->fd = ev->fd;
            copy->obj = ev->obj;
            copy->func = ev->func;
            copy->errfunc = ev->errfunc;
            copy->timeout = ev->timeout; 
            copy->next = triggered;
            triggered = copy;
        }

        /* Check if this file descriptor was timed out */
        if (ev->timeout.tv_sec == 0 && ev->timeout.tv_nsec == 0) {
            PSSelectEvent* copy = (PSSelectEvent*)malloc(sizeof(PSSelectEvent));
            copy->fd = ev->fd;
            copy->obj = ev->obj;
            copy->func = ev->func;
            copy->errfunc = ev->errfunc;
            copy->timeout = ev->timeout; 
            copy->next = timedout;
            timedout = copy;
        }

        /* Check if this file descriptor was errored */
        if (FD_ISSET(ev->fd, &erfs)) {
            PSSelectEvent* copy = (PSSelectEvent*)malloc(sizeof(PSSelectEvent));
            copy->fd = ev->fd;
            copy->obj = ev->obj;
            copy->func = ev->func;
            copy->errfunc = ev->errfunc;
            copy->timeout = ev->timeout; 
            copy->next = errored;
            errored = copy;
        }

    }
    JS_RELEASE_LOCK(ps_EventsLock);

    /* Handle the result */
    if (result > 0) {
        for (PSSelectEvent* ev = triggered; ev != NULL; ev = ev->next) {
            ev->func(cx, ev->obj);
        }
    }
    else
    if (result == 0) {
        for (PSSelectEvent* ev = timedout; ev != NULL; ev = ev->next) {
            ev->func(cx, ev->obj);
        }
    }
    else {
        for (PSSelectEvent* ev = errored; ev != NULL; ev = ev->next) {
            ev->errfunc(cx, ev->obj);
        }
    }

    /* Cleanup the temporary copies */
    for (PSSelectEvent* ev = triggered; ev != NULL; /* Updated inside */) {
        PSSelectEvent* next = ev->next;
        free(next);
        ev = next;
    }
    for (PSSelectEvent* ev = timedout; ev != NULL; /* Updated inside */) {
        PSSelectEvent* next = ev->next;
        free(next);
        ev = next;
    }
    for (PSSelectEvent* ev = errored; ev != NULL; /* Updated inside */) {
        PSSelectEvent* next = ev->next;
        free(next);
        ev = next;
    }

    return JS_TRUE;
}

JSBool
ps_AddSelect(JSContext *cx, int fd, PSFDSet fdsetmask, JSObject* obj,
             PSSelectCallback func, PSSelectCallback errfunc,
             int timeout)
{
    PSSelectEvent* event;

    /* Create a new event structure */
    event = (PSSelectEvent*) JS_malloc(cx, sizeof(PSSelectEvent));
    if (!event) {
        return JS_FALSE;
    }
    event->fd = fd;
    event->fdsetmask = fdsetmask;
    event->obj = obj;
    event->func = func;
    event->errfunc = errfunc;
    event->prev = NULL;
    event->next = NULL;

    /* Set the timeout, assume that '-1' means no timeout. */
    if (timeout == -1) {
        event->timeout = set_no_timeout();
    }
    else {
        event->timeout.tv_sec = timeout / 1000;
        event->timeout.tv_nsec = (timeout % 1000) * 1000000;
    }

    /* Remove any existing event with matching file-descriptor. */
    ps_RemoveSelect(cx, fd);

    /* Add the new event to the list. */
    JS_ACQUIRE_LOCK(ps_EventsLock);
    if (ps_Events != NULL) {
        event->next = ps_Events;
        ps_Events->prev = event;
    }
    ps_Events = event;
    JS_RELEASE_LOCK(ps_EventsLock);

    return JS_TRUE;
}

JSBool
ps_RemoveSelect(JSContext *cx, int fd)
{
    /* Remove any existing event with matching file-descriptor. */
    JS_ACQUIRE_LOCK(ps_EventsLock);
    for (PSSelectEvent* ev = ps_Events; ev != NULL; ev = ev->next) {
        if (ev->fd == fd) {
            if (ev->prev != NULL) {
                ev->prev = ev->next;
            }
            if (ev->next != NULL) {
                ev->next->prev = ev->prev;
            }
            if (ev == ps_Events) {
                ps_Events = ev->next;
            }
            JS_free(cx, ev);
        }
    }
    JS_RELEASE_LOCK(ps_EventsLock);
    return JS_TRUE;
}

