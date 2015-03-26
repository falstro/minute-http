#include "iobuf.h"
#include <string.h>

iobuf
minute_iobuf_init    (unsigned size, char *data)
{
  iobuf r = {0, 0, size-1, 0, data};
  return r;
}

int
minute_iobuf_put     (const char  c,
                      iobuf      *io)
{
  unsigned r = io->read;
  unsigned w = io->write;
  unsigned ri = r&io->mask;
  unsigned wi = w&io->mask;
  if(r < w && ri == wi)
    return 0;

  io->data[wi] = c;
  io->write++;
  return 1;
}

int
minute_iobuf_replace (int         offset,
                      const char  c,
                      iobuf      *io)
{
  unsigned o = offset < 0 ? io->write : io->read;
  io->data[(o+offset)&io->mask] = c;
  return 1;
}

int
minute_iobuf_write   (const char *data,
                      unsigned    sz,
                      iobuf      *io)
{
  unsigned b = io->read;
  unsigned e = io->write;
  unsigned ne = e+sz;
  char *buf = io->data;
  size_t ei = e&io->mask, nei = (ne)&io->mask;
  if (ne-b > io->mask+1)
    return -1; // wont fit..
  else if (nei > ei) {
    memcpy(buf+ei, data, sz);
  } else {
    unsigned split = io->mask+1-ei;
    memcpy(buf+ei, data, split);
    memcpy(buf, data+split, sz-split);
  }
  io->write = ne;
  return sz;
}
int
minute_iobuf_writesz (const char *text,
                      iobuf      *io)
{
  return minute_iobuf_write(text, strlen(text), io);
}

int
minute_iobuf_read  (char     *data,
                    unsigned  sz,
                    iobuf    *io)
{
  unsigned b = io->read;
  unsigned e = io->write;
  unsigned l, ei, bi = b&io->mask;

  if (e-b > sz) e = b + sz;

  l = e-b;
  ei = e&io->mask;

  if (ei < bi) {
    int tail = io->mask + 1 - bi;
    memcpy (data, io->data + bi, tail);
    memcpy (data+tail, io->data, l-tail);
  } else if (l) {
    memcpy (data, io->data + bi, l);
  }

  io->read = e;

  return l;
}
