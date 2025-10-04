bin_PROGRAMS = prontoscript jscpucfg

prontoscript_SOURCES = \
    js.c \
    jsapi.c \
    jsarena.c \
    jsarray.c \
    jsatom.c \
    jsbool.c \
    jscntxt.c \
    jsdate.c \
    jsdbgapi.c \
    jsdhash.c \
    jsdtoa.c \
    jsemit.c \
    jsexn.c \
    jsfile.c \
    jsfun.c \
    jsgc.c \
    jshash.c \
    jsinterp.c \
    jslock.c \
    jslog2.c \
    jslong.c \
    jsmath.c \
    jsnum.c \
    jsobj.c \
    jsopcode.c \
    jsparse.c \
    jsprf.c \
    jsregexp.c \
    jsscan.c \
    jsscope.c \
    jsscript.c \
    jsstr.c \
    jsutil.c \
    jsxdrapi.c \
    jsxml.c \
    prmjtime.c \
    ext/jsunit.c \
    ext/psselect.c \
    ext/pssystem.c \
    ext/pstcpsocket.c \
    ext/psudpsocket.c

prontoscript_CPPFLAGS = \
    -DXP_UNIX \
    -DHAVE_VA_COPY -DVA_COPY=va_copy \
    -DJS_C_STRINGS_ARE_UTF8 \
    -Wno-shift-negative-value

BUILT_SOURCES = \
    jsautocfg.h

jscpucfg_SOURCES = \
    jscpucfg.c

jsautocfg.h: jscpucfg
	./jscpucfg > $@
