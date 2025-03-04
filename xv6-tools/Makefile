# Makefile for xv6 tools

DTC ?= dtc
DTCFLAGS = -I dts -O dtb -o

all: wally-xv6.dtb

%.dtb: %.dts
	$(DTC) $(DTCFLAGS) $@ $<

clean:
	rm -f *.dtb

.PHONY: all clean
