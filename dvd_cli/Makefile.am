# Ogle - A video player
# Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

pkglib_PROGRAMS = ogle_cli

AM_CPPFLAGS = @TOP_INCLUDES@ @X_CFLAGS@ @XML_CFLAGS@
AM_CFLAGS = @O_CFLAGS@

ogle_cli_SOURCES = main.c \
	xsniffer.c xsniffer.h \
	bindings.c bindings.h \
	interpret_config.c interpret_config.h

ogle_cli_LDADD = @X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ \
	@XML_LIBS@ \
	$(top_builddir)/ogle/libdvdcontrol.la
