AM_CFLAGS = $(VARNISHAPI_CFLAGS) -Wall -Werror -Wunused-parameter -Wmissing-prototypes
AM_LDFLAGS = $(VMOD_LDFALGS)

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = $(VARNISHAPI_VMODDIR)
vmod_LTLIBRARIES = libvmod_prehash.la
notrans_man_MANS = vmod_prehash.3

libvmod_prehash_la_SOURCES = vdir.c vdir.h \
	prehash.c random.c roundrobin.c sha256.c override.c mapping.c passthru.c \
	prehash.h

nodist_libvmod_prehash_la_SOURCES = vcc_prehash_if.c vcc_prehash_if.h

@BUILD_VMOD_PREHASH@

EXTRA_DIST = $(top_srcdir)/src/*.vcc $(top_srcdir)/src/_vdir.h $(top_srcdir)/include/*.h

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/*.man.rst 	$(builddir)/*.3

SUFFIXES = .man.rst
