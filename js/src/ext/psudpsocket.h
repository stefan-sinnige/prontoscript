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

#ifndef psudpsocket_h___
#define psudpsocket_h___

#include "jsprvtd.h"
#include "jspubtd.h"

/*
 * ProntoScipt UDPSocket class.
 */

JS_BEGIN_EXTERN_C

/* Initialise the JavaScript 'UDPSocket' class.  */
extern JSObject *
ps_InitUDPSocketClass(JSContext *cx, JSObject *obj);

JS_END_EXTERN_C

#endif /* psudpsocket_h___ */
