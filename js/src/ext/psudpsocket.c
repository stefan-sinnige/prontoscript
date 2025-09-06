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
#include "psudpsocket.h"
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
static JSBool UDPSocket_GetProperty(JSContext*, JSObject*, jsval, jsval*);
static JSBool UDPSocket_SetProperty(JSContext*, JSObject*, jsval, jsval*);
static JSBool UDPSocket_Connect(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool UDPSocket_Close(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool UDPSocket_Read(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool UDPSocket_Send(JSContext*, JSObject*, uintN, jsval*, jsval*);
static JSBool UDPSocket_CT(JSContext*, JSObject*, uintN, jsval*, jsval*);
static void   UDPSocket_DT(JSContext*, JSObject*);
static void   UDPSocket_SelectCallback(JSContext*, JSObject*);
static void   UDPSocket_SelectErrorCallback(JSContext*, JSObject*);
static JSBool UDPSocket_Invoke(JSContext*, JSObject*, jsval, uintN, jsval*);

/*
 * The UDPSocket socket class private instance data.
 */

typedef struct {
    JSBool blocking;        /* True if blocking IO is to be used. */
    jsval onData;           /* The on-data callback function. */
    jsval onIOError;        /* The on-error callback function. */
    int fd;                 /* The socket file descriptor, or -1. */
    int port;               /* The port number, or -1. */
} UDPSocket;

/**
 * Definition of the class properties
 */
enum udpsocket_tinyid {
    UDPSOCKET_ONDATA = -1,
    UDPSOCKET_ONIOERROR = -2
};

#define UDPSOCKET_PROP_ATTRS (JSPROP_PERMANENT)

static JSPropertySpec udpsocket_props[] = {
    /* { name, tinyid, flags, getter, setter } */
    {"onData", UDPSOCKET_ONDATA, UDPSOCKET_PROP_ATTRS , 0, 0},
    {"onIOError", UDPSOCKET_ONIOERROR, UDPSOCKET_PROP_ATTRS , 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * Definition of the class methods
 */
static JSFunctionSpec udpsocket_methods[] = {
    /* { name, call, nargs, flags, extra } */
    {"close", UDPSocket_Close, 0, 0, 0},
    {"send", UDPSocket_Send, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * Definition of the class
 */
static JSClass udpsocket_class = {
    ps_UDPSocket_str,               /* name */
    JSCLASS_HAS_PRIVATE,            /* flags */
    JS_PropertyStub,                /* add property */
    JS_PropertyStub,                /* del property */
    UDPSocket_GetProperty,          /* get property */
    UDPSocket_SetProperty,          /* set property */
    JS_EnumerateStub,               /* enumerate */
    JS_ResolveStub,                 /* resolve */
    JS_ConvertStub,                 /* convert */
    UDPSocket_DT,                   /* finalize */
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/*
 * Create and destroy the socket instance.
 */

UDPSocket*
UDPSocket_New(JSContext *cx, JSBool blocking)
{
    UDPSocket *udp = NULL;
    udp = (UDPSocket*) JS_malloc(cx, sizeof(UDPSocket));
    if (!udp) {
        return NULL;
    }
    udp->onData = JSVAL_VOID;
    udp->onIOError = JSVAL_VOID;
    udp->fd = -1;
    udp->port = -1;
    return udp;
}

void
UDPSocket_Delete(JSContext* cx, UDPSocket* udp)
{
    if (udp->fd != -1) {
        close(udp->fd);
    }
    JS_free(cx, udp);
}

static JSBool
UDPSocket_GetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{   
    UDPSocket* udp = NULL;
    jsint slot;

    /* Get the property's slot */
    if (!JSVAL_IS_INT(id)) {
        return JS_TRUE;
    }
    slot = JSVAL_TO_INT(id);

    /* Get the value */
    JS_LOCK_OBJ(cx, obj);
    udp = (UDPSocket*)JS_GetInstancePrivate(cx, obj, &udpsocket_class, NULL);
    if (udp) {
        switch (slot) {
            case UDPSOCKET_ONDATA:
                *vp = udp->onData;
                break;
            case UDPSOCKET_ONIOERROR:
                *vp = udp->onIOError;
                break;
            default:
                break;
        }
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

static JSBool
UDPSocket_SetProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    UDPSocket* udp = NULL;
    jsint slot;

    /* Get the property's slot */
    if (!JSVAL_IS_INT(id)) {
        return JS_TRUE;
    }
    slot = JSVAL_TO_INT(id);

    /* Set the value */
    JS_LOCK_OBJ(cx, obj);
    udp = (UDPSocket*)JS_GetInstancePrivate(cx, obj, &udpsocket_class, NULL);
    switch (slot) {
        case UDPSOCKET_ONDATA:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                udp->onData = *vp;
            }
            break;
        case UDPSOCKET_ONIOERROR:
            if (JSVAL_IS_FUNCTION(cx, *vp)) {
                udp->onIOError = *vp;
            }
            break;
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

/*
 * Callback when the file descriptor has been triggered. That means that there
 * is data available on the socket.
 */
static void
UDPSocket_SelectCallback(JSContext *cx, JSObject *obj)
{
    UDPSocket* udp = NULL;
    JSString* data;
    JSString* host;
    jsval func;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    ssize_t nread;

    udp = (UDPSocket*) JS_GetPrivate(cx, obj);
    if (!udp) {
        return;
    }

    /* 
     * Receive the datagram.
     */
    memset(&addr, 0, sizeof(addr));
    data = JS_NewGrowableString(cx, NULL, 0);
    while (1) {
        char buf[256];
        nread = recvfrom(udp->fd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&addr, &len);
        if (nread < 0) {
            if (errno == EAGAIN) {
                /* No more data */
                break;
            }
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_SOCKET_ERROR);
            return;
        }
        data = JS_ConcatStrings(cx, data, JS_NewStringCopyN(cx, buf, nread));
    }
    host = JS_NewStringCopyZ(cx, inet_ntoa(addr.sin_addr));

    /*
     * The select callback would indicate that data is ready to be received.
     */

    uintN argc = 3;
    jsval argv[3];
    argv[0] = STRING_TO_JSVAL(data);
    argv[1] = STRING_TO_JSVAL(host);
    argv[2] = INT_TO_JSVAL(ntohs(addr.sin_port));
    func = udp->onData;

    /*
     * Invoke the callback
     */
    UDPSocket_Invoke(cx, obj, func, argc, argv);
}

/*
 * Callback when the file descriptor has triggered an error.
 */
static void
UDPSocket_SelectErrorCallback(JSContext *cx, JSObject *obj)
{
    UDPSocket* udp = NULL;
    jsval rval;
    jsval func;
    uintN argc = 0;
    jsval argv[1];

    udp = (UDPSocket*) JS_GetPrivate(cx, obj);
    if (!udp) {
        return;
    }

    /*
     * Invoke the callback
     */
    UDPSocket_Invoke(cx, obj, udp->onIOError, argc, argv);

    /* Ensure to close the socket. */
    UDPSocket_Close(cx, obj, 0, NULL, &rval);
}

/*
 * Invoke a callback function.
 */
static JSBool
UDPSocket_Invoke(JSContext *cx, JSObject *obj, jsval fun, uintN argc,
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
 *      UDPSocket(port)
 * Purpose:
 *      Create a new UDPSocket instance.
 * Parameters:
 *      port    Integer (opt)
 *          The local orignating port when sending UDP packets and the port
 *          used when listening to incoming UDP packets. If omitted, the socket
 *          will not listen for incoming UDP packets. Also, when sending UDP
 *          packets, an unspecified originating port is used.
 * Returns:
 *      A new UDPSocket instance.
 * Exceptions:
 *      Arguments is not a positive integer.
 *      Argument out of range
 *      Socket error
 */
static JSBool
UDPSocket_CT(JSContext* cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_TRUE;
    UDPSocket* udp = NULL;
    JSUint16 port;
    JSString* str;
    JSBool blocking = JS_FALSE;
    struct sockaddr_in addr;
    int flags;

    /* Create the object */
    if (!obj) {
        obj = js_NewObject(cx, &udpsocket_class, NULL, NULL);
        if (!obj) {
            return JS_FALSE;
        }
    }
    udp = UDPSocket_New(cx, blocking);

    /* Get the optional 'port' argument */
    if (argc >= 0) {
        if (JS_TypeOfValue(cx, argv[0]) != JSTYPE_NUMBER) {
            port = JSVAL_TO_INT(argv[0]);
        }
        else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_ARGUMENT_NOT_INT);
            return JS_FALSE;
        }
    }

    /* Create the UDP socket */
    udp->fd = socket(AF_INET, SOCK_DGRAM, 0);
    udp->port = port;
    if (udp->fd < 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, PSMSG_SOCKET_ERROR);
        return JS_FALSE;
    }

    /* If a port has been specified, bind to it. */
    if (udp->port != -1) {
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(udp->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_SOCKET_ERROR);
            return JS_FALSE;
        }
    }

    /* Always use non-blocking IO for UDP sockets */
    if ((flags = fcntl(udp->fd, F_GETFL, 0)) < 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_SOCKET_ERROR);
        return JS_FALSE;
    }
    flags |= O_NONBLOCK;
    if (fcntl(udp->fd, F_SETFL, O_NONBLOCK, flags) < 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_SOCKET_ERROR);
        return JS_FALSE;
    }

    /* Add the file descriptor to the asynchroneous select mechanism that
     * will be triggered when UDP packets are available on the socket. */
    if (!ps_AddSelect(cx, udp->fd, PSFDSET_READ,
                      obj, &UDPSocket_SelectCallback,
                      &UDPSocket_SelectErrorCallback, -1))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_FAILED, "asynchronous socket setup");
        return JS_FALSE;
    }

    /* Set the private instance state object */
    JS_LOCK_OBJ(cx, obj);
    ok = JS_SetPrivate(cx, obj, udp);
    JS_LOCK_OBJ(cx, obj);
    if (!ok) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

/**
 * Destructor.
 */
static void
UDPSocket_DT(JSContext* cx, JSObject *obj)
{
    UDPSocket* udp = NULL;
    udp = (UDPSocket*)JS_GetInstancePrivate(cx, obj, &udpsocket_class, NULL);
    if (udp) {
        UDPSocket_Delete(cx, udp);
    }
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
UDPSocket_Close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    UDPSocket* udp = NULL;

    udp = (UDPSocket*) JS_GetPrivate(cx, obj);
    if (!udp) {
        return JS_FALSE;
    }
    if (udp->fd != -1) {
        ps_RemoveSelect(cx, udp->fd);
        close(udp->fd);
        udp->fd = -1;
    }
    return JS_TRUE;
}

/**
 * Synopsis:
 *      send(s, host, port)
 * Purpose:
 *      Send a UDP packet.
 * Parameters:
 *      s       String
 *              The data to be transmitted, may contain binary data.
 *      host    String
 *              The destination IP address of the packet.
 *      port    Integer
 *              The destination UDP IP address of the packet.
 * Exceptions:
 *      Not enough arguments specified
 *      Argument our of range
 *      Argument is not a string
 *      Argument is not an IP address
 *      Argument is not a positive integer number
 *      UDP socket busy
 *      Socket error
 */
static JSBool
UDPSocket_Send(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                  jsval *rval)
{
    UDPSocket* udp = NULL;
    char* peer = NULL;
    JSUint32 ip = 0;
    JSUint16 port = 0;
    JSString* data = NULL;
    struct sockaddr_in addr;
    socklen_t len;
    int result;
    int dotted = 1;
    ssize_t nwritten;

    udp = (UDPSocket*) JS_GetPrivate(cx, obj);
    if (!udp) {
        return JS_FALSE;
    }

    /*
     * Extract the parameters
     */
    if (argc != 3) {
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
    if (JS_TypeOfValue(cx, argv[1]) != JSTYPE_STRING) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_ARGUMENT_NOT_STRING);
        return JS_FALSE;
    }
    peer = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
    if (JS_TypeOfValue(cx, argv[2]) != JSTYPE_NUMBER) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_ARGUMENT_NOT_INT);
        return JS_FALSE;
    }
    port = JSVAL_TO_INT(argv[2]);

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
     * Send the data.
     */
    nwritten = sendto(udp->fd, js_GetStringBytes(data), JSSTRING_LENGTH(data),
                      0, (struct sockaddr*)&addr, sizeof(addr));
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
ps_InitUDPSocketClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &udpsocket_class, UDPSocket_CT, 1,
                         udpsocket_props, udpsocket_methods, NULL, NULL);
    if (!proto) {
        return NULL;
    }
    /* What else */
    return proto;
}
