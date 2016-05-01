AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror -Wunused-parameter -Wmissing-prototypes

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_prehash.la


libvmod_prehash_la_LDFLAGS = $(AM_LDFLAGS) -module -export-dynamic -avoid-version -shared
libvmod_prehash_la_SOURCES = vcc_prehash_if.c vcc_prehash_if.h vdir.c vdir.h \
	prehash.c random.c roundrobin.c \
	prehash.h
vcc_prehash_if.c vcc_prehash_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_prehash.vcc
	@VMODTOOL@ -w ../docs/ -o vcc_prehash_if $(top_srcdir)/src/vmod_prehash.vcc

EXTRA_DIST = $(top_srcdir)/src/*.vcc $(top_srcdir)/src/_vdir.h $(top_srcdir)/include/*.h

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/../docs/vmod_*.man.rst \
	$(builddir)/../docs/vmod_*.rst
