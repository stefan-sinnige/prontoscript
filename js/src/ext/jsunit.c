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
#include "jsstr.h"
#include "jsunit.h"
#include "psselect.h"

#include <stdio.h>
#include <string.h>

/*
 * Forward declarations
 */
static JSBool JSUnit_CT(JSContext*, JSObject*, uintN, jsval*, jsval*);
static void   JSUnit_DT(JSContext*, JSObject*);
static JSBool JSUnit_Add(JSContext*, JSObject*j, uintN, jsval*, jsval*);
static JSBool JSUnit_Assert(JSContext*, JSObject*j, uintN, jsval*, jsval*);
static JSBool JSUnit_Events(JSContext*, JSObject*j, uintN, jsval*, jsval*);
static JSBool JSUnit_Run(JSContext*, JSObject*j, uintN, jsval*, jsval*);

/*
 * The JSUnit class private instance data.
 */

typedef enum 
{
    NOT_RUN,
    PASS,
    FAIL
} result_t;

typedef struct _JSUnitTestCase {
    JSString* name;                 /* The test case name. */
    jsval func;                     /* The function to run the test case. */
    result_t result;                /* The test result. */
    struct _JSUnitTestCase *next;   /* The next test case. */
} JSUnitTestCase;

typedef struct {
    JSString* name;         /* The suite name. */
    size_t passed;          /* Number of passed test cases. */
    size_t failed;          /* Number of failed test cases. */
    JSUnitTestCase *test;   /* The test-case currently being executed */
    JSUnitTestCase *cases;  /* The linked list of test cases. */
} JSUnitSuite;

/*
 * Definition of class properties
 */

static JSPropertySpec jsunit_props[] = {
    /* { name, tinyid, flags, getter, setter } */
    {0, 0, 0, 0, 0}
};

/*
 * Definition of class methods
 */
static JSFunctionSpec jsunit_methods[] = {
    /* { name, call, nargs, flags, extra } */
    {"add", JSUnit_Add, 2, 0, 0},
    {"assert", JSUnit_Assert, 2, 0, 0},
    {"events", JSUnit_Events, 0, 0, 0},
    {"run", JSUnit_Run, 2, 0, 0},
    {0, 0, 0, 0, 0}
};

/**
 * Definition of the class
 */
static JSClass jsunit_class = {
    js_JSUnit_str,                  /* name */
    0,                              /* flags */
    JS_PropertyStub,                /* add property */
    JS_PropertyStub,                /* del property */
    JS_PropertyStub,                /* get property */
    JS_PropertyStub,                /* set property */
    JS_EnumerateStub,               /* enumerate */
    JS_ResolveStub,                 /* resolve */
    JS_ConvertStub,                 /* convert */
    JSUnit_DT,                      /* finalize */
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/*
 * Create and destroy a JSUnit instance.
 */

static JSUnitSuite*
JSUnit_New(JSContext *cx, JSString *name)
{
    JSUnitSuite *suite = NULL;
    suite = (JSUnitSuite*) JS_malloc(cx, sizeof(JSUnitSuite));
    if (!suite) {
        return NULL;
    }
    suite->name = name;
    suite->passed = 0;
    suite->failed = 0;
    suite->test = NULL;
    suite->cases = NULL;
    return suite;
}

static void
JSUnit_Delete(JSContext *cx, JSUnitSuite *suite)
{
    JSUnitTestCase *test_case = suite->cases;
    while (test_case != NULL) {
        JSUnitTestCase *next = test_case->next;
        JS_free(cx, test_case);
        test_case = next;
    }
    JS_free(cx, suite);
}

/**
 * Synopsis:
 *      JSUnit(name)
 * Purpose:
 *      Create a new JSUnit instance.
 * Parameters:
 *      name String (optional)
 *           The name of the test-suite
 * Returns:
 *      A new JSUnit instance.
 */
static JSBool 
JSUnit_CT(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok = JS_TRUE;
    JSString* name = NULL;
    JSUnitSuite* suite = NULL;

    /* Get the suite name */
    if (argc == 1) {
        name = JSVAL_TO_STRING(argv[0]);
    }

    /* Create the object */
    if (!obj) {
        obj = js_NewObject(cx, &jsunit_class,NULL, NULL);
        if (!obj) {
            return JS_FALSE;
        }
    }

    /* Set the private instance state object */
    suite = JSUnit_New(cx, name);
    JS_LOCK_OBJ(cx, obj);
    ok = JS_SetPrivate(cx, obj, suite);
    JS_UNLOCK_OBJ(cx, obj);
    if (!ok) {
        return JS_FALSE;
    }
    return JS_TRUE;
}

/**
 * Destructor
 */
static void
JSUnit_DT(JSContext* cx, JSObject *obj)
{
    JSUnitSuite* suite = NULL;
    suite = (JSUnitSuite*)JS_GetInstancePrivate(cx, obj, &jsunit_class, NULL);
    if (suite) {
        JSUnit_Delete(cx, suite);
    }
};

/**
 * Synopsis:
 *      add(name, function)
 * Purpose:
 *      Add a test-case function and an associated name.
 * Parameters:
 *      name String (optional)
 *           The name of the test-case
 *      function Object
 *           The test-case function to run.
 */
static JSBool
JSUnit_Add(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSUnitSuite *suite;
    JSUnitTestCase *test_case = NULL;
    JSString *name;

    /* Get the instance data */
    suite = (JSUnitSuite*) JS_GetPrivate(cx, obj);
    if (!suite) {
        return JS_FALSE;
    }

    /* Expect at least one argument */
    if (argc < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_NOT_ENOUGH_ARGUMENTS);
        return JS_FALSE;
    }

    /* Get the test-case name and function */
    if (argc == 2) {
        name = JSVAL_TO_STRING(argv[0]);
        --argc, ++argv;
    }
    if (!JSVAL_IS_FUNCTION(cx, argv[0])) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_FUNCTION);
        return JS_FALSE;
    }
    
    /*
     * Create the test-case.
     */
    test_case = (JSUnitTestCase*) JS_malloc(cx, sizeof(JSUnitTestCase));
    if (!test_case) {
        return JS_FALSE;
    }
    test_case->name = name;
    test_case->func = argv[0];
    test_case->result = NOT_RUN;
    test_case->next = NULL;

    /*
     * Append the test case to the suite.
     */
    JS_LOCK_OBJ(cx, obj);
    if (suite->cases == NULL) {
        suite->cases = test_case;
    }
    else {
        JSUnitTestCase *linked_case = suite->cases;
        while (linked_case->next != NULL) {
            linked_case = linked_case->next;
        }
        linked_case->next = test_case;
    }
    JS_UNLOCK_OBJ(cx, obj);
    return JS_TRUE;
}

/**
 * Synopsis:
 *      assert(expected, actual)
 * Purpose:
 *      Perform an assertion test to verify that the expected value matches the
 *      actual value.
 * Parameters:
 *      expected Object
 *           The expected value
 *      actual Object
 *           The actual value
 */
static JSBool
JSUnit_Assert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
               jsval *rval)
{
    JSBool result = JS_TRUE;
    JSUnitSuite* suite;

    /* Get the instance data */
    suite = (JSUnitSuite*) JS_GetPrivate(cx, obj);
    if (!suite) {
        return JS_FALSE;
    }

    /* Expect two argument */
    if (argc < 2) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_NOT_ENOUGH_ARGUMENTS);
        return JS_FALSE;
    }

    /* Match the actual with the expected type (convert if necessary) and 
     * compare them. */
    switch (JS_TypeOfValue(cx, argv[0])) {
        case JSTYPE_NUMBER:
            {
                if (JSVAL_IS_INT(argv[0]) && JSVAL_IS_INT(argv[1])) {
                    int32 expected, actual;
                    expected = JSVAL_TO_INT(argv[0]);
                    actual = JSVAL_TO_INT(argv[1]);
                    if (expected != actual) {
                        result = JS_FALSE;
                        fprintf(stderr, "Assertion failed:\n"
                                        "  Expected: %d\n"
                                        "  Actual  : %d\n",
                                expected, actual);
                    }
                }
                else
                if (JSVAL_IS_DOUBLE(argv[0]) && JSVAL_IS_DOUBLE(argv[1])) {
                    jsdouble expected, actual;
                    expected = *JSVAL_TO_DOUBLE(argv[0]);
                    actual = *JSVAL_TO_DOUBLE(argv[1]);
                    if (expected != actual) {
                        result = JS_FALSE;
                        fprintf(stderr, "Assertion failed:\n"
                                        "  Expected: %f\n"
                                        "  Actual  : %f\n",
                                expected, actual);
                    }
                }
                else {
                    result = JS_FALSE;
                    fprintf(stderr, "Assertion failed: type mismatch\n");
                }
            }
            break;
        case JSTYPE_STRING:
            {
                if (JSVAL_IS_STRING(argv[0]) && JSVAL_IS_STRING(argv[1])) {
                    char *expected, *actual;
                    expected = js_GetStringBytes(JSVAL_TO_STRING(argv[0]));
                    actual = js_GetStringBytes(JSVAL_TO_STRING(argv[0]));
                    if (strcmp(expected, actual) != 0) {
                        result = JS_FALSE;
                        fprintf(stderr, "Assertion failed:\n"
                                        "  Expected: \"%s\"\n"
                                        "  Actual  : \"%s\"\n",
                                expected, actual);
                    }
                }
                else
                if (JSVAL_IS_STRING(argv[0]) && JSVAL_IS_VOID(argv[1])) {
                    char *expected, *actual;
                    expected = js_GetStringBytes(JSVAL_TO_STRING(argv[0]));
                    if (strlen(expected) > 0) {
                        result = JS_FALSE;
                        fprintf(stderr, "Assertion failed:\n"
                                        "  Expected: \"%s\"\n"
                                        "  Actual  : void\n",
                                expected);
                    }
                }
                else {
                    result = JS_FALSE;
                    fprintf(stderr, "Assertion failed: type mismatch\n");
                }
            }
            break;
        case JSTYPE_BOOLEAN:
            {
                if (JSVAL_IS_BOOLEAN(argv[0]) && JSVAL_IS_BOOLEAN(argv[1])) {
                    JSBool expected, actual;
                    expected = JSVAL_TO_BOOLEAN(argv[0]);
                    actual = JSVAL_TO_BOOLEAN(argv[1]);
                    if (expected != actual) {
                        result = JS_FALSE;
                        fprintf(stderr, "Assertion failed:\n"
                                        "  Expected: %s\n"
                                        "  Actual  : %s\n",
                                expected == JS_TRUE ? "true" : "false",
                                actual == JS_TRUE ? "true" : "false");
                    }
                }
                else {
                    result = JS_FALSE;
                    fprintf(stderr, "Assertion failed: type mismatch\n");
                }
            }
            break;
        default:
            fprintf(stderr, "Unsupported assertion types\n");
            result = JS_FALSE;
            break;
            
    }

    /* If the assertion failed, record it in the running test case */
    if (!result) {
        suite->test->result = FAIL;
    }

    return JS_TRUE;
}

/**
 * Synopsis:
 *      events()
 * Purpose:
 *      Run through all outstanding events until there are none left. This will
 *      invoke functions asynchroneoulsy when events are triggered.
 * Parameters:
 */
static JSBool
JSUnit_Events(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
              jsval *rval)
{
    while (ps_HandleSelect(cx));
    return JS_TRUE;
}

/**
 * Run a test-case function
 */
static JSBool
JSUnit_RunTestCase(JSContext *cx, JSObject *obj, jsval func, uintN argc,
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
    *sp++ = func;
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
 *      run()
 * Purpose:
 *      Run all the test cases and report a summary result. If one of the test
 *      cases failed, report this as an error which would set the exit code to
 *      be non-zero.
 * Parameters:
 */
static JSBool
JSUnit_Run(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSUnitSuite* suite;
    JSUnitTestCase* test_case;
    size_t total = 0;
    size_t pass = 0;
    size_t fail = 0;

    /* Get the instance data */
    suite = (JSUnitSuite*) JS_GetPrivate(cx, obj);
    if (!suite) {
        return JS_FALSE;
    }

    /* Run all the test cases. */
    test_case = suite->cases;
    while (test_case != NULL) {
        char* name = "";

        /* Run the test case function. Any assertion that failed will be seen as
         * a failure of running the test-case. */
        int argc = 0;
        jsval argv[0];
        suite->test = test_case;
        test_case->result = PASS;
        if (!JSUnit_RunTestCase(cx, obj, test_case->func, argc, argv)) {
            test_case->result = FAIL;
        }

        /* Print the result */
        name = js_GetStringBytes(test_case->name);
        if (test_case->result == PASS) {
            fprintf(stdout, "PASS: %s\n", name);
        }
        else {
            fprintf(stdout, "FAIL: %s\n", name);
        }

        /* Next test-case */
        test_case = test_case->next;
    }

    /* Accumulate the result and display a summary. */
    total = 0;
    test_case = suite->cases;
    while (test_case != NULL) {
        ++total;
        if (test_case->result == PASS) ++pass;
        if (test_case->result == FAIL) ++fail;
        test_case = test_case->next;
    }
    fprintf(stdout, "Total: %zu  Pass: %zu  Fail: %zu\n", total, pass, fail);

    /* Check if any failed */
    if (fail > 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_FAILING_TEST_SUITE);
        return JS_FALSE;
    }

    /* Check if all test-cases have run */
    if (total != pass + fail) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             PSMSG_NOT_ALL_TEST_CASES_RUN);
        return JS_FALSE;
    }

    return JS_TRUE;
}

/**
 * JSUnit class initialiser.
 */
JSObject*
js_InitJSUnitClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    proto = JS_InitClass(cx, obj, NULL, &jsunit_class, JSUnit_CT, 0,
                         jsunit_props, jsunit_methods, NULL, NULL);
    if (!proto) {
        return NULL;
    }
    return proto;
}
