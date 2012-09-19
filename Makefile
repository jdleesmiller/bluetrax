#
# Requirements:
#   sudo apt-get install libbluetooth-dev
#
# Recommended:
#   sudo apt-get install bluez-hcidump
#
PROGRAMS := bluetrax_basic_scan bluetrax_basic_view
PROGRAMS += bluetrax_scan bluetrax_scan_unpack

all: ${PROGRAMS}

CFLAGS := $(CFLAGS) -Wall
LDFLAGS := $(LDFLAGS) -lbluetooth

bluetrax.o: bluetrax.h
bluetrax_basic_scan.o: bluetrax.h
bluetrax_basic_view.o: bluetrax.h
bluetrax_scan.o: bluetrax.h
bluetrax_scan_unpack.o: bluetrax.h

bluetrax_basic_view: bluetrax.o bluetrax_basic_view.o
	$(CC) -o $@ $^ $(LDFLAGS)

bluetrax_basic_scan: bluetrax.o bluetrax_basic_scan.o
	$(CC) -o $@ $^ $(LDFLAGS)

bluetrax_scan: bluetrax.o bluetrax_scan.o
	$(CC) -o $@ $^ $(LDFLAGS)

bluetrax_scan_unpack: bluetrax.o bluetrax_scan_unpack.o
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean clobber
clean:
	rm -f *.o
clobber: clean
	rm -f ${PROGRAMS}

