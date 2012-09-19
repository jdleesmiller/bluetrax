#include "bluetrax.h"

#include <getopt.h>
#include <time.h>

/**
* Read stream of binary bluetrax_record_t structs and print them in
* human-readable form, one per line.
*/
static void binary_to_text(FILE *file) {
  bluetrax_record_t record;
  struct tm *timeinfo;
  char time[20] = { 0 };
  char addr[19] = { 0 };

  while (fread(&record, sizeof(record), 1, file) == 1) {
    timeinfo = localtime(&record.time);
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", timeinfo);
    ba2str(&record.bdaddr, addr);
    printf("%s\t%s\n", time, addr);
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

  binary_to_text(file);

  return 0;
}

