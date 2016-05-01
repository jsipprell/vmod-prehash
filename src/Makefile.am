AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror -Wunused-parameter -Wmissing-prototypes

DEFAULT_INCLUDES = -I. -I$(top_srcdir) -I$(top_srcdir)/include

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_prehash.la
notrans_man_MANS = vmod_prehash.3

libvmod_prehash_la_LDFLAGS = $(AM_LDFLAGS) -module -export-dynamic -avoid-version -shared
libvmod_prehash_la_SOURCES = vcc_prehash_if.c vcc_prehash_if.h vdir.c vdir.h \
	prehash.c random.c roundrobin.c sha256.c \
	prehash.h

vcc_prehash_if.c vcc_prehash_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_prehash.vcc
	@VMODTOOL@ -w ../docs/ -o vcc_prehash_if $(top_srcdir)/src/vmod_prehash.vcc

vmod_prehash.man.rst: vcc_prehash_if.c vcc_prehash_if.h
	cp -f $(top_srcdir)/docs/$@ $@

%.3: %.man.rst
if HAVE_RST2MAN
	${RST2MAN} -r error $< $@
else
	@echo "========================================"
	@echo "You need rst2man installed to make dist"
	@echo "========================================"
	@false
endif

EXTRA_DIST = $(top_srcdir)/src/*.vcc $(top_srcdir)/src/_vdir.h $(top_srcdir)/include/*.h

CLEANFILES = $(builddir)/vcc*if.c $(builddir)/vcc*if.h \
	$(builddir)/../docs/vmod_*.man.rst \
	$(builddir)/../docs/vmod_*.rst \
	$(builddir)/*.man.rst 	$(builddir)/*.3

SUFFIXES = .man.rst
