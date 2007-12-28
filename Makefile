exec_prefix = /
libdir = $(exec_prefix)/lib

INSTALL = install
CC = gcc
CFLAGS = -O2 -Wall
LD = ld

ALL_CFLAGS = $(CFLAGS) -fPIC
ALL_LDFLAGS = $(LDFLAGS) -shared -Wl,-x

all: libnss_afspag.so.2 linktest

libnss_afspag.so.2: nss_afspag.o
	$(CC) -o $@ $(ALL_LDFLAGS) -Wl,-soname,$@ $^ $(LOADLIBES) $(LDLIBS)

%.o: %.c
	$(CC) -c $(ALL_CFLAGS) $(CPPFLAGS) $<

linktest: libnss_afspag.so.2
	$(LD) --entry=0 -o /dev/null $^

install: libnss_afspag.so.2
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -m a+r,u+w $< $(DESTDIR)$(libdir)/

clean:
	rm -f *.so.* *.o

.PHONY: all linktest install clean
