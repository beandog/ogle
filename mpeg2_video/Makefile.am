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

if MLIB_LIBS
MLIBSOURCES=yuv2rgb_mlib.c yuv2rgb_mlib.h
else
if USE_ALTIVEC
ALTIVECSOURCES=altivec_mlib.c
else
if USE_MMX
MMXSOURCES=yuv2rgb_mmx.c idct_mmx.c mmx_mlib.c mmx_mlib.h mmx.h
else
CSOURCES=c_mlib.c c_mlib.h
endif
endif
endif

if USE_SPARCASM
SPARCASM=ffb_asm.s ffb_asm.h
endif


EXTRA_DIST = \
	yuv2rgb_mlib.c yuv2rgb_mlib.h \
	yuv2rgb_mmx.c yuv2rgb_mmx.h mmx.h mmx_mlib.h mmx_mlib.c idct_mmx.c \
	altivec_mlib.c \
	c_mlib.c c_mlib.h \
	ffb_asm.s ffb_asm.h

pkglib_PROGRAMS = ogle_mpeg_vs ogle_vout

AM_CPPFLAGS = @TOP_INCLUDES@ @X_CFLAGS@ @MLIB_CPPFLAGS@ @JPEG_CPPFLAGS@ @XML_CFLAGS@
AM_CFLAGS = @O_CFLAGS@

ogle_mpeg_vs_SOURCES = \
	video_stream.c video_stream.h \
	video_mpeg1.c video_mpeg2.c video_motion.c \
	c_getbits.c c_getbits.h \
	video_tables.c video_tables.h \
	video_types.h \
	$(MLIBSOURCES) $(ALTIVECSOURCES) $(MMXSOURCES) $(CSOURCES)
ogle_mpeg_vs_LDADD = \
	@MLIB_LIBS@ \
	$(top_builddir)/common/libcommon.a \
	$(top_builddir)/ogle/libmsgevents.la

ogle_vout_SOURCES = \
	video_output_x11.c video_output.c \
	yuv2rgb.c yuv2rgb.h \
	screenshot.c screenshot.h \
	spu_mixer.c spu_mixer.h \
	wm_state.c wm_state.h \
	display.c display.h \
	video_output.h \
        xscreensaver-comm.c xscreensaver-comm.h \
	video_output_parse_config.c video_output_parse_config.h \
	$(MLIBSOURCES) $(ALTIVECSOURCES) $(SPARCASM) $(MMXSOURCES) $(CSOURCES)
ogle_vout_LDADD = \
	@MLIB_LIBS@ @JPEG_LIBS@ \
	@X_PRE_LIBS@ @X_LIBS@ -lX11 @X_EXTRA_LIBS@ @XEXT_LIB@ \
	@XF86VIDMODE_LIBS@ @XML_LIBS@ \
	@XINERAMA_LIBS@ \
	$(top_builddir)/common/libcommon.a \
	$(top_builddir)/ogle/libmsgevents.la

