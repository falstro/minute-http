SUBS=libhttp libhttpd minuted

all: all-recursive
clean: clean-recursive
check: check-recursive

ROOT=.
include $(ROOT)/Makefile.frame
