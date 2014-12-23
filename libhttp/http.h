#ifndef __MINUTE_HTTP_H__
#define __MINUTE_HTTP_H__

struct iobuf;
struct textint;

enum
http_version
{
  http_unknown_version = 0,
  http_0_9 = 9,
  http_1_0 = 10,
  http_1_1 = 11
};

enum
http_method
{
  http_unknown_method = 0,
  http_get,
  http_head,
  http_post,
  http_put,
  http_options,
  http_delete,
  http_trace,
  http_connect
};
enum
http_header_flags
{
  http_no_flags         = 0,
  http_connection_close = 0x01,
  http_connection_keep  = 0x02
};

/** \brief Structure for the parsed request.

  Unsigned integers, except flags, are references into the supplied text
  IO buffer supplied to minute_http_init.
*/
typedef struct
minute_http_rq
{
  //TODO how to query collected headers?
  unsigned      flags;

  enum
  http_version  server_protocol;
  enum
  http_method   request_method;

  unsigned      path;
  unsigned      query;
}
minute_http_rq;

/** \brief Private request parser state structure.

    Clients need to instantiate this structure, thus it needs to be known. We
    could use placeholders and make sure we get the right size, but this is
    probably easier. Perhaps we should pad it to allow future extensions.

    Changes to this data will cause undefined behavior.
*/
typedef struct
minute_http_rqs
{
  unsigned        flags;
  unsigned        httpv[2];
  unsigned        tries[2];
  unsigned        m;
  unsigned        esc;
  unsigned        st;
  unsigned        est;
  unsigned        nl;
  unsigned        hmask;
  struct iobuf   *io;
  struct textint *text;
}
minute_http_rqs;

/** Initialize request parser state.

  The text part of the textint buffer stores all collected text data and is
  referenced using simple offsets. The first character in the buffer will be
  a null character, and any zero offset should be treated as unset. The int
  part of the buffer stores header name/value pairs, where the name is an
  http_request_header enum, and the value is an offset into the text buffer.

  \param  heads   Mask describing which headers to collect, zero to ignore all
                  headers, and all bits set to collect all headers. OR the
                  bits corresponding to the enum value of the
                  http_reqest_header enum to have them collected,
                  e.g.  1<<http_rq_host.
  \param  in      The IO buffer to read when parsing requests.
  \param  text    The text buffer to store path, query, header values, etc
  \param  state   The request parser state to initalize.
  \note The request parser state is tied to offsets into the IO buffer in case
        it needs to backtrack. The buffer may be relocated, but may not change
        size or have it's data shifted around within the buffer. The iobuf
        struct itself may not move as long as it's needed for minute_http_read
        calls. */
void      minute_http_init (unsigned          heads,
                            struct iobuf     *in,
                            struct textint   *text,
                            minute_http_rqs  *state);
#define MINUTE_ALL_HEADERS (~0u)
#define MINUTE_HEADER_MASK(x) (1<<(x))

/** Parse an HTTP request.
  \param  rq      The IO buffer to use when parsing requests.
  \param  state   The request parser state to initalize.
*/
unsigned  minute_http_read (minute_http_rq   *rq,
                            minute_http_rqs  *state);

#endif /* idempotent include guard */
