# Ogle - A video player
# Copyright (C) 2000, 2001 Björn Englund, Håkan Hjort, Martin Norbäck
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

pkglib_PROGRAMS = ogle_nav
bin_PROGRAMS = ifo_dump
noinst_PROGRAMS =  vmg_dump dump

AM_CPPFLAGS = @TOP_INCLUDES@ @DVDREAD_CPPFLAGS@ @XML_CFLAGS@
 
AM_CFLAGS = @O_CFLAGS@

CFLAGS = @CFLAGS@ @DVDREAD_ALIGN_CFLAGS@

#vmg_LDADD = $(top_builddir)/common/libcommon.a \
#	$(top_builddir)/ogle/libmsgevents.la
#vmg_SOURCES = vmg.c com.c 


vmg_dump_LDADD = @DVDREAD_LIBS@
vmg_dump_SOURCES = vmg_dump.c \
	vmcmd.c vmcmd.h

dump_LDADD = @DVDREAD_LIBS@
dump_SOURCES = dump.c \
	vmcmd.c vmcmd.h \
	decoder.c decoder.h

ifo_dump_LDADD = @DVDREAD_LIBS@
ifo_dump_SOURCES = ifo_dump.c \
	vmcmd.c

ogle_nav_LDADD = $(top_builddir)/common/libcommon.a \
	$(top_builddir)/ogle/libmsgevents.la \
	@DVDREAD_LIBS@ @XML_LIBS@
ogle_nav_SOURCES = nav.c \
	vm.c vm.h vm_serialize.c vm_serialize.h \
	decoder.c decoder.h \
	vmcmd.c vmcmd.h \
	com.c \
	interpret_config.c interpret_config.h

