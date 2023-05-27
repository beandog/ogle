#ifndef AUDIO_TYPES_H
#define AUDIO_TYPES_H

/* Ogle - A video player
 * Copyright (C) 2002 Björn Englund
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

/* if you change this don't forget to change channeltype_str() */

typedef enum {
  ChannelType_Unspecified = -1,
  ChannelType_Null = 0,
  ChannelType_Left = 0x1,
  ChannelType_Right = 0x2,
  ChannelType_Center = 0x4,
  ChannelType_LeftSurround = 0x8,
  ChannelType_RightSurround = 0x10,
  ChannelType_Surround = 0x20,
  ChannelType_LFE = 0x40,
  ChannelType_CenterSurround = 0x80,
  ChannelType_Mono = 0x100,
  ChannelType_AC3 =  0x10000,
  ChannelType_DTS =  0x20000,
  ChannelType_MPEG = 0x40000,
  ChannelType_LPCM = 0x80000
} ChannelType_t;

typedef enum {
  ChannelTypeMask_Channels = 0x1ff, 
  ChannelTypeMask_Streams = 0xf0000
} ChannelTypeMask_t;

char *channeltype_str(ChannelType_t chtype);

#endif /* AUDIO_TYPES_H */
