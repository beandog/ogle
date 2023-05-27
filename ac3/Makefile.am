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

pkglib_PROGRAMS = ogle_ac3_p ogle_audio

AM_CPPFLAGS = @TOP_INCLUDES@ @A52_CPPFLAGS@ @MAD_CPPFLAGS@ @XML_CFLAGS@

AM_CFLAGS = @O_CFLAGS@

ogle_ac3_p_SOURCES = ac3dec_wrap.c
ogle_ac3_p_LDADD = $(top_builddir)/common/libcommon.a \
	$(top_builddir)/ogle/libmsgevents.la

#ogle_a52_SOURCES = a52_decoder.c parse_config.h parse_config.c
#ogle_a52_LDADD = $(top_builddir)/common/libcommon.a \
#	$(top_builddir)/ogle/libmsgevents.la \
#	$(top_builddir)/libao/libao.la \
#	@A52_LIBS@ \
#	@XML_LIBS@

ogle_audio_SOURCES = audio_out.c audio_types.h audio_types.c \
	parse_config.h parse_config.c \
	audio_config.h audio_config.c conversion.h conversion.c \
	audio_play.h audio_play.c \
	decode.h decode_private.h decode.c decode.h \
	decode_a52.c decode_a52.h \
	decode_lpcm.c decode_lpcm.h \
	decode_mpeg.c decode_mpeg.h \
	decode_dts.c decode_dts.h

ogle_audio_LDADD = $(top_builddir)/common/libcommon.a \
	$(top_builddir)/ogle/libmsgevents.la \
	$(top_builddir)/libogleao/libogleao.la \
	@A52_LIBS@ \
	@MAD_LIBS@ \
	@XML_LIBS@
