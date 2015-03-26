#ifndef __MINUTE_IOBUF_H__
#define __MINUTE_IOBUF_H__

/** \brief A simple input/output ring buffer.

  The iobuf represents an i/o ring buffer, with absolute read and write offsets.
  Both read and write increase, and only the access is wrapped, thus either may
  well be larger than the size of the buffer, but they may never be further than
  the buffer size apart. If read and write are equal, the buffer is empty. The
  buffer is full if read is less than write, but both map to the same data
  index.

  \note The size of the buffer must be a power of two, as all accesses are
        masked with the mask value to properly wrap the index.
*/
typedef struct
iobuf
{
  unsigned  read;
  unsigned  write;
  unsigned  mask;
  unsigned  flags;
  char     *data;
}
iobuf;

#define IOBUF_EOF   0x01

#define minute_iobuf_used(io) (((io).write-(io).read))
#define minute_iobuf_free(io) ((io).mask+1-(minute_iobuf_used(io)))

/** \brief Constructor function for initializing an iobuf buffer */
iobuf minute_iobuf_init    (unsigned size, char *data);

/** \brief Append a character to the end of the buffer.

  \param c    the character to append.
  \param io   the io buffer.
  \returns    the number of characters written, 1 if succesful, 0 if the buffer
              is full.
*/
int   minute_iobuf_put     (char        c,
                            iobuf      *io);

/** \brief Replace a character in the buffer.

  The offset is a positive number from the start of the buffer, 0 being the
  first character (i.e. at the read index), or a negative number from the end
  of the buffer, with -1 being the last character in the buffer.

  \param offset the offset to replace.
  \param c      the character set.
  \param io     the io buffer.
  \returns      the number of characters written, 1 if succesful.
*/
int   minute_iobuf_replace (int         offset,
                            char        c,
                            iobuf      *io);

/** \brief Write nelem number of bytes to the IO buffer.

  \param data   the data buffer to be read from.
  \param nelem  the number of bytes to write
  \param io     the io buffer to write to.
  \returns      the number of bytes actually written, or -1 if an error occurs,
                i.e. the data did not fit in the buffer.
*/
int   minute_iobuf_write   (const char *data,
                            unsigned    nelem,
                            iobuf      *io);

/**\brief Write zero-terminated string to the IO buffer.

  \param data   the data buffer to be read from.
  \param io     the io buffer to write to.
  \returns      the number of bytes actually written, or -1 if an error occurs,
                i.e. the data did not fit in the buffer.
*/
int   minute_iobuf_writesz (const char *text,
                            iobuf      *io);

/** \brief Read at most nelem number of bytes to the IO buffer.

  \param data   the data buffer to be written to.
  \param nelem  the space available in the data buffer.
  \param io     the io buffer to read from.
  \returns      the number of bytes actually written to the data buffer, 0 if
                there's currently no more data in the buffer or if nelem is 0.
*/
int   minute_iobuf_read    (char     *data,
                            unsigned  nelem,
                            iobuf    *io);

#endif /* idempotent include guard */
