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

#include "jsapi.h"
#include "jscntxt.h"
#include "jsstr.h"
#include "pssystem.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

extern FILE *gOutFile;

/**
 * Synopsis:
 *      System.include(name)
 * Purpose:
 *      Causes a library script to be included. This library script will be
 *      executed, causing the classes and variables declared in that library
 *      script to become available in the global scope.
 * Parameters:
 *      name String
 *           Filename of the library script.
 * Exceptions:
 *      No argument specified
 *      Invalid name
 */
static JSBool
System_Include(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    /* Access the (formerly static) 'Process' function from 'js.c' to execute
     * the specified file and the module path. */
    extern void Process(JSContext*, JSObject*, char*);
    extern const char* gModulePath;

    JSString* str;
    char* name;
    struct stat statbuf;
    char path[255];

    /* Get the filename */
    str = JS_ValueToString(cx, argv[0]);
    name = js_GetStringBytes(str);

    /* Locate the file, either in current working directory or in the module
     * path. */
    strlcpy(path, name, sizeof(path));
    if (stat(path, &statbuf) != 0) {
        if (gModulePath != NULL) {
            strlcpy(path, gModulePath, sizeof(path));
            strlcat(path, "/", sizeof(path));
            strlcat(path, name, sizeof(path));
            if (stat(path, &statbuf) != 0) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     PSMSG_INVALID_NAME);
                return JS_FALSE;
            }
        }
        else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 PSMSG_INVALID_NAME);
            return JS_FALSE;
        }
    }

    /* Execute the script with the global object scope. */
    Process(cx, cx->globalObject,  path);
    return JS_TRUE;
}

/**
 * Synopsis:
 *      System.print(s)
 * Purpose:
 *      Display a debug message on the debug output panel.
 * Parameters:
 *      s    String
 *           Text to be displayed.
 * Exceptions:
 *      None
 * Additional Info:
 *      The debug panel is a panel or button tagged "_PS_DEBUG_". Use "\n" to
 *      insert line breaks in the text output.
 *
 *      In standalone engine, the debug panel is the default output file.
 */
static JSBool
System_Print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    /* Access the (formerly static) 'Print' function from 'js.c' to print to
     * standard output. */
    extern JSBool Print(JSContext*, JSObject*, uintN, jsval*, jsval*);
    return Print(cx, obj, argc, argv, rval);
}

/**
 * Definition of the class
 */
static JSClass system_class = {
    ps_System_str,                  /* name */
    0,                              /* flags */
    JS_PropertyStub,                /* add property */
    JS_PropertyStub,                /* del property */
    JS_PropertyStub,                /* get property */
    JS_PropertyStub,                /* set property */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/**
 * Definition of the static class methods
 */
static JSFunctionSpec system_static_methods[] = {
    /* { name, call, nargs, flags, extra } */
    {"include", System_Include, 1, 0, 0},
    {"print",   System_Print,   1, 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * System class initialiser.
 */
JSObject*
ps_InitSystemClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_DefineObject(cx, obj, ps_System_str, &system_class, NULL, 0);
    if (!proto) {
        return NULL;
    }
    if (!JS_DefineFunctions(cx, proto, system_static_methods)) {
        return NULL;
    }
    return proto;
}
