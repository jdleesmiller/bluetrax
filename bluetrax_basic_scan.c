#include "bluetrax.h"

#include <getopt.h>
#include <stdio.h>
#include <time.h>

#include <bluetooth/hci_lib.h>

#define INQUIRY_MAX_RESPONSES 255

#define INQUIRY_FLAGS IREQ_CACHE_FLUSH

static void run_scan(int dev_id, int scan_length, FILE *out_file) {
  inquiry_info *info = NULL;
  bluetrax_record_t record[INQUIRY_MAX_RESPONSES];
  time_t now;
  size_t i, num_responses;

  /* run the scan */
  num_responses = hci_inquiry(dev_id, scan_length, INQUIRY_MAX_RESPONSES, 
    NULL, &info, INQUIRY_FLAGS);
  if(num_responses < 0) {
    perror("hci_inquiry failed");
    goto free;
  }

  /* get the time once for all responses */
  now = time(NULL);

  /* copy times and addresses to the record array for writing */
  for (i = 0; i < num_responses; ++i) {
    record[i].time = now;
    record[i].bdaddr = info[i].bdaddr;
  }

  /* write discovered devices in binary mode */
  if (num_responses != fwrite(record, sizeof(bluetrax_record_t), 
    num_responses, out_file)) { 
    perror("record write failed");
    goto free;
  }

  /* try to flush to disk after each scan */
  fflush(out_file);

free:
  if (info) {
    bt_free(info);
  }
}

static void print_usage(char **argv) {
  fprintf(stderr, "Usage: %s [--length=n] [--file=file] [--help]\n\n"
    "--length n: length of each scan is approx 1.28*n seconds; default 8\n"
    "--file file: name of file to write to; if omitted, writes to stdout\n"
    "--help: displays this message\n", argv[0]);
}

int main(int argc, char **argv)
{
  int scan_length = 8;
  FILE * out_file = stdout;
  int dev_id;
  int opt;

  static struct option options[] =
  {
    {"help",   no_argument,       0, 'h'},
    {"file",   required_argument, 0, 'f'},
    {"length", required_argument, 0, 'l'},
    {0, 0, 0, 0}
  };

  while ((opt=getopt_long(argc, argv, "+f:l:h", options, NULL)) != -1) {
    switch (opt) {
    case 'f':
      out_file = fopen(optarg, "w");
      if (out_file == NULL) { 
        perror("failed to open output file");
        exit(1);
      }
      break;
    case 'l':
      scan_length = atoi(optarg);
      if (scan_length < 1 || scan_length > 100) { 
        fprintf(stderr, "bad scan length: %s\n", optarg);
        exit(1);
      }
      break;
    case 'h':
    default:
      print_usage(argv);
      exit(0);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 0) {
    print_usage(argv);
    exit(1);
  }

  /* look up the bluetooth device to scan with */
  dev_id = hci_get_route(NULL);
  if (dev_id < 0) {
    perror("Device is not available");
    exit(1);
  }

  /* scan forever */
  for (;;) {
    run_scan(dev_id, scan_length, out_file);
  }

  /* TODO if too many scans fail, could try to restart bluetooth */
  /* IDEA: we could spin off a thread to poke into the buffer at higher
   * frequency and look for changes to num_rsp -- maybe it updates it in place?
   * (but maybe not)
   */

  return 0;
}

