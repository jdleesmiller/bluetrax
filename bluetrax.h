#ifndef _BLUETRAX_H_
#define _BLUETRAX_H_

#include <stdlib.h>
#include <unistd.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

/* __attribute__ is gcc-specific */
#ifndef __GNUC__
#  define  __attribute__(x)  /* nothing */
#endif

/**
 * Record that corresponds to an EVT_INQUIRY_COMPLETE message. 
 */
typedef struct {
  struct timeval time;
} __attribute__((packed)) bluetrax_inquiry_complete_t;

/**
 * Record that corresponds to an EVT_INQUIRY_RESULT.
 */
typedef struct {
  struct timeval time;
  bdaddr_t       bdaddr;
  uint8_t        dev_class[3];
} __attribute__((packed)) bluetrax_inquiry_result_t;

/**
 * Record that corresponds to an EVT_INQUIRY_RESULT_WITH_RSSI message. 
 *
 * The valid range for the rssi byte is -127 to +20 dBm [BTSPEC, volume 2,
 * section 7.7.33, page 757]. Note that the specification calls for accuracy of
 * +/- 6dBm [BTSPEC, volume 2, section 4.1.6, page 48].
 */
typedef struct {
  struct timeval time;
  bdaddr_t       bdaddr;
  uint8_t        dev_class[3];
  int8_t         rssi;
} __attribute__((packed)) bluetrax_inquiry_result_with_rssi_t;

/**
 * Record for the basic scan.
 */
typedef struct {
  time_t time;
  bdaddr_t bdaddr;
} __attribute__((packed)) bluetrax_record_t;

/**
 * String for the minor device class.
 *
 * @return pointer to string in static storage
 */
char *bluetrax_get_minor_device_name(int major, int minor);

#endif /* guard */
