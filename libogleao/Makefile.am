
AM_CFLAGS = @O_CFLAGS@

noinst_LTLIBRARIES = libogleao.la
libogleao_la_SOURCES = ogle_ao.h ogle_ao_private.h ogle_ao.c \
	solaris_audio.h solaris_audio.c null_audio.h null_audio.c \
	oss_audio.h oss_audio.c alsa_audio.h alsa_audio.c \
	obsd_audio.h obsd_audio.c
libogleao_la_LIBADD = @LIBOGLEAO_LIBS@
libogleao_la_LDFLAGS =
