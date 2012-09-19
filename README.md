Bluetrax
========

Programs for scanning for Bluetooth devices and logging the results. There are
two programs:

1. `bluetrax_scan` uses periodic inquiry mode. It returns a timestamped record
every time a device is detected; duplicate detections will be returned.

2. `bluetrax_basic_scan` uses the higher-level HCI inquiry API. This is provided
mainly for reference; it is probably better to use `bluetrax_scan`.

These programs produce output in a packed binary format. To decode the output
into ASCII CSV, use `bluetrax_scan_unpack` and `bluetrax_basic_view` with the
above, respectively.

Example: to monitor a scan interactively

  ./bluetrax_scan -u | ./bluetrax_scan_unpack

Requirements
------------

