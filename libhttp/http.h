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
http_status_code
{
    http_continue = 100,
    http_switching_protocols = 101,

    http_ok = 200,
    http_created = 201,
    http_accepted = 202,
    http_non_authoritative_information = 203,
    http_no_content = 204,
    http_reset_content = 205,
    http_partial_content = 206,

    http_multiple_choices = 300,
    http_moved_permanently = 301,
    http_found = 302,
    http_see_other = 303,
    http_not_modified = 304,
    http_use_proxy = 305,
    // reserved = 306,
    http_temporary_redirect = 307,

    http_bad_request = 400,
    http_not_authorized = 401,
    http_payment_required = 402,
    http_forbidden = 403,
    http_not_found = 404,
    http_method_not_allowed = 405,
    http_not_acceptable = 406,
    http_proxy_authentication_required = 407,
    http_request_timed_out = 408,
    http_conflict = 409,
    http_gone = 410,
    http_length_required = 411,
    http_precondition_failed = 412,
    http_request_entity_too_large = 413,
    http_request_uri_too_long = 414,
    http_unsupported_media_type = 415,
    http_requested_range_not_satisfiable = 416,
    http_expectation_failed = 417,

    http_im_a_teapot = 418,

    http_internal_server_error = 500,
    http_not_implemented = 501,
    http_bad_gateway = 502,
    http_service_unavailable = 503,
    http_gateway_timeout = 504,
    http_http_version_not_supported = 505
};

enum
http_header_flags
{
  http_no_flags         = 0,
  http_connection_close = 0x01,
  http_connection_keep  = 0x02,
  http_expect_continue  = 0x04,
  http_transfer_chunked = 0x08,
  http_content_length   = 0x10
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

  unsigned      content_length;
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
                  http_request_header enum to have them collected,
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

/** \brief Initialize request parser state for parsing trailers (i.e. jump
    straight to parsing headers).

    Parameters are the same as for minute_http_init.

    \see minute_http_init */
void      minute_http_init_trailers (unsigned          heads,
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
