ROOT+=../

tests = test-http
targets = libminute-http.a
install_library = $(targets:%=/usr/lib/%)

all: $(targets)

libminute-http.a: http.o http-text.o http-headers.o iobuf.o textint.o

test-http: test-http.o http.o http-headers.o iobuf.o textint.o

include $(ROOT)/Makefile.frame
ifeq ($(DEBUG_HTTP_READ),1)
CFLAGS+=-DDEBUG_MINUTE_HTTP_READ
endif
