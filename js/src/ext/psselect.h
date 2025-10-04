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
