/*
 * A periodic Bluetooth scanner.
 *
 * Notes:
 * - the scanner records 'Inquiry Response' messages in binary format; use
 *   bluetrax_scan_unpack to get the results in text (CSV) format
 * - in order to be compliant with the Bluetooth specification [BTSPEC], there
 *   is a Uniform(1.28s, 2.56s) delay between inquiry periods
 * - the scanner records 'Inquiry Complete' messages as well as Inquiry Response
 *   messages; these mark the end of each inquiry; note that you can get the
 *   start of the inquiry by subtracting the inquiry length, which is 1.28s
 *   times the --length argument passed to the scanner (default 8)
 * - the first 'complete' record is a dummy that marks the start of the scan,
 *   according to gettimeofday; all other timings come from the HCI socket
 *
 * References:
 *   [BTSPEC] Bluetooth Specification Version 4.0 (Core_V4.0.pdf)
 *
 * Based on:
 * http://www.wensley.org.uk/c/inq/inq.c
 * http://svn.assembla.com/svn/linuxmce/trunk/0710/VIPShared/PhoneDetection_Bluetooth_Linux.cpp
 * 
 * NB run hciconfig hci0 inqmode 1 to get RSSI data*/
#include "bluetrax.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>

#include <bluetooth/hci_lib.h>

/**
 * Assume something has gone wrong if the select blocks for longer than this, in
 * seconds.
 */
#define SELECT_TIMEOUT 5*60

/**
 * Global flag to stop the loop in run_scan when we get a signal.
 */
static int request_stop_scan = 0xdeadbeef;

static void handle_signal(int signo) {
  if (request_stop_scan != 1) {
    /* first signal: try to stop normally */
    syslog(LOG_NOTICE, "stopping due to signal %d", signo);
    request_stop_scan = 1;
  } else {
    /* second signal: something went wrong; exit now */
    syslog(LOG_ERR, "multiple stop requests; exiting after signal %d", signo);
    exit(EXIT_FAILURE);
  }
}

/**
 * Send HCI command to exit periodic inquiry mode.
 */
static void stop_scan(int dev_sd) {
  if (hci_send_cmd(dev_sd,
        OGF_LINK_CTL, OCF_EXIT_PERIODIC_INQUIRY, 0, NULL) < 0) {
    syslog(LOG_ERR, "failed to exit periodic inquiry state: %m");
  }
}

/**
 * Set up signal handling. Stop scan on SIGINT or SIGTERM.
 *
 * We have to block these signals until we get into pselect (see the pselect man
 * page for why).
 *
 * @return true if no errors
 */
static int setup_signals() {
  struct sigaction sa;
  sigset_t blockset;

  sa.sa_handler = handle_signal;
  sa.sa_flags = 0;

  return
    0 == sigemptyset(&blockset) &&
    0 == sigaddset(&blockset, SIGINT) &&
    0 == sigaddset(&blockset, SIGTERM) &&
    0 == sigprocmask(SIG_BLOCK, &blockset, NULL) &&
    0 == sigemptyset(&sa.sa_mask) &&
    0 == sigaction(SIGINT, &sa, NULL) &&
    0 == sigaction(SIGTERM, &sa, NULL);
}

/**
 * Record an EVT_INQUIRY_RESULT message.
 *
 * Writes a byte with value EVT_INQUIRY_RESULT and then a
 * bluetrax_inquiry_result_t structure (in binary format) to out_file.
 *
 * Reference: [BTSPEC, volume 2, section 7.7.2, page 716]
 *
 * @param out_file to write record to
 *
 * @param time that the message was received
 *
 * @param hdr the event header
 *
 * @param data all data after the header; length is at least hdr->plen 
 *
 * @return EXIT_SUCCESS if no errors
 */
static int handle_inquiry_result(FILE *out_file,
    struct timeval time, hci_event_hdr *hdr, unsigned char *data)
{
  int num_rsp, i;
  inquiry_info *info;
  bluetrax_inquiry_result_t record;

  if (hdr->plen <= 0) {
    syslog(LOG_ERR, "handle_inquiry_result: bad plen: plen=%hhd", hdr->plen);
    return EXIT_FAILURE;
  }

  /* note: we never seem to get num_rsp > 1 here, but handle it anyway */
  num_rsp = data[0];
  syslog(LOG_DEBUG, "handle_inquiry_result: num_rsp=%d", num_rsp);

  /* sanity check */
  if (hdr->plen != num_rsp * sizeof(inquiry_info) + 1) {
    syslog(LOG_ERR, "handle_inquiry_result: bad plen: num_rsp=%d, plen=%hhd",
        num_rsp, hdr->plen);
    return EXIT_FAILURE;
  }

  record.time = time;
  info = (inquiry_info *)(data + 1);
  for (i = 0; i < num_rsp; ++i) {
    bacpy(&record.bdaddr, &info[i].bdaddr);
    memcpy(&record.dev_class, info[i].dev_class, sizeof(record.dev_class));

    /* write the event type and then the event record */
    if (EVT_INQUIRY_RESULT != fputc(EVT_INQUIRY_RESULT, out_file)) {
      syslog(LOG_ERR, "handle_inquiry_result: fputc: %m");
      return EXIT_FAILURE;
    }
    if (1 != fwrite(&record, sizeof(record), 1, out_file)) {
      syslog(LOG_ERR, "handle_inquiry_result: fwrite: %m");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

/**
 * Called by run_scan when we receive a complete EVT_INQUIRY_RESULT_WITH_RSSI
 * message.
 *
 * Writes a byte with value EVT_INQUIRY_RESULT_WITH_RSSI and then a
 * bluetrax_inquiry_result_with_rssi_t structure (in binary format) to out_file.
 *
 * Reference: [BTSPEC, volume 2, section 7.7.33, page 756]
 *
 * @param out_file to write record to
 *
 * @param time that the message was received
 *
 * @param hdr the event header
 *
 * @param data all data after the header; length is at least hdr->plen 
 *
 * @return EXIT_SUCCESS if no errors
 */
static int handle_inquiry_result_with_rssi(FILE *out_file,
    struct timeval time, hci_event_hdr *hdr, unsigned char *data)
{
  int num_rsp, i;
  inquiry_info_with_rssi *info;
  bluetrax_inquiry_result_with_rssi_t record;

  if (hdr->plen <= 0) {
    syslog(LOG_ERR, "handle_inquiry_result_with_rssi: bad plen: plen=%hhd",
        hdr->plen);
    return EXIT_FAILURE;
  }

  /* note: we never seem to get num_rsp > 1 here, but handle it anyway */
  num_rsp = data[0];
  syslog(LOG_DEBUG, "handle_inquiry_result_with_rssi: num_rsp=%d", num_rsp);

  /* sanity check */
  if (hdr->plen != num_rsp * sizeof(inquiry_info_with_rssi) + 1) {
    syslog(LOG_ERR,
        "handle_inquiry_result_with_rssi: bad plen: num_rsp=%d, plen=%hhd",
        num_rsp, hdr->plen);
    return EXIT_FAILURE;
  }

  record.time = time;
  info = (inquiry_info_with_rssi *)(data + 1);
  for (i = 0; i < num_rsp; ++i) {
    bacpy(&record.bdaddr, &info[i].bdaddr);
    memcpy(&record.dev_class, info[i].dev_class, sizeof(record.dev_class));
    record.rssi = info[i].rssi;

    /* write the event type and then the event record */
    if (EVT_INQUIRY_RESULT_WITH_RSSI !=
        fputc(EVT_INQUIRY_RESULT_WITH_RSSI, out_file)) {
      syslog(LOG_ERR, "handle_inquiry_result_with_rssi: fputc: %m");
      return EXIT_FAILURE;
    }
    if (1 != fwrite(&record, sizeof(record), 1, out_file)) {
      syslog(LOG_ERR, "handle_inquiry_result_with_rssi: fwrite: %m");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

/**
 * Write a byte with value EVT_INQUIRY_COMPLETE and then a
 * bluetrax_inquiry_complete_t structure (in binary format) to out_file.
 */
static int write_inquiry_complete(FILE *out_file,
    bluetrax_inquiry_complete_t record)
{
  if (EVT_INQUIRY_COMPLETE != fputc(EVT_INQUIRY_COMPLETE, out_file)) {
    syslog(LOG_ERR, "handle_inquiry_complete: fputc: %m");
    return EXIT_FAILURE;
  }
  if (1 != fwrite(&record, sizeof(record), 1, out_file)) {
    syslog(LOG_ERR, "handle_inquiry_complete: fwrite: %m");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/**
 * Called by run_scan when we receive a complete EVT_INQUIRY_COMPLETE message.
 *
 * Writes a byte with value EVT_INQUIRY_COMPLETE and then a
 * bluetrax_inquiry_complete_t structure (in binary format) to out_file.
 *
 * Reference: [BTSPEC, volume 2, section 7.7.1, page 715]
 *
 * @param out_file to write record to
 *
 * @param time that the message was received
 *
 * @param hdr the event header
 *
 * @param data all data after the header; length is at least hdr->plen 
 *
 * @return EXIT_SUCCESS if no errors
 */
static int handle_inquiry_complete(FILE *out_file,
    struct timeval time, hci_event_hdr *hdr, unsigned char *data)
{
  int rc;
  bluetrax_inquiry_complete_t record;

  syslog(LOG_DEBUG, "inquiry complete");

  /* sanity check */
  if (hdr->plen != 1) {
    syslog(LOG_ERR, "handle_inquiry_complete: bad plen: plen=%hhd", hdr->plen);
    return EXIT_FAILURE;
  }

  /* check for errors; abort the scan if we get one */
  errno = bt_error(data[0]);
  if (errno != 0) {
    syslog(LOG_ERR, "handle_inquiry_complete: error: %m");
    return EXIT_FAILURE;
  }

  record.time = time;

  rc = write_inquiry_complete(out_file, record);

  return rc;
}

/**
 * Set up the socket so that we get the message that we are interested in, and
 * put the device into periodic inquiry mode.
 */
static int start_scan(int dev_sd, int scan_length) {
  int opt;
  struct hci_filter flt;
  periodic_inquiry_cp info_data;
  periodic_inquiry_cp *info = &info_data;

  opt = 1;
  if (setsockopt(dev_sd, SOL_HCI, HCI_TIME_STAMP, &opt, sizeof(opt)) < 0) {
    syslog(LOG_ERR, "failed to request data timestamps: %m");
    return EXIT_FAILURE;
  }

  hci_filter_clear(&flt);
  hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
  hci_filter_set_event(EVT_INQUIRY_RESULT, &flt);
  hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
  hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
  if (setsockopt(dev_sd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
    syslog(LOG_ERR, "failed to set hci filter: %m");
    return EXIT_FAILURE;
  }

  /* no limit on number of responses per scan */
  info->num_rsp = 0x00;

  /* use the global inquiry access code (GIAC), which has 0x338b9e as its lower
   * address part (LAP) */
  info->lap[0] = 0x33;
  info->lap[1] = 0x8b;
  info->lap[2] = 0x9e;

  /* note: according to [BTSPEC, volume 2, section 7.1.3], we must have
   *   max_period > min_period > length
   * so we set these values to give us the shortest random delay between scans
   * that is permitted by the specification
   */
  info->length = scan_length;
  info->min_period = info->length + 1;
  info->max_period = info->min_period + 1;

  if (hci_send_cmd(dev_sd, OGF_LINK_CTL,
        OCF_PERIODIC_INQUIRY, PERIODIC_INQUIRY_CP_SIZE, info) < 0)
  {
    syslog(LOG_ERR, "failed to request periodic inquiry: %m");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

/**
 * The main select loop.
 *
 * @param flush flush output to disk (out_file) after every message; if 0, flush
 *        only when scan completes
 */
static int run_scan(int dev_sd, int scan_length, int flush, FILE *out_file) {
  int rc, len, flush_after_this_message;
  fd_set readfds_master, readfds;
  struct timespec select_timeout;
  sigset_t emptyset;
  unsigned char buf[HCI_MAX_FRAME_SIZE];
  unsigned char control_buf[1024]; /* arbitrary */
  struct iovec iov;
  struct msghdr msg;
  struct cmsghdr *cmsg;
  struct timeval tstamp;
  hci_event_hdr *hdr; 
  
  /* set up arguments for select */
  FD_ZERO(&readfds_master);
  FD_SET(dev_sd, &readfds_master);

  select_timeout.tv_sec = SELECT_TIMEOUT;
  select_timeout.tv_nsec = 0;

  sigemptyset(&emptyset);

  /* set up arguments for recvmsg */
  iov.iov_base = &buf;
  iov.iov_len = sizeof(buf);
  bzero(&msg, sizeof(msg));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = &control_buf;
  msg.msg_controllen = sizeof(control_buf);

  request_stop_scan = 0;
  while (!request_stop_scan)
  {
    memcpy(&readfds, &readfds_master, sizeof(readfds_master));
    rc = pselect(dev_sd + 1, &readfds, NULL, NULL, &select_timeout, &emptyset);

    if (rc < 0 && errno != EINTR) {
      syslog(LOG_ERR, "select failed: %m");
      return EXIT_FAILURE;
    } else if (rc == 0) {
      syslog(LOG_ERR, "select timed out");
      return EXIT_FAILURE;
    } else if (rc > 0) {
      if (rc != 1)
        syslog(LOG_ERR, "only one fd in set but rc > 1");

      /* OK; some data is ready */
      len = recvmsg(dev_sd, &msg, 0);
      if (len < 0 && errno != EINTR) {
        syslog(LOG_ERR, "recvmsg: %m");
        return EXIT_FAILURE;
      } else if (len > HCI_EVENT_HDR_SIZE) {
        /* process the message header to get a high-precision timestamp */
        bzero(&tstamp, sizeof(tstamp));
        cmsg = CMSG_FIRSTHDR(&msg);
        while (cmsg) {
          if (cmsg->cmsg_type == HCI_CMSG_TSTAMP) {
            tstamp = *((struct timeval *) CMSG_DATA(cmsg));
          }
          cmsg = CMSG_NXTHDR(&msg, cmsg);
        }

        /* process the message itself */
        if (buf[0] == HCI_EVENT_PKT) {
          hdr = (hci_event_hdr *)(buf + 1);
          syslog(LOG_DEBUG, "HCI_EVENT_PKT: evt=%hhd, plen=%hhd",
            hdr->evt, hdr->plen);

          /* check that we got all the data; if not, just call recvmsg again */
          if (len == 1 + HCI_EVENT_HDR_SIZE + hdr->plen) {
            /* flush either after each message or upon completion of a scan */
            flush_after_this_message = flush;

            /* dispatch on event */
            switch(hdr->evt) {
              case EVT_INQUIRY_RESULT:
                rc = handle_inquiry_result(out_file, tstamp, hdr, buf + 3);
                break;
              case EVT_INQUIRY_RESULT_WITH_RSSI:
                rc = handle_inquiry_result_with_rssi(out_file, tstamp, hdr,
                    buf + 3);
                break;
              case EVT_INQUIRY_COMPLETE:
                flush_after_this_message = 1;
                rc = handle_inquiry_complete(out_file, tstamp, hdr, buf + 3);
                break;
              default:
                rc = EXIT_SUCCESS;
                syslog(LOG_WARNING, "unknown evt=%hhd", hdr->evt);
                break;
            } 

            /* check for message processing failure */
            if (rc != EXIT_SUCCESS)
              break;

            if (flush_after_this_message) {
              fflush(out_file);
            }
          } else {
            /* this is not an error; recvmsg may have read part of a message,
             * and if we call it again, it is clever enough get the rest */
            syslog(LOG_DEBUG, "partial read from recvmsg: len=%d, plen=%hhd",
              len, hdr->plen);
          }
        } else {
          syslog(LOG_WARNING, "got non-HCI_EVENT_PKT: buf[0]=%hhd", buf[0]);
        }
      }
    }
  }

  return EXIT_SUCCESS;
}

static void print_usage(char **argv) {
  fprintf(stderr,
    "Usage: %s [options]\n\n"
    "--length n: length of each scan is approx 1.28*n seconds; default 8\n"
    "--truncate: when --file is specified, truncate it at startup\n"
    "--file file: name of file to write to; if omitted, writes to stdout\n"
    "--flush: flush output buffer after each HCI message\n"
    "--verbose: log debugging and info messages\n"
    "--verbose=0: log only errors\n"
    "--help: displays this message\n", argv[0]);
}

int main(int argc, char **argv)
{
  int scan_length = 8, truncate = 0, verbose = -1, flush = 0;
  FILE * out_file = stdout;
  int dev_id, dev_sd, opt, rc;
  bluetrax_inquiry_complete_t record;

  static struct option options[] =
  {
    {"truncate", no_argument,       0, 't'},
    {"file",     required_argument, 0, 'f'},
    {"length",   required_argument, 0, 'l'},
    {"verbose",  optional_argument, 0, 'v'},
    {"flush",    no_argument,       0, 'u'},
    {"help",     no_argument,       0, 'h'},
    {0, 0, 0, 0}
  };

  while ((opt=getopt_long(argc, argv, "+f:tl:vuh", options, NULL)) != -1) {
    switch (opt) {
    case 't':
      if (out_file != stdout) {
        syslog(LOG_ERR, "--truncate must be passed before --file");
        exit(EXIT_FAILURE);
      }
      truncate = 1;
      break;
    case 'f':
      if (truncate) {
        out_file = fopen(optarg, "w");
      } else {
        out_file = fopen(optarg, "a");
      }
      if (out_file == NULL) { 
        syslog(LOG_ERR, "failed to open output file: %m");
        exit(EXIT_FAILURE);
      }
      break;
    case 'l':
      scan_length = atoi(optarg);
      if (scan_length < 1 || scan_length > 100) { 
        fprintf(stderr, "bad scan length: %s\n", optarg);
        exit(EXIT_FAILURE);
      }
      break;
    case 'v':
      if (optarg) {
        verbose = 0;
      } else {
        verbose = 1;
      }
      break;
    case 'u':
      flush = 1;
      break;
    case 'h':
    default:
      print_usage(argv);
      exit(EXIT_SUCCESS);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 0) {
    print_usage(argv);
    exit(EXIT_FAILURE);
  }

  /* use syslog for logging */
  openlog(NULL, LOG_PID | LOG_PERROR | LOG_CONS, LOG_USER);
  switch(verbose) {
  case -1:
    setlogmask(LOG_UPTO(LOG_NOTICE)); 
    break;
  case 0:
    setlogmask(LOG_UPTO(LOG_ERR)); 
    break;
  /* else: verbose output: log everything */
  }

  if (!setup_signals()) {
    syslog(LOG_ERR, "setup_signals: %m");
    return EXIT_FAILURE;
  }

  /* use the default bluetooth device */
  dev_id = hci_get_route(NULL);
  if (dev_id < 0) {
    syslog(LOG_ERR, "hci_get_route: %m");
    return EXIT_FAILURE;
  }

  dev_sd = hci_open_dev(dev_id);
  if (dev_sd < 0) {
    syslog(LOG_ERR, "hci_open_dev: %m");
    return EXIT_FAILURE;
  }

  rc = start_scan(dev_sd, scan_length);
  if (rc == EXIT_SUCCESS) {
    /* write a fake 'complete' record with the start time of the first scan */
    gettimeofday(&record.time, NULL);
    rc = write_inquiry_complete(out_file, record);

    if (rc == EXIT_SUCCESS)
      rc = run_scan(dev_sd, scan_length, flush, out_file);

    stop_scan(dev_sd);
  }

  /* recovery:
   * restart bluetooth
   * I've seen toggling inqmode bring it back to life on my laptop after a
   * period in which nothing was being detected; I've now seem this twice
   * on my netbook: run
   * hciconfig hci0 inqmode 0
   * hciconfig hci0 inqmode 1
   * and it seems to be happy again. It was detecting only some devices (a GPS
   * logger but not my phone or computer) */

  hci_close_dev(dev_sd);

  return rc;
}

