/*
 * This file is part of the ProntoScript replication distribution
 * (https://github.com/stefan-sinnige/prontoscript)
 *
 * Copyright (C) 2025, Stefan Sinnige <stefan@kalion.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jslock.h"
#include "jstypes.h"
#include "psselect.h"
#include "pstcpsocket.h"
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#include <stdio.h>

/*
 *Forward declarations
 */
static JSBool TCPSocket_GetProperty(JSContext*, JSObject*, jsval, jsval*);
static JSBool TCPSocket_SetProperty(JSContext*, JSObject*, jsval, jsval*);
static JSBool TCPSocket_Connect(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool TCPSocket_Close(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool TCPSocket_Read(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool TCPSocket_Write(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool TCPSocket_CT(JSContext*, JSObject*, uintN, jsval*, jsval*);
static void   TCPSocket_DT(JSContext*, JSObject*);
static void   TCPSocket_SelectCallback(JSContext*, JSObject*);
static void   TCPSocket_SelectErrorCallback(JSContext*, JSObject*);
static JSBool TCPSocket_Invoke(JSContext*, JSObject*, jsval, uintN, jsval*);

/*
 * The TCPSocket socket class private instance data.
 */

typedef enum {
    TCPSTATE_UNCONNECTED,
    TCPSTATE_CONNECTING,
    TCPSTATE_CONNECTED
} TCPSocketState;

typedef struct {
    JSBool blocking;        /* True if blocking IO is to be used. */
    jsval onConnect;        /* The on-connect callback function. */
    jsval onData;           /* The on-data callback function. */
    jsval onClose;          /* The on-close callback function. */
    jsval onIOError;        /* The on-error callback function. */
    int fd;                 /* The socket file descriptor, or -1. */
    TCPSocketState state;   /* Connection state. */
} TCPSocket;

/**
 * Definition of the class properties
 */
enum tcpsocket_tinyid {
    TCPSOCKET_TCPSTATE_CONNECTED = -1,
    TCPSOCKET_ONCONNECT = -2,
    TCPSOCKET_ONDATA = -3,
    TCPSOCKET_ONCLOSE = -4,
    TCPSOCKET_ONIOERROR = -5
};

#define TCPSOCKET_PROP_ATTRS (JSPROP_PERMANENT)

static JSPropertySpec tcpsocket_props[] = {
    /* { name, tinyid, flags, getter, setter } */
    {"connected", TCPSOCKET_TCPSTATE_CONNECTED, TCPSOCKET_PROP_ATTRS | JSPROP_READONLY, 0, 0},
    {"onConnect", TCPSOCKET_ONCONNECT, TCPSOCKET_PROP_ATTRS , 0, 0},
    {"onData", TCPSOCKET_ONDATA, TCPSOCKET_PROP_ATTRS , 0, 0},
    {"onClose", TCPSOCKET_ONCLOSE, TCPSOCKET_PROP_ATTRS , 0, 0},
    {"onIOError", TCPSOCKET_ONIOERROR, TCPSOCKET_PROP_ATTRS , 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * Definition of the class methods
 */
static JSFunctionSpec tcpsocket_methods[] = {
    /* { name, call, nargs, flags, extra } */
    {"connect", TCPSocket_Connect, 0, 0, 0},
    {"close", TCPSocket_Close, 0, 0, 0},
    {"read", TCPSocket_Read, 0, 0, 0},
    {"write", TCPSocket_Write, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * Definition of the class
 */
static JSClass tcpsocket_class = {
    ps_TCPSocket_str,               /* name */
    JSCLASS_HAS_PRIVATE,            /* flags */
    JS_PropertyStub,                /* add property */
    JS_PropertyStub,                /* del property */
    TCPSocket_GetProperty,          /* get property */
    TCPSocket_SetProperty,          /* set property */
    JS_EnumerateStub,               /* enumerate */
    JS_ResolveStub,                 /* resolve */
    JS_ConvertStub,                 /* convert */
    TCPSocket_DT,                   /* finalize */
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/*
 * Create and destroy the socket instance.
 */

TCPSocket*
TCPSocket_New(JSContext *cx, JSBool blocking)
{
    TCPSocket *tcp = NULL;
    tcp = (TCPSocket*) JS_malloc(cx, sizeof(TCPSocket));
    if (!tcp) {
        return NULL;
    }
    tcp->blocking = blocking;
    tcp->onConnect = JSVAL_VOID;
    tcp->onData = JSVAL_VOID;
    tcp->onClose = JSVAL_VOID;
    tcp->onIOError = JSVAL_VOID;
    tcp->fd = -1;
    tcp->state = TCPSTATE_UNCONNECTED;
    return tcp;
}

void
TCPSocket_Delete(JSContext* cx, TCPSocket* tcp)
{
    if (tcp->fd != -1) {
        (void) shutdown(tcp->fd, SHUT_WR);
        (void) close(tcp->fd);
    }
    JS_free(cx, tcp);
}

static JSBool
TCPSocket_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{   
    TCPSocket* tcp = NULL;
    jsint slot;

    /* Get the property's slot */
    if (!JSVAL_IS_INT(id)) {
        return JS_TRUE;
    }
    slot = JSVAL_TO_INT(id);

    /* Get the value */
    JS_LOCK_OBJ(cx, obj);
    tcp = (TCPSocket*)JS_GetInstancePrivate(cx, obj, &tcpsocket_class, NULL);
    if (tcp) {
        switch (slot) {
            case TCPSOCKET_TCPSTATE_CONNECTED:
                *vp = BOOLEAN_TO_JSVAL(tcp->state == TCPSTATE_CONNECTED);
                break;
            case TCPSOCKET_ONCONNECT:
                *vp = tcp->onConnect;
                break;
            case TCPSOCKET_ONDATA:
                *vp = tcp->onData;
                break;
            case TCPSOCKET_ONCLOSE:
                *vp = tcp->onClose;
                break;
            case TCPSOCKET_ONIOERROR:
                *vp = tcp->onIOError;
                break;
            default:
                break;
        }
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

static JSBool
TCPSocket_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    TCPSocket* tcp = NULL;
    jsint slot;

    /* Get the property's slot */
    if (!JSVAL_IS_INT(id)) {
        return JS_TRUE;
    }
    slot = JSVAL_TO_INT(id);

    /* Set the value */
    JS_LOCK_OBJ(cx, obj);
    tcp = (TCPSocket*)JS_GetInstancePrivate(cx, obj, &tcpsocket_class, NULL);
    switch (slot) {
        case TCPSOCKET_TCPSTATE_CONNECTED:
            break;
        case TCPSOCKET_ONCONNECT:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                tcp->onConnect = *vp;
            }
            break;
        case TCPSOCKET_ONDATA:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                tcp->onData = *vp;
            }
            break;
        case TCPSOCKET_ONCLOSE:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                tcp->onClose = *vp;
            }
            break;
        case TCPSOCKET_ONIOERROR:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                tcp->onIOError = *vp;
            }
            break;
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

/*
 * Callback when the file descriptor has been triggered.
 */
static void
TCPSocket_SelectCallback(JSContext *cx, JSObject *obj)
{
    TCPSocket* tcp = NULL;
    jsval func;

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return;
    }

    /*
     * Bail out if blocking.
     */
    if (tcp->blocking) {
        (void) ps_RemoveSelect(cx, tcp->fd);
        return;
    }

    /*
     * Bail out if unconnected.
     */
    if (tcp->state == TCPSTATE_UNCONNECTED) {
        return;
    }

    /*
     * If connecting, check if we are successfully connected. Otherwise,
     * there is data to be read.
     */
    uintN argc = 0;
    jsval argv[1];
    if (tcp->state == TCPSTATE_CONNECTING) {
        /* Connecting: check if we are successfully connected. */
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        memset(&addr, 0, addrlen);
        if (getpeername(tcp->fd, (struct sockaddr*)&addr, &addrlen) == 0) {
            tcp->state = TCPSTATE_CONNECTED;
            func = tcp->onConnect;
            argc = 0;
        }
        else {
            /* Failure to connect, remove the descriptor so we're not
             * triggered over and over again. */
            const char* errmsg = strerror(errno);
            ps_RemoveSelect(cx, tcp->fd);
            tcp->state = TCPSTATE_UNCONNECTED;
            func = tcp->onIOError;
            JSString* data = JS_NewStringCopyZ(cx, errmsg);
            if (!data) {
                return;
            }
            argc = 1;
            argv[0] = STRING_TO_JSVAL(data);
        }
    }
    else {
        /* Connected: expecting data. If there is no data available, assume that
         * the connection is closed by peer. */
        char dummy;
        ssize_t npeek = recv(tcp->fd, &dummy, 1, MSG_PEEK);
        if (npeek == 0) {
            ps_RemoveSelect(cx, tcp->fd);
            (void) shutdown(tcp->fd, SHUT_WR);
            (void) close(tcp->fd);
            tcp->state = TCPSTATE_UNCONNECTED;
            tcp->fd = -1;
            argc = 0;
            func = tcp->onClose;
        }
        else if (npeek < 0) {
            /* Failure to read data, remove the descriptor so we're not
             * triggered over and over again. */
            const char* errmsg = strerror(errno);
            ps_RemoveSelect(cx, tcp->fd);
            tcp->state = TCPSTATE_UNCONNECTED;
            func = tcp->onIOError;
            JSString* data = JS_NewStringCopyZ(cx, errmsg);
            if (!data) {
                return;
            }
            argc = 1;
            argv[0] = STRING_TO_JSVAL(data);
        }
	else {
            argc = 0;
            func = tcp->onData;
        }
    }

    /*
     * By default, set up a selection without timeout if we're connected. This
     * may be overwritten with a user-defined timeout, or canceled, when the
     * callback is called.
     */
    if (tcp->state == TCPSTATE_CONNECTED) {
        if (!ps_AddSelect(cx, tcp->fd, PSFDSET_READ,
                          obj, &TCPSocket_SelectCallback,
                          &TCPSocket_SelectErrorCallback, -1))
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_FAILED, "asynchronous socket setup");
            return;
        }
    }

    /*
     * Invoke the callback
     */
    TCPSocket_Invoke(cx, obj, func, argc, argv);
}

/*
 * Callback when the file descriptor has triggered an error.
 */
static void
TCPSocket_SelectErrorCallback(JSContext *cx, JSObject *obj)
{
    TCPSocket* tcp = NULL;
    jsval rval;
    jsval func;
    uintN argc = 0;
    jsval argv[1];

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return;
    }

    /*
     * Invoke the callback
     */
    TCPSocket_Invoke(cx, obj, tcp->onIOError, argc, argv);

    /* Ensure to close the socket. */
    TCPSocket_Close(cx, obj, 0, NULL, &rval);
}

/*
 * Invoke a callback function.
 */
static JSBool
TCPSocket_Invoke(JSContext *cx, JSObject *obj, jsval fun, uintN argc,
                 jsval *argv)
{
    JSObject* callable = NULL;
    JSStackFrame* fp;
    jsval *sp, *oldsp, rval;
    void *mark;
    JSBool result;

    /* Allocate call stack frame and push the function, object and argument */
    sp = js_AllocStack(cx, 2 + argc, &mark);
    if (!sp) {
        return JS_FALSE;
    }
    *sp++ = fun;
    *sp++ = OBJECT_TO_JSVAL(obj);
    for (int i = 0; i < argc; ++i) {
        *sp++ = argv[i];
    }
    
    /* Lift current frame and call */
    fp = cx->fp;
    oldsp = fp->sp;
    fp->sp = sp;
    result = js_Invoke(cx, argc, JSINVOKE_INTERNAL | JSINVOKE_SKIP_CALLER);

    /* Store the rval and pop the call stack frame */
    rval = fp->sp[-1];
    fp->sp = oldsp;
    js_FreeStack(cx, mark);
    return result;
}

/**
 * Synopsis:
 *      TCPSocket(blocking)
 * Purpose:
 *      Create a new TCPSocket instance.
 * Parameters:
 *      blocking    Boolean (opt)
 *          When true, creates a socket with synchronous (blocking) actions on
 *          connecting, reading and writing. If false or omitted, the socket
 *          operates asynchronously using the callback functions.
 * Returns:
 *      A new TCPSocket instance.
 * Exceptions:
 *      Maximum active socket count reached.
 */
static JSBool
TCPSocket_CT(JSContext* cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_TRUE;
    TCPSocket* tcp = NULL;
    JSString* str;
    JSBool blocking = JS_FALSE;

    /* Create the object */
    if (!obj) {
        obj = js_NewObject(cx, &tcpsocket_class, NULL, NULL);
        if (!obj) {
            return JS_FALSE;
        }
    }

    /* Get the optional 'blocking' argument */
    if (argc >= 0) {
        switch (JS_TypeOfValue(cx, argv[0])) {
            case JSTYPE_NUMBER:
                blocking = JSVAL_IS_INT(argv[0])
                        ? JSVAL_TO_INT(argv[0])
                        : (jsint) *JSVAL_TO_DOUBLE(argv[0]);
                break;
            case JSTYPE_BOOLEAN:
                blocking = JSVAL_TO_BOOLEAN(argv[0]);
                break;
            default:
                break;
        }
    }

    /* Set the private instance state object */
    tcp = TCPSocket_New(cx, blocking);
    JS_LOCK_OBJ(cx, obj);
    ok = JS_SetPrivate(cx, obj, tcp);
    JS_UNLOCK_OBJ(cx, obj);
    if (!ok) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

/**
 * Destructor.
 */
static void
TCPSocket_DT(JSContext* cx, JSObject *obj)
{
    TCPSocket* tcp = NULL;
    tcp = (TCPSocket*)JS_GetInstancePrivate(cx, obj, &tcpsocket_class, NULL);
    if (tcp) {
        TCPSocket_Delete(cx, tcp);
    }
}

/**
 * Synopsis:
 *      connect(ip, port, timeout)
 * Purpose:
 *      Create a connection to a TCP server.
 * Parameter:
 *      ip      String
 *          IP Address or host name to connect to.
 *      port    Integer
 *          Port number to connect to.
 *      timeout Integer (opt)
 *          Maximum time in milliseconds to establish an asynchroneous
 *          connection.
 * Exceptions:
 *      Not enough arguments specified
 *      Argument is not a string
 *      Argument is not an integer
 *      Argument is not a positive number
 *      Failed to connect
 *      Failed
 * Additional Information:
 *      For a synchroneous socket, the fnction returns when the connection is
 *      established, or failed.
 *
 *      For an asynchronous socket, it returns immediately and the onConnect
 *      is called as soon as the connection is effective.
 */
static JSBool
TCPSocket_Connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    TCPSocket* tcp = NULL;
    char* peer = "";
    JSUint32 ip = 0;
    JSUint16 port = 0;
    JSUint32 timeout = 5000;
    struct sockaddr_in addr;
    socklen_t len;
    int result;
    int dotted = 1;

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return JS_FALSE;
    }

    /*
     * Extract the address and port.
     */
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_NOT_ENOUGH_ARGUMENTS);
        return JS_FALSE;
    }
    if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_STRING) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_ARGUMENT_NOT_STRING);
        return JS_FALSE;
    }
    peer = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
    if (JS_TypeOfValue(cx, argv[1]) != JSTYPE_NUMBER) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_ARGUMENT_NOT_INT);
        return JS_FALSE;
    }
    port = JSVAL_TO_INT(argv[1]);
    if (argc == 3) {
        if (JS_TypeOfValue(cx, argv[2]) != JSTYPE_NUMBER) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_ARGUMENT_NOT_INT);
            return JS_FALSE;
        }
        timeout = JSVAL_TO_INT(argv[2]);
    }

    /* 
     * Get the IP address from the peer
     */
    for (char* cp = peer; *cp != '\0'; ++cp) {
        if ((*cp < '0' || *cp > '9' ) && *cp != '.') {
            dotted = 0;
            break;
        }
    }
    if (dotted) {
        ip = inet_addr(peer);
    }
    else {
        struct addrinfo* infos;
        struct addrinfo* info;
        struct addrinfo hint;

        // Define any lookup hints
        memset(&hint, 0, sizeof(struct addrinfo));
        hint.ai_family = AF_INET;
        hint.ai_socktype = SOCK_STREAM;
        hint.ai_protocol = IPPROTO_TCP;

        // Perform the lookup (this is likely to be expensive)
        result = getaddrinfo(peer, NULL, &hint, &infos);
        if (result != 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_FAILED, "lookup error");
            return JS_FALSE;
        }
        for (info = infos; info != NULL; info = info->ai_next)
        {
            // Use the first match
            ip = ((struct sockaddr_in*)info->ai_addr)->sin_addr.s_addr;
            break;
        }
        freeaddrinfo(infos);
    }

    /*
     * Create the address structure.
     */
    len = sizeof(addr);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ip;

    /* 
     * If already connected, close it first.
     */
    if (tcp->state == TCPSTATE_CONNECTED) {
        (void) shutdown(tcp->fd, SHUT_WR);
        (void) close(tcp->fd);
        tcp->state = TCPSTATE_UNCONNECTED;
        tcp->fd = -1;
    }

    /*
     * Create the socket. Set to non-blocking if requested.
     */

    tcp->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (!tcp->blocking) {
        int flags;
        if ((flags = fcntl(tcp->fd, F_GETFL, 0)) < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                PSMSG_SOCKET_ERROR);
            return JS_FALSE;
        }
        flags |= O_NONBLOCK;
        if (fcntl(tcp->fd, F_SETFL, O_NONBLOCK, flags) < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                PSMSG_SOCKET_ERROR);
            return JS_FALSE;
        }
    }

    /*
     * Connect.
     */
    result = connect(tcp->fd, (struct sockaddr*)&addr, len);
    if (result == -1) {
        if (!tcp->blocking && errno == EINPROGRESS) {
            tcp->state = TCPSTATE_CONNECTING;
            if (!ps_AddSelect(cx, tcp->fd, PSFDSET_WRITE,
                              obj, &TCPSocket_SelectCallback,
                              &TCPSocket_SelectErrorCallback, timeout))
            {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     PSMSG_FAILED, "asynchronous socket setup");
                return JS_FALSE;
            }
        }
        else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                PSMSG_FAILED, strerror(errno));
            return JS_FALSE;
        }
    }
    else {
        tcp->state = TCPSTATE_CONNECTED;
    }

    return JS_TRUE;    
}

/**
 * Synopsis:
 *      close()
 * Purpose:
 *      Terminates the connection.
 * Parameters:
 *      None
 * Exceptions:
 *      Socket error
 */
static JSBool
TCPSocket_Close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    TCPSocket* tcp = NULL;

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return JS_FALSE;
    }
    if (tcp->fd != -1) {
        ps_RemoveSelect(cx, tcp->fd);
        (void) shutdown(tcp->fd, SHUT_WR);
        (void) close(tcp->fd);
        tcp->fd = -1;
    }
    return JS_TRUE;
}

/**
 * Synopsis:
 *      read([count[, timeout]])
 * Purpose:
 *      Read data from the socket.
 * Parameters:
 *      count   Integer (opt)
 *              Number of bytes to read. If not specified, read all available
 *              data.
 *      timeout Integer (opt)
 *              Maxixmum time in milliseconds to wait for data to arrive for a
 *              synchronous socket. If omitted, return immediately with the
 *              currently available data.
 * Returns:
 *      String  The available socket data in case of a synchronous socket. For
 *              asynchronous sockets, returns immediately and the onData
 *              callback is called when the data is received.
 * Exceptions:
 *      Argument is not an integer
 *      Argument is not a positive integer
 *      Maximum blocking read length exceeded
 *      Insufficient internal memory available
 *      Socket error
 *      Failed
 */
static JSBool
TCPSocket_Read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    TCPSocket* tcp = NULL;
    JSUint16 count = 65535;
    JSUint32 timeout = 0;
    JSString* data;
    ssize_t nread = 0;

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return JS_FALSE;
    }

    /*
     * Extract the count and timeout.
     */
    if (argc > 0) {
        if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_NUMBER) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_ARGUMENT_NOT_STRING);
            return JS_FALSE;
        }
        count = JSVAL_TO_INT(argv[0]);
        if (argc > 1) {
            if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_NUMBER) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                    PSMSG_ARGUMENT_NOT_STRING);
                return JS_FALSE;
            }
            timeout = JSVAL_TO_INT(argv[0]);
        }
    }

    /*
     * Bail out if not connected.
     */
    if (tcp->state != TCPSTATE_CONNECTED) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_FAILED, "not connected");
        return JS_FALSE;
    }

    /*
     * Read the data.
     */
    data = JS_NewGrowableString(cx, NULL, 0);
    while (nread != count) {
        char buf[256];
        ssize_t buflen;

        /* Determing how many bytes to read, either to fill up the local buffer
         * or the remaining bytes left to read, whichever is the smallest. */
        if ((count -nread) < sizeof(buf)) {
            buflen = count - nread;
        }
        else {
            buflen = sizeof(buf);
        }

        /* Read the buffer and concatenate to the result. */
        buflen = recv(tcp->fd, buf, buflen, 0);
        if (buflen == -1) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                PSMSG_SOCKET_ERROR);
            return JS_FALSE;
        }
        if (buflen == 0) {
            break;
        }
        data = JS_ConcatStrings(cx, data, JS_NewStringCopyN(cx, buf, buflen));
        nread += buflen;
    }

    *rval = STRING_TO_JSVAL(data);
    return JS_TRUE;
}

/**
 * Synopsis:
 *      write()
 * Purpose:
 *      Write data to the socket.
 * Parameters:
 *      data    String
 *              THe data to be transmitted, may contain binary data.
 * Exceptions:
 *      Not enough arguments specified
 *      Socket not ready
 *      Socket error
 */
static JSBool
TCPSocket_Write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    TCPSocket* tcp = NULL;
    JSString* data;
    ssize_t nwritten;

    tcp = (TCPSocket*) JS_GetPrivate(cx, obj);
    if (!tcp) {
        return JS_FALSE;
    }

    /*
     * Extract the string to write.
     */
    if (argc == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_NOT_ENOUGH_ARGUMENTS);
        return JS_FALSE;
    }
    if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_STRING) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_ARGUMENT_NOT_STRING);
        return JS_FALSE;
    }
    data = JSVAL_TO_STRING(argv[0]);

    /*
     * Bail out if not connected.
     */
    if (tcp->state != TCPSTATE_CONNECTED) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_FAILED, "not connected");
        return JS_FALSE;
    }

    /*
     * Send the data.
     */
    nwritten = send(tcp->fd,
                    js_GetStringBytes(data),
                    JSSTRING_LENGTH(data),
                    0);
    if (nwritten == -1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_SOCKET_ERROR);
        return JS_FALSE;
    }
    return JS_TRUE;
}

/**
 * System class initialiser.
 */
JSObject*
ps_InitTCPSocketClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &tcpsocket_class, TCPSocket_CT, 1,
                         tcpsocket_props, tcpsocket_methods, NULL, NULL);
    if (!proto) {
        return NULL;
    }
    /* What else */
    return proto;
}
