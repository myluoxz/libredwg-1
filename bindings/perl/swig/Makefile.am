## Makefile.am
##
## Copyright (C) 2018 Free Software Foundation, Inc.
##
## This file is part of LibreDWG.
##
## LibreDWG is free software: you can redistribute it and/or modify it
## under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## LibreDWG is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty
## of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
## See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

## bindings/perl/Makefile.am: call SWIG and build perl module
## written by Reini Urban

## TODO: place generated files out of source tree

ACLOCAL_AMFLAGS = -I $(top_srcdir)/m4

## magic to override pkgperldir = $(perldir)/$(PACKAGE)
## at least in Ubuntu perl wouldn't find the module there
#pkgperldir = $(perldir)

so = ../blib/arch/auto/LibreDWG/LibreDWG.bundle
dist_perl_PERL = ../LibreDWG.pm
../blib/arch/auto/LibreDWG/LibreDWG.bundle: ../LibreDWG.pm ../LibreDWG.xs ../Makefile.PL
	cd .. && \
	  $(PERL) Makefile.PL CFLAGS="-I$(top_srcdir)/include" \
                              LDFLAGS="-L$(top_builddir)/src/.libs" && \
	  $(MAKE)

## perl looks dynamically for libredwg_perl.so on 'use LibreDWG;'
#libredwg_perl_la_SOURCES = $(BUILT_SOURCES) $(SWIG_SOURCES)
#libredwg_perl_la_CPPFLAGS = -I$(top_srcdir)/include $(AX_SWIG_PERL_CPPFLAGS)
#libredwg_perl_la_LDFLAGS = -module
#libredwg_perl_la_LIBADD = ../../src/libredwg.la

# regen the wrapper
if HAVE_SWIG_PERL
BUILT_SOURCES = $(abs_top_builddir)/bindings/perl/wrap_perl.c
SWIG_SOURCES = $(top_srcdir)/bindings/dwg.i \
	$(top_srcdir)/include/dwg.h \
	$(top_srcdir)/include/dwg_api.h

$(BUILT_SOURCES): $(SWIG_SOURCES) Makefile.am
	cd .. && swig -perl5 -importall -I$(abs_top_srcdir)/include -o $@ \
	              $(abs_top_srcdir)/bindings/dwg.i
endif
