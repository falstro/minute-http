#ifndef __MINUTE_TEXTINT_H__
#define __MINUTE_TEXTINT_H__

/** \brief A double headed text/integer buffer.

    Text grows up and integers grow down. It is up to the client
    to ensure that the buffer is properly aligned.
*/
typedef struct
textint
{
  unsigned text;
  unsigned ints;
  unsigned size;
  void *data;
}
textint;

/** \brief Query the number of characters stored in the buffer */
unsigned  minute_textint_textsize  (textint *text);

/** \brief Query the number of integers stored in the buffer */
unsigned  minute_textint_intsize   (textint *text);

/** \brief Append a character to the text buffer.

  \param c    the character to append.
  \param text the text buffer.
  \returns    the number of characters written, 1 if succesful, 0 if the buffer
              is full.
*/
int   minute_textint_putc  (char        c,
                            textint    *text);

/** \brief Get a pointer to a string in the buffer.

  The offset is a positive number from the start of the buffer, 0 being the
  first character, or a negative number from the end of the buffer, with -1
  being the last character in the buffer.

  \param offset the character offset to the start of the string
  \param text   the textint buffer.
  \returns      a pointer into the buffer at the specified offset.
*/
char* minute_textint_gets      (int         offset,
                                textint    *text);


/** \brief Replace a character in the buffer.

  The offset is a positive number from the start of the buffer, 0 being the
  first character, or a negative number from the end of the buffer, with -1
  being the last character in the buffer.

  \param offset the offset to replace.
  \param c      the character set.
  \param text   the textint buffer.
  \returns      the number of characters written, 1 if succesful.
*/
int   minute_textint_replacec  (int         offset,
                                char        c,
                                textint    *text);


/** \brief append an integer to the end of the int buffer

  \param i    the character to append.
  \param text the text buffer.
  \returns    the number of characters written, 1 if succesful, 0 if the buffer
              is full.
*/
int   minute_textint_puti  (int         i,
                            textint    *text);

/** \brief Get an integer from the buffer.

  The offset is a positive number from the start of the buffer, 0 being the
  first integer (i.e. at the very end of the data block), or a negative number
  from the end of the buffer, with -1 being the last integer stored in the
  buffer.

  \param offset the offset to fetch (integer offset, not byte offset).
  \param text   the textint buffer.
  \returns      the fetched integer.
*/
int   minute_textint_geti  (int         offset,
                            textint    *text);

/** \brief Replace an integer in the buffer.

  The offset is a positive number from the start of the buffer, 0 being the
  first integer (i.e. at the very end of the data block), or a negative number
  from the end of the buffer, with -1 being the last integer stored in the
  buffer.

  \param offset the offset to replace (integer offset, not byte offset).
  \param i      the integer set.
  \param text   the textint buffer.
  \returns      the number of characters written, 1 if succesful.
*/
int   minute_textint_replacei  (int         offset,
                                const int   i,
                                textint    *text);

#endif /* idempotent include guard */
