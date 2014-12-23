#include "libhttp/iobuf.h"
#include "iobuf-util.h"

#include <sys/uio.h>
#include <stdarg.h>
#include <stdio.h>

int
minute_iobuf_readfd  (int         fd,
                      iobuf      *io)
{
  struct iovec iov[2];
  size_t r;
  unsigned b = io->read;
  unsigned e = io->write;
  char *buf = io->data;
  size_t bi = b&io->mask, ei = e&io->mask;
  iov[0].iov_base = buf+ei;
  iov[0].iov_len  = bi>ei?bi-ei:io->mask+1-ei;
  iov[1].iov_base = buf;
  iov[1].iov_len  = bi>ei?0:bi;
  if (b != e && ei-bi == 0) {
    // no more buffer space
    // http 414 Request-URI Too Long ?
    return -1;
  } else if (0>(r = readv (fd, iov, 2))) {
    // error...
  } else if (0<r) {
      e += r;
      io->write = e;
  } else {
      // r == 0  end of file..
      io->flags |= IOBUF_EOF;
  }
  return r;
}

int
minute_iobuf_gather  (struct iovec *A,
                      struct iovec *B,
                      iobuf        *io)
{
  unsigned b = io->read;
  unsigned e = io->write;
  char *buf = io->data;
  size_t bi = b&io->mask, ei = e&io->mask;

  if(e > b) {
    A->iov_base = buf+bi;
    A->iov_len  = bi<ei?ei-bi:io->mask+1-bi;
    B->iov_base = buf;
    B->iov_len  = bi<ei?0:ei;
    return bi < ei ? 1 : 2;
  } else {
    return 0;
  }
}

int
minute_iobuf_flushfd (int         fd,
                      iobuf      *io)
{
  struct iovec iov[2];
  int r = minute_iobuf_gather(&iov[0], &iov[1], io);
  r = writev(fd, iov, r);
  if (r > 0)
    io->read += r;
  return r;
}

int
minute_iobuf_vprintf (iobuf      *io,
                      const char *fmt, va_list ap)
{
  int r;
  char buffer[io->mask+1+1];
  r = vsnprintf(buffer, sizeof(buffer), fmt, ap);
  return minute_iobuf_write(buffer, r, io);
}
int
minute_iobuf_printf  (iobuf      *io,
                      const char *fmt, ...)
{
  int r;
  va_list ap;
  va_start (ap, fmt);
  r = minute_iobuf_vprintf(io, fmt, ap);
  va_end (ap);
  return r;
}
