#ifndef ENDIAN_H_INCLUDED
#define ENDIAN_H_INCLUDED

/* Ogle - A video player
 * Copyright (C) 2001 Martin Norbäck
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* For now, just 16,32 bit byteswap */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef WORDS_BIGENDIAN
#  define FROM_BE_16(x) (x)
#  define FROM_BE_32(x) (x)
#else

#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#if defined(HAVE_BYTESWAP_H)
#  include <byteswap.h>
#  define FROM_BE_16(x) (bswap_16(x))
#  define FROM_BE_32(x) (bswap_32(x))
#elif defined(HAVE_SYS_BSWAP_H)
#  include <sys/bswap.h>
#  define FROM_BE_16(x) (bswap16(x))
#  define FROM_BE_32(x) (bswap32(x))
#elif defined(HAVE_SYS_ENDIAN_H) && !defined(__FreeBSD__)
#  include <sys/endian.h>
#  define FROM_BE_16(x) (swap16(x))
#  define FROM_BE_32(x) (swap32(x))
#elif defined(HAVE_SYS_ENDIAN_H) && defined(__FreeBSD__) && __FreeBSD_version >= 470000
#  include <sys/endian.h>
#  define FROM_BE_16(x) (be16toh(x))
#  define FROM_BE_32(x) (be32toh(x))
#else
#  warning "No accelerated byte swap found. Using slow c version."
#  include <inttypes.h>
static inline uint32_t FROM_BE_32(uint32_t x)
{
  return (((x & 0xff000000) >> 24) |
          ((x & 0x00ff0000) >>  8) |
          ((x & 0x0000ff00) <<  8) |
          ((x & 0x000000ff) << 24));
}

static inline uint16_t FROM_BE_16(uint16_t x)
{
  return (((x & 0xff00) >> 8) |
          ((x & 0x00ff) <<  8));
}

#endif /* INCLUDE TESTS */

#endif /* WORDS_BIGENDIAN */

#endif /* ENDIAN_H_INCLUDED */
