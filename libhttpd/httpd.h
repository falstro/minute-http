#ifndef __MINUTE_HTTPD_H__
#define __MINUTE_HTTPD_H__

enum http_response_header;
struct textint;
struct iobuf;

/** \brief Structure passed to the head processing of the application and
           allows setting headers in the response.

    Much of the responsibility of setting correct headers lies with the
    application, the server only sets Server, Date, and (if appropriate)
    Connection automatically and does not check if a header has already been
    set. Thus if the application sets a header twice, it'll appear twice in the
    output, which is allowed in the HTTP standard.
*/
typedef struct
minute_httpd_head
{
  /** \brief Set a string header.

      \param ref this structure instance.
  */
  int (*string)    (enum http_response_header header,
                    const char               *value,
                    struct minute_httpd_head *ref);

  /** \brief Set a timestamp header.

      \param ref this structure instance.
  */
  int (*timestamp) (enum http_response_header header,
                    unsigned                  epochtime,
                    struct minute_httpd_head *ref);
}
minute_httpd_head;

/** \brief Structure passed to the application for reading the request payload
 */
typedef struct
minute_httpd_in
{
  /** \brief Read data from the input buffer
    * \return Number of bytes read, 0 on end of input or negative on error. */
  int (*read) (char *buf,
               unsigned count,
               struct minute_httpd_in*);
}
minute_httpd_in;

/** \brief Structure passed to the application for generating the response
 *         payload.
 */
typedef struct
minute_httpd_out
{
  /** \brief Write data to the output buffer. */
  int (*write) (const char *buf,
                unsigned count,
                struct minute_httpd_out*);
  /** \brief Force flushing of the output buffer. */
  int (*flush) (struct minute_httpd_out*);
}
minute_httpd_out;

/** \brief The callback structure provided by the application.

    The structure represents the application to be called by the HTTP server.
    It currently provides two functions, one head processing function to
    produce headers and response code, and one payload for producing the
    payload.
*/
typedef struct
minute_httpd_app
{
      /** Application header processing function.
       *
       *  Called first to examine the request and determine the outcome,
       *  including what headers to send.
       *
       *  \param request  The incoming request.
       *  \param head     API functions for writing header fields.
       *  \param text     The text buffer used to store request header values,
       *                  if requested.
       *  \param user     The supplied user data pointer.
       *
       *  \return The HTTP response code to send, e.g. 200. Return 100 to visit
       *          the request payload function regardless whether Expect:
       *          100-continue was requested by the client.
      **/
      unsigned (*header)   (minute_http_rq     *request,
                            minute_httpd_head  *head,
                            struct textint     *text,
                            void               *user);

      /** Application request payload processing function.
       *
       *  Called if the head function returns 100. If the client sent an
       *  Expect: 100-continue header, a 100 Continue will be sent before this
       *  function is called. Response headers may still be set at this point.
       *
       *  NOTE: Only called if the headers function returns 100, regardless
       *        whether the Expect: 100-continue header is set or not.
       *
       *  \param request  The incoming request.
       *  \param head     API functions for writing header fields.
       *  \param payload  API functions for reading request payload.
       *  \param text     The text buffer used to store request header values,
       *                  if requested.
       *  \param user     The supplied user data pointer.
       *
       *  \return The HTTP response code to send, e.g. 200.
      **/
      unsigned (*payload)  (minute_http_rq     *request,
                            minute_httpd_head  *head,
                            minute_httpd_in    *payload,
                            struct textint     *text,
                            void               *user);

      /** Application response payload function.
       *
       *  Called after the head function to actually produce the response
       *  payload. No headers can be set at this point, and the status code
       *  may no longer be changed.
       *
       *  NOTE: Not called if we're processing a HEAD request or if the head
       *        function returned a 204 No content, a 205 Reset content, or
       *        a 304 Not modified code.
       *
       *  \param request  The incoming request.
       *  \param output   API functions for payload output. The output is
       *                  buffered.
       *  \param text     The text buffer used to store request header values,
       *                  if requested.
       *  \param status   The status code returned by the head function.
       *  \param user     The supplied user data pointer.
       *
       *  \return Zero on succes, non-zero otherwise. In case a non-zero status
       *          is returned and no actual payload data has been sent, a
       *          generic body will be generated using the status code.
      **/
      unsigned (*response) (minute_http_rq   *request,
                            minute_httpd_out *output,
                            struct textint   *text,
                            unsigned          status,
                            void             *user);

      /** Application error function.
       *
       *  Called if we received an erronous request which could not be parsed
       *  for some reason. The connection to the client will be closed, and
       *  there can be no further communication. This function is primarily
       *  useful for logging purposes.
       *
       *  NOTE: The request may contain incomplete data, if any at all.
       *
       *  \param request  The incoming request.
       *  \param status   The status code provided by the parser, typically
       *                  in the 400-range.
       *  \param user     The user pointer.
       */
      void     (*error)  (minute_http_rq   *request,
                          unsigned          status,
                          void             *user);
}
minute_httpd_app;

enum httpd_client_status
{
  httpd_client_ok = 0,
  /** Client made no request */
  httpd_client_no_request
};

/** \brief Handle request using file descriptors.

    Handle a request by reading from the read descriptor, passing control to
    the app, with output being written to the write descriptor. There's no
    requirement that the descriptors should be distinct, as is the case when
    using sockets.

    \return Zero on success, non-zero otherwise. Positive values match
            httpd_client_status enum, negative values are the negated status
            code of the last request; returned when the parsing of the request
            failed and the connection should be closed.
*/
int   minute_httpd_handle  (int read,
                            int write,
                            minute_httpd_app*,
                            void*);

#endif
