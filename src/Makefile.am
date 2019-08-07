AM_CFLAGS = $(VARNISHAPI_CFLAGS) -Wall -Werror -Wunused-parameter -Wmissing-prototypes
AM_LDFLAGS = $(VMOD_LDFLAGS) @LDFLAGS_VERSION@

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = $(VARNISHAPI_VMODDIR)
vmod_LTLIBRARIES = libvmod_prehash.la
notrans_man_MANS = vmod_prehash.3

libvmod_prehash_la_SOURCES = vdir.c vdir.h \
	prehash.c random.c roundrobin.c sha256.c override.c mapping.c passthru.c \
	prehash.h

nodist_libvmod_prehash_la_SOURCES = vcc_prehash_if.c vcc_prehash_if.h
$(libvmod_prehash_la_OBJECTS): vcc_prehash_if.h
$(vmod_LTLIBRARIES): symbols.map

@BUILD_VMOD_PREHASH@

EXTRA_DIST = $(top_srcdir)/src/*.vcc $(top_srcdir)/include/*.h $(top_srcdir)/src/symbols.map

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/*.man.rst 	$(builddir)/*.3

SUFFIXES = .man.rst
