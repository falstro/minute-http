#ifndef __MINUTE_IOBUF_UTIL_H__
#define __MINUTE_IOBUF_UTIL_H__

struct iovec;

int   minute_iobuf_readfd  (int         fd,
                            iobuf      *io);
int   minute_iobuf_flushfd (int         fd,
                            iobuf      *io);
int   minute_iobuf_gather  (struct iovec *a,
                            struct iovec *b,
                            iobuf      *io);
int   minute_iobuf_printf  (iobuf      *io,
                            const char *fmt, ...);

#endif /* idempoten include guard */
