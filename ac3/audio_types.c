#include "audio_types.h"

char *channeltype_str(ChannelType_t chtype)
{
  char *str;
  switch(chtype) {
  case ChannelType_Unspecified:
    str = "Unspecified";
    break;
  case ChannelType_Null:
    str = "Null";
    break;
  case ChannelType_Left:
    str = "Left";
    break;
  case ChannelType_Right:
    str = "Right";
    break;
  case ChannelType_Center:
    str = "Center";
    break;
  case ChannelType_LeftSurround:
    str = "LeftSurround";
    break;
  case ChannelType_RightSurround:
    str = "RightSurround";
    break;
  case ChannelType_Surround:
    str = "Surround";
    break;
  case ChannelType_LFE:
    str = "LFE";
    break;
  case ChannelType_CenterSurround:
    str = "CenterSurround";
    break;
  case ChannelType_Mono:
    str = "Mono";
    break;
  case ChannelType_AC3:
    str = "AC3";
    break;
  case ChannelType_DTS:
    str = "DTS";
    break;
  case ChannelType_MPEG:
    str = "MPEG";
    break;
  case ChannelType_LPCM:
    str = "LPCM";
    break;
  default:
    str = "(unknown)";
    break;
  }
  return str;
}  
