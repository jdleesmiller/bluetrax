#include "bluetrax.h"

#include <getopt.h>
#include <time.h>
#include <syslog.h>

/**
 * Write fields for the device class bytes. We're not very interested in the
 * services byte, so they just get printed as a number.
 *
 * Based on cmd_class from:
 * http://lxr.post-tech.com/source/external/bluetooth/bluez/tools/hciconfig.c
 */
static void write_dev_class(uint8_t dev_class[3]) {
  static const char *major_devices[] = { "Miscellaneous",
                                         "Computer",
                                         "Phone",
                                         "LAN Access",
                                         "Audio/Video",
                                         "Peripheral",
                                         "Imaging",
                                         "Uncategorized" };
  uint8_t service = dev_class[2];
  uint8_t major = dev_class[1];
  uint8_t minor = dev_class[0];

  printf("%hhd,", service);

  if ((major & 0x1f) >= sizeof(major_devices) / sizeof(*major_devices)) {
    fputs(",,", stdout);
  } else {
    printf("%s,%s,", major_devices[major & 0x1f], 
        bluetrax_get_minor_device_name(major, minor));
  }
}

/**
 * Write a bluetooth device address (a MAC address) as a string.
 */
static void write_bdaddr(bdaddr_t bdaddr) {
  char addr[18] = { 0 };

  ba2str(&bdaddr, addr);
  printf("%s,", addr);
}

/**
 * Write a timeval structure as a string; timeval provides microsecond-level
 * precision for times.
 *
 * Based on
 * http://stackoverflow.com/questions/1551597
 */
static void write_timeval(struct timeval *tv) {
  char fmt[64];
  struct tm *tm;

  if((tm = localtime(&tv->tv_sec)) == NULL) {
    syslog(LOG_ERR, "write_timeval: localtime: %m");
    exit(EXIT_FAILURE);
  }
    
  strftime(fmt, sizeof fmt, "%Y-%m-%d %H:%M:%S.%%06u,", tm);
  printf(fmt, tv->tv_usec);
}

/**
* Read binary stream from the bluetrax_scan program and print them in
* human-readable form, one per line.
*/
static void binary_to_text(FILE *file) {
  int tag;
  bluetrax_inquiry_complete_t inquiry_complete;
  bluetrax_inquiry_result_t inquiry_result;
  bluetrax_inquiry_result_with_rssi_t inquiry_result_with_rssi;

  puts("type,time,bdaddr,services,major,minor,rssi");

  /* read in the tag; this tells us how much more to read */
  while (EOF != (tag = fgetc(file))) {
    switch(tag) {
      case EVT_INQUIRY_COMPLETE:
        if (1 != fread(&inquiry_complete, sizeof inquiry_complete, 1, file)) {
          syslog(LOG_ERR, "fread inquiry_complete: %m");
          exit(EXIT_FAILURE);
        }
        
        fputs("complete,", stdout);
        write_timeval(&inquiry_complete.time);
        fputs(",,,,\n", stdout);

        break;
      case EVT_INQUIRY_RESULT:
        if (1 != fread(&inquiry_result, sizeof inquiry_result, 1, file)) {
          syslog(LOG_ERR, "fread inquiry_result: %m");
          exit(EXIT_FAILURE);
        }
        
        fputs("inquiry,", stdout);
        write_timeval(&inquiry_result.time);
        write_bdaddr(inquiry_result.bdaddr);
        write_dev_class(inquiry_result.dev_class);
        puts(",");
        break;
      case EVT_INQUIRY_RESULT_WITH_RSSI:
        if (1 != fread(&inquiry_result_with_rssi,
              sizeof inquiry_result_with_rssi, 1, file))
        {
          syslog(LOG_ERR, "fread inquiry_result_with_rssi: %m");
          exit(EXIT_FAILURE);
        }
        
        fputs("inquiry,", stdout);
        write_timeval(&inquiry_result_with_rssi.time);
        write_bdaddr(inquiry_result_with_rssi.bdaddr);
        write_dev_class(inquiry_result_with_rssi.dev_class);
        printf("%hhd\n", inquiry_result_with_rssi.rssi);
        break;
      default:
        syslog(LOG_ERR, "unsupported tag: %d", tag);
        exit(EXIT_FAILURE);
    }

    fflush(stdout);
  }
}

static void print_usage(char **argv) {
  fprintf(stderr, "Usage: %s [--file=file] [--help]\n\n"
    "--file file: name of file to read; if omitted, reads stdin\n"
    "--help: displays this message\n", argv[0]);
}

int main(int argc, char **argv) { 
  FILE *file = stdin;
  int opt;

  static struct option options[] =
  {
    {"help", no_argument,       0, 'h'},
    {"file", required_argument, 0, 'f'},
    {0, 0, 0, 0}
  };

  while ((opt=getopt_long(argc, argv, "+f:h", options, NULL)) != -1) {
    switch (opt) {
    case 'f':
      file = fopen(optarg, "r");
      if (file == NULL) { 
        perror("failed to open input file");
        exit(1);
      }
      break;
    case 'h':
    default:
      print_usage(argv);
      exit(1);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 0) {
    print_usage(argv);
    exit(1);
  }

  openlog(NULL, LOG_PID | LOG_PERROR | LOG_CONS, LOG_USER);

  binary_to_text(file);

  return 0;
}


