ROOT+=../

LIBS=-ltcl8.5 -lpthread -lrt

tests =
targets = minuted
install_program = /usr/sbin/minuted

all: $(targets)

minuted: main.o minuted.o tap.o config.o \
    $(ROOT)/libhttpd/libminute-httpd.a \
    $(ROOT)/libhttp/libminute-http.a

include $(ROOT)/Makefile.frame

CFLAGS+=-D_POSIX_C_SOURCE=200112L
ifeq ($(SINGLE),1)
CFLAGS+=-DMINUTED_SINGLE_PROCESS
endif
