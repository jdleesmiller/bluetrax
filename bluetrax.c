#include "bluetrax.h"

/**
 * Taken from
 * https://raw.github.com/writefaruq/bluez/devel/tools/hciconfig.c
 *
 * see http://www.bluetooth.org/assigned-numbers/baseband.htm
 */
char *bluetrax_get_minor_device_name(int major, int minor)
{
  switch (major) {
    case 0:	/* misc */
      return "";
    case 1:	/* computer */
      switch(minor) {
        case 0:
          return "Uncategorized";
        case 1:
          return "Desktop workstation";
        case 2:
          return "Server";
        case 3:
          return "Laptop";
        case 4:
          return "Handheld";
        case 5:
          return "Palm";
        case 6:
          return "Wearable";
      }
      break;
    case 2:	/* phone */
      switch(minor) {
        case 0:
          return "Uncategorized";
        case 1:
          return "Cellular";
        case 2:
          return "Cordless";
        case 3:
          return "Smart phone";
        case 4:
          return "Wired modem or voice gateway";
        case 5:
          return "Common ISDN Access";
        case 6:
          return "Sim Card Reader";
      }
      break;
    case 3:	/* lan access */
      if (minor == 0)
        return "Uncategorized";
      switch(minor / 8) {
        case 0:
          return "Fully available";
        case 1:
          return "1-17% utilized";
        case 2:
          return "17-33% utilized";
        case 3:
          return "33-50% utilized";
        case 4:
          return "50-67% utilized";
        case 5:
          return "67-83% utilized";
        case 6:
          return "83-99% utilized";
        case 7:
          return "No service available";
      }
      break;
    case 4:	/* audio/video */
      switch(minor) {
        case 0:
          return "Uncategorized";
        case 1:
          return "Device conforms to the Headset profile";
        case 2:
          return "Hands-free";
          /* 3 is reserved */
        case 4:
          return "Microphone";
        case 5:
          return "Loudspeaker";
        case 6:
          return "Headphones";
        case 7:
          return "Portable Audio";
        case 8:
          return "Car Audio";
        case 9:
          return "Set-top box";
        case 10:
          return "HiFi Audio Device";
        case 11:
          return "VCR";
        case 12:
          return "Video Camera";
        case 13:
          return "Camcorder";
        case 14:
          return "Video Monitor";
        case 15:
          return "Video Display and Loudspeaker";
        case 16:
          return "Video Conferencing";
          /* 17 is reserved */
        case 18:
          return "Gaming/Toy";
      }
      break;
    case 5:	/* peripheral */ {
                                   static char cls_str[48];

                                   cls_str[0] = '\0';

                                   switch(minor & 48) {
                                     case 16:
                                       strncpy(cls_str, "Keyboard", sizeof(cls_str));
                                       break;
                                     case 32:
                                       strncpy(cls_str, "Pointing device", sizeof(cls_str));
                                       break;
                                     case 48:
                                       strncpy(cls_str, "Combo keyboard/pointing device", sizeof(cls_str));
                                       break;
                                   }
                                   if((minor & 15) && (strlen(cls_str) > 0))
                                     strcat(cls_str, "/");

                                   switch(minor & 15) {
                                     case 0:
                                       break;
                                     case 1:
                                       strncat(cls_str, "Joystick", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     case 2:
                                       strncat(cls_str, "Gamepad", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     case 3:
                                       strncat(cls_str, "Remote control", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     case 4:
                                       strncat(cls_str, "Sensing device", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     case 5:
                                       strncat(cls_str, "Digitizer tablet", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     case 6:
                                       strncat(cls_str, "Card reader", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                     default:
                                       strncat(cls_str, "(reserved)", sizeof(cls_str) - strlen(cls_str));
                                       break;
                                   }
                                   if(strlen(cls_str) > 0)
                                     return cls_str;
                                 }
    case 6:	/* imaging */
                                 if (minor & 4)
                                   return "Display";
                                 if (minor & 8)
                                   return "Camera";
                                 if (minor & 16)
                                   return "Scanner";
                                 if (minor & 32)
                                   return "Printer";
                                 break;
    case 7: /* wearable */
                                 switch(minor) {
                                   case 1:
                                     return "Wrist Watch";
                                   case 2:
                                     return "Pager";
                                   case 3:
                                     return "Jacket";
                                   case 4:
                                     return "Helmet";
                                   case 5:
                                     return "Glasses";
                                 }
                                 break;
    case 8: /* toy */
                                 switch(minor) {
                                   case 1:
                                     return "Robot";
                                   case 2:
                                     return "Vehicle";
                                   case 3:
                                     return "Doll / Action Figure";
                                   case 4:
                                     return "Controller";
                                   case 5:
                                     return "Game";
                                 }
                                 break;
    case 63:	/* uncategorised */
                                 return "";
  }
  return "Unknown (reserved) minor device class";
}
