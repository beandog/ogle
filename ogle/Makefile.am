# Ogle - A video player
# Copyright (C) 2000, 2001 Bj�rn Englund, H�kan Hjort
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

pkglib_LTLIBRARIES = libmsgevents.la libdvdcontrol.la
pkginclude_HEADERS = dvd.h dvdcontrol.h msgevents.h dvdevents.h dvdbookmarks.h


AM_CPPFLAGS = @TOP_INCLUDES@ @XML_CFLAGS@
AM_CFLAGS = @O_CFLAGS@

libmsgevents_la_SOURCES = msgevents.c
libmsgevents_la_LDFLAGS = -version-info 8:0:2

libdvdcontrol_la_SOURCES = dvdcontrol.c dvdbookmarks.c
libdvdcontrol_la_LIBADD = libmsgevents.la @XML_LIBS@
libdvdcontrol_la_LDFLAGS = -version-info 11:0:2
