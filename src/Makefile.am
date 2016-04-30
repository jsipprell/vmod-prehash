AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror -Wunused-parameter -Wmissing-prototypes

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_backmap.la


libvmod_backmap_la_LDFLAGS = $(AM_LDFLAGS) -module -export-dynamic -avoid-version -shared
libvmod_backmap_la_SOURCES = vcc_backmap_if.c vcc_backmap_if.h
vcc_backmap_if.c vcc_backmap_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_backmap.vcc
	@VMODTOOL@ -w ../docs/ -o vcc_backmap_if $(top_srcdir)/src/vmod_backup.vcc

EXTRA_DIST = $(top_srcdir)/src/*.vcc

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/../docs/vmod_*.man.rst \
	$(builddir)/../docs/vmod_*.rst
