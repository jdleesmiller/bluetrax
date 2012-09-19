Bluetrax
========

Programs for scanning for Bluetooth devices and logging the results. There are
two programs:

1. `bluetrax_scan` uses periodic inquiry mode. It returns a timestamped record
every time a device is detected. If a device is detected many times, a record is
returned for each detection. The program scans almost continuously, except for a
gap of Uniform(1.28s, 2.56s) between scans, as required by the specification.

2. `bluetrax_basic_scan` uses the higher-level HCI inquiry API. This is provided
mainly for reference; it is probably better to use `bluetrax_scan`.

These programs produce output in a packed binary format. To decode the output
into ASCII CSV, use `bluetrax_scan_unpack` and `bluetrax_basic_view` with the
above, respectively. For example, to monitor a scan interactively run

    ./bluetrax_scan -u | ./bluetrax_scan_unpack

You can run the scan on any Linux machine with a Bluetooth device. But, it helps
if you have an antenna. A friend of mine produced [a nice
write-up](http://snowdonjames.com/tracking-traffic-with-bluetooth/) of an
experiment using this code including info on the hardware.

Requirements
------------

Bluetrax uses [BlueZ](http://www.bluez.org/), the official Linux Bluetooth
protocol stack.

On Debian:
    sudo apt-get install libbluetooth-dev

On Angstrom:
    opkg install bluez4-dev

To build, just run
    make

License
-------

(The MIT license.)

Copyright (c) 2011-2012 John Lees-Miller

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

