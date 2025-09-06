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

#ifndef psselect_h___
#define psselect_h___

#include "jscntxt.h"

/*
 * ProntoScipt asynchronous handling.
 *
 * Asynchronous handling utilises the `select` mechanism that can wait for
 * multiple file-descriptor events simultaneously. The selecting mechanims is
 * only handled at the end of the script execution and will be in effect while
 * there are still file-desccriptors left to be monitored. The script will
 * typically initialise and start the file-descriptor monitoting, for example,
 * the script will initiate an HTTP connection, while the handling is then
 * handled asynchroneously until the connection has been closed.
 */

JS_BEGIN_EXTERN_C

/* The file descriptor set to register a file-descriptor to. This can be OR'd
 * to register to more than one file-descriptor set. */
typedef enum PSFDSet {
    PSFDSET_READ  = 1 << 0,
    PSFDSET_WRITE = 1 << 1
} PSFDSet;

/* The callback function type when a file-descriptor is triggered. The object
 * refers to the javascript object that owns the file-descriptor. */
typedef void (*PSSelectCallback)(JSContext *cx, JSObject *obj);

/* Initialise the ProntoScript select mechanism. */
JSBool ps_InitSelect(JSContext *cx);

/* Handle any outstanding select calls. Returns JS_TRUE while there are still
 * events to be monitored. */
JSBool ps_HandleSelect(JSContext *cx);

/* Destroy the ProntoScript select mechanism. */
void ps_DestroySelect(JSContext *cx);

/* Add the file-descriptor to the asynchroneous mechanism and register the
 * function to call when the event is triggered. */
JSBool ps_AddSelect(JSContext* cx, int fd, PSFDSet fdsetmask, JSObject *obj,
        PSSelectCallback func, PSSelectCallback errfunc,
        int timeout);

/* Remove the file-descriptor from the asynchroneous mechanism. */
JSBool ps_RemoveSelect(JSContext* cx, int fd);

JS_END_EXTERN_C

#endif /* psselect_h___ */
