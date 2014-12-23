ROOT+=../

tests = test-httpd
targets = libminute-httpd.a
install_library = $(targets:%=/usr/lib/%)

all: $(targets)

libminute-httpd.a: httpd.o iobuf-util.o
test-httpd: test-httpd.o httpd.o iobuf-util.o ../libhttp/libminute-http.a

include $(ROOT)/Makefile.frame

CFLAGS+=-D_POSIX_C_SOURCE=200112L
