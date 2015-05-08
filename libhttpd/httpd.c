#include "libhttp/iobuf.h"
#include "libhttp/textint.h"
#include "libhttp/http.h"
#include "libhttp/http-headers.h"
#include "libhttp/http-text.h"
#include "iobuf-util.h"
#include "httpd.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <time.h>

#define BIT(x) (1ul<<(x))

#define SERVER_NAME "minuted"
#define SERVER_VERSION "0.1"

#define NL "\r\n"

typedef enum
httpd_header_flags
{
  httpd_te_chunked       = 0x01,
  // always use chunked on keep-alive
  httpd_connection_keep  = 0x02|httpd_te_chunked
}
httpd_header_flags;

typedef struct
httpd_head
{
  minute_httpd_head base;
  unsigned          flags;
}
httpd_head;

typedef struct
httpd_in
{
  minute_httpd_in base;
  int             pending;
}
httpd_in;

typedef struct
httpd_out
{
  minute_httpd_out  base;
}
httpd_out;

//TODO rename?
typedef struct
httpd_response
{
  minute_httpd_state *state;
  minute_http_rq  rq;
  httpd_head      head;
  httpd_in        in;
  httpd_out       out;
}
httpd_response;

#ifndef offsetof
# define offsetof(type,memb) ((char*)&(((type*)0)->memb)-((char*)0))
#endif
#define downcast(type,memb,var) ((type*)(((char*)var)-offsetof(type,memb)))

#define PENDING_INIT -1
#define PENDING_EOF -2
#define PENDING_ERROR -3

typedef enum
{
  chunk_previous,
  chunk_previous_cr,
  chunk_size,
  chunk_extension,
  chunk_cr,
  chunk_error
}
chunk_state;

static int
minute_httpd_read_request(httpd_response   *resp,
                          minute_http_rqs  *rqs)
{
  minute_httpd_state *state = resp->state;
  iobuf *in = &state->in;
  int status;
  if(minute_iobuf_used(*in) > 0)
    goto prefilled; //yes, gotos do have proper uses.
  do {
    {
      //TODO connection timeout on keep-alive.
      //TODO serve multiple connections in same process?
      int r = minute_iobuf_readfd(state->infd, in);
      if(r < 0) {
        status = http_request_uri_too_long;
        break;
      } else if (!r && in->flags & IOBUF_EOF) {
        return -1;
      }
    }
    prefilled: ;
  } while((status = minute_http_read (&resp->rq, rqs)) == EAGAIN);

  return status;
}

static int
minute_httpd_in_read(char *buf, unsigned count, minute_httpd_in* in)
{
  httpd_response *resp = downcast(httpd_response, in.base, in);
  minute_httpd_state *state = resp->state;

  if(resp->in.pending <= PENDING_EOF)
    return 0;

  while(1) {
    if (resp->in.pending > 0) {
      if(state->in.read < state->in.write) {
        int r, toread = resp->in.pending > count ? count : resp->in.pending;
        if (buf) {
          r = minute_iobuf_read (buf, toread, &state->in);
        } else {
          if (state->in.read + toread > state->in.write)
            toread = state->in.write - state->in.read;
          state->in.read += toread;
          r = toread;
        }
        resp->in.pending -= r;
        return r;
      }
      // read more.
    } else if (resp->rq.flags & http_transfer_chunked) {
      iobuf i = state->in;

      unsigned val = 0;
      chunk_state cs = resp->in.pending == 0 ? chunk_previous : chunk_size;
#define reset(c) do{cs=c;goto top;}while(0)
#define shift(c) do{cs=c;}while(0)
      for(unsigned bi = i.read&i.mask; i.read < i.write; bi=++i.read&i.mask) {
        int c = i.data[bi];
        top: switch(cs)
        {
          case chunk_previous:
            if (c == 13)
              shift(chunk_previous_cr);
            else if (c == 10)
              reset(chunk_previous_cr);
            else
              reset(chunk_error);
            break;
          case chunk_previous_cr:
            if (c == 10)
              shift(chunk_size);
            else
              reset(chunk_error);
            break;
          case chunk_size:
            if (c >= '0' && c <= '9')
              val = (val<<4) + (c-'0');
            else if (c >= 'a' && c <= 'f')
              val = (val<<4) + (c-'a') + 0xa;
            else if (c >= 'A' && c <= 'F')
              val = (val<<4) + (c-'A') + 0xA;
            else if (c == ';')
              shift(chunk_extension);
            else if (c == 13)
              shift(chunk_cr);
            else if (c == 10)
              reset(chunk_cr);
            else
              reset(chunk_error);
            break;
          case chunk_extension:
            if (c == 13)
              shift(chunk_cr);
            else if (c == 10)
              reset(chunk_cr);
            break;
          case chunk_cr:
            if (c == 10) {
              ++i.read;
              state->in = i;
              if (val == 0) {
                int status;
                resp->in.pending = PENDING_EOF;
                minute_http_rqs rqs = {};
                //TODO only headers specified in the Trailers header?
                minute_http_init_trailers(MINUTE_ALL_HEADERS,
                  &state->in, &state->text, &rqs);
                status = minute_httpd_read_request (resp, &rqs);
                //any non-zero status means failure.
                return status ? -1 : 0;
              } else {
                resp->in.pending = val;
                return minute_httpd_in_read (buf, count, in); //tail recursion
              }
            } else {
              reset(chunk_error);
            }
            break;
          case chunk_error:
            resp->in.pending = PENDING_ERROR;
            return -1;
        }
#undef shift
#undef reset
        if (i.read == i.write) {
            // buffer is full; someone's using a big extension.
            resp->in.pending = PENDING_ERROR;
            return -1;
        }
      }
      // read more.
    } else {
      // not chunked, we're done here. This won't consume the cr/lf at the
      // end of the payload, however 1) the request parser will skip leading
      // cr/lf, and 2) if there is no payload, we won't have a trailing
      // cr/lf.
      resp->in.pending = PENDING_EOF;
      return 0;
    }

    int rfd = minute_iobuf_readfd (state->infd, &state->in);
    if(!rfd && state->in.flags & IOBUF_EOF) {
        resp->in.pending = PENDING_EOF;
        return 0;
    }
  }

  /* unreachable */
}

static void
minute_httpd_in_discard(httpd_response *resp)
{
  minute_httpd_state *state = resp->state;
  while(0<minute_httpd_in_read(NULL, state->in.mask+1, &resp->in.base))
    ;
}

static int
minute_httpd_chunk_end(httpd_response *resp)
{
  minute_httpd_state *state = resp->state;
  if (resp->head.flags & httpd_te_chunked) {
    if(write (state->outfd, "0" NL NL, 5) < 0) {
      close(state->outfd);
      state->outfd = -1;
    }
  }
  return 0;
}

static int
minute_httpd_chunk(const char      *buf,
                   unsigned         count,
                   int              chunked,
                   httpd_response  *resp)
{
  char chunksz[8];
  struct iovec iov[5] = {};
  int r, c = 0;
  minute_httpd_state *state = resp->state;
  unsigned used  = minute_iobuf_used(state->out);
  unsigned total = used + count;
  if (total == 0)
    return 0; // don't output a zero chunk, as it would terminate the transfer
  else if (chunked) {
      iov[0].iov_len = snprintf (chunksz, sizeof(chunksz), "%x" NL, total);
      iov[0].iov_base = chunksz;
      iov[4].iov_len = 2;
      iov[4].iov_base = NL;
      c = 5;
  } else {
      iov[0].iov_len = 0;
      iov[4].iov_len = 0;
      c = 4;
  }
  minute_iobuf_gather (&iov[1], &iov[2], &state->out);
  iov[3].iov_base = (void*) buf;
  iov[3].iov_len = count;
  // TODO perhaps use function pointers instead of writev directly,
  // to simplify testing?
  r = writev (state->outfd, iov, c);
  if (r > 0) {
    unsigned prefix = iov[0].iov_len;
    if (r < prefix) // we weren't even able to print the chunk size?
      return -1;

    int cwritten;
    if (r < prefix + used) {
      state->out.read += r - prefix;
      cwritten = 0;
    } else {
      state->out.read += used;
      cwritten = r - prefix - used;
    }

    if (r < total) {
      // Unable to write the entire chunk. Try again, block if we have to.
      int rr = minute_httpd_chunk (buf + cwritten, count - cwritten, 0, resp);
      if(rr < 1)
        return rr;
      r += rr;
    }
  } else {
    close(state->outfd);
    state->outfd = -1;
  }
  return r;
}
static int
minute_httpd_flush   (minute_httpd_out *o)
{
  httpd_response *resp = downcast(httpd_response, out.base, o);
  minute_httpd_chunk (0, 0, (resp->head.flags & httpd_te_chunked), resp);
  return 0;
}

static int
minute_httpd_output (const char      *append,
                     unsigned         count,
                     int              chunked,
                     httpd_response  *resp)
{
  minute_httpd_state *state = resp->state;
  if (minute_iobuf_free(state->out) < count)
    minute_httpd_chunk(append, count, chunked, resp);
  else
    minute_iobuf_write(append, count, &state->out);
  return count;
}
static inline int
minute_httpd_print  (const char *append,
                     int chunked,
                     httpd_response *resp)
{ return minute_httpd_output(append, strlen(append), chunked, resp); }

static int
minute_httpd_write   (const char       *buf,
                      unsigned          count,
                      minute_httpd_out *o)
{
  httpd_response *resp = downcast(httpd_response, out.base, o);
  return minute_httpd_output(buf, count,
              (resp->head.flags & httpd_te_chunked), resp);
}

static int
minute_httpd_header (enum http_response_header  header,
                     const char                *value,
                     minute_httpd_head         *head)
{
  // TODO if the output buffer is too small, we will erroneously flush it
  // before writing the status, i.e. TODO suppress flushing the output buffer
  httpd_response *resp = downcast(httpd_response, head, head);
  // TODO check header? Multiline header?
  minute_httpd_print (http_response_header_names[header], 0, resp);
  minute_httpd_output (": ", 2, 0, resp);
  minute_httpd_print (value, 0, resp);
  minute_httpd_output (NL, 2, 0, resp);
  return 0;
}

int
minute_httpd_start (httpd_response  *resp)
{
  resp->head.flags = 0;
  if (resp->rq.server_protocol >= http_1_1)
  {
    if (! (resp->rq.flags & http_connection_close))
      resp->head.flags |= httpd_connection_keep;
  }
  else
  {
    if (resp->rq.flags & http_connection_keep)
      resp->head.flags |= httpd_connection_keep;
  }

  if (resp->rq.flags & http_transfer_chunked)
    resp->in.pending = PENDING_INIT;
  else if(resp->rq.flags & http_content_length)
    resp->in.pending = resp->rq.content_length;
  // TODO What about non-keep-alive connections?

  return 0;
}

struct
minute_rfc_datetime
{// "Sun, 06 Nov 1994 08:49:37 GMT"
  char string[32];
};

static char *
minute_rfc_date (time_t t,
                 struct minute_rfc_datetime *rfc)
{
  struct tm tm;
  static char *day[7] = {
    "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
  };
  static char *mon[12] = {
    "Jan","Feb","Mar","Apr",
    "May","Jun","Jul","Aug",
    "Sep","Oct","Nov","Dec"
  };
  gmtime_r (&t, &tm);

  snprintf (rfc->string, sizeof(rfc->string),
            "%s, %02d %s %d %02d:%02d:%02d GMT",
            day[tm.tm_wday], tm.tm_mday, mon[tm.tm_mon], 1900+tm.tm_year,
            tm.tm_hour, tm.tm_min, tm.tm_sec);

  return rfc->string;
}

static int
minute_httpd_header_timestamp  (enum http_response_header type,
                                unsigned            epochtime,
                                minute_httpd_head  *head)
{
  struct minute_rfc_datetime rfc;
  return minute_httpd_header(type, minute_rfc_date (epochtime, &rfc), head);
}

void
minute_httpd_standard_body (unsigned        status,
                            httpd_response *resp)
{
  const char *msg = minute_http_response_text(status);
  minute_httpd_state *state = resp->state;
  minute_iobuf_printf (&state->out,
    "<html><head>"
      "<title>%d %s</title>"
    "</head><body>"
    "<h1>%d %s</h1>"
    "</body></html>\n",
    status, msg, status, msg
  );
}

void
minute_httpd_init    (int       readfd,
                      int       writefd,
                      iobuf     in,
                      iobuf     out,
                      textint   text,
                      minute_httpd_state *state)
{
  memset (state, 0, sizeof(*state));

  state->in = in;
  state->out = out;
  state->text = text;

  state->infd = readfd;
  state->outfd = writefd;
}

int
minute_httpd_handle  (minute_httpd_app *app,
                      minute_httpd_state *state,
                      void *user)
{
  // requests may be pipelined, do not reset the iobuffer each iteration.
  httpd_response  resp = {
    state,
    {}, /* rq */
    { /* httpd_head */
      { /* minute_http_head */
        minute_httpd_header,
        minute_httpd_header_timestamp
      },
      0 /* flags */
    },
    { /* httpd_in */
      {
        minute_httpd_in_read
      },
      0
    },
    { /* httpd_out */
      {
        minute_httpd_write,
        minute_httpd_flush
      }
    }
  };

  char head[64];
  int nhead;
  int status = 0;
  memset (&resp.rq, 0, sizeof(resp.rq));

  minute_iobuf_clear(&state->out);
  minute_textint_clear(&state->text);

  //TODO parameterized header mask, remember to use for trailers too.
  minute_http_rqs rqs = {};
  minute_http_init(MINUTE_ALL_HEADERS, &state->in, &state->text, &rqs);

  status = minute_httpd_read_request(&resp, &rqs);

  if (status < 0) {
    // client closed connection.
    return httpd_client_no_request;
  } else if (status) {
    nhead = snprintf (head, sizeof(head), "%s %d %s" NL,
                      minute_http_version_text(resp.rq.server_protocol),
                      status, minute_http_response_text(status));

    // TODO don't write() directly
    if(0 >= write (state->outfd, head, nhead)) {
      close(state->outfd);
      state->outfd = -1;
    }

    resp.head.flags = 0;

    // TODO parameterize the Server: header.
    minute_httpd_header (http_rsp_server, SERVER_NAME "/" SERVER_VERSION,
                        &resp.head.base);
    minute_httpd_header_timestamp (http_rsp_date, time(0), &resp.head.base);

    if (resp.rq.server_protocol == http_1_1)
      minute_httpd_header (http_rsp_connection, "close", &resp.head.base);

    minute_httpd_chunk (NL, 2, 0, &resp);
    minute_httpd_standard_body(status, &resp);
    minute_httpd_flush (&resp.out.base);
    minute_httpd_chunk_end (&resp);
    app->error (&resp.rq, status, user);
    return -status;
  } else {
    unsigned headermark, nproto;
    minute_httpd_start (&resp);

    minute_httpd_header (http_rsp_server, SERVER_NAME "/" SERVER_VERSION,
                        &resp.head.base);
    minute_httpd_header_timestamp (http_rsp_date, time(0), &resp.head.base);

    nproto = snprintf (head, sizeof(head), "%s ",
                       minute_http_version_text (resp.rq.server_protocol));

    status=app->header (&resp.rq, &resp.head.base, &state->text, user);
    if (100 == status) {
      if (resp.rq.flags & http_expect_continue) {
        nhead = snprintf (head+nproto, sizeof(head)-nproto, "%d %s" NL,
                          status, minute_http_response_text(status))+nproto;
        // TODO don't write() ...
        if(0 >= write (state->outfd, head, nhead)) {
          close(state->outfd);
          state->outfd = -1;
        }
      }

      status = app->payload (&resp.rq, &resp.head.base, &resp.in.base,
                             &state->text, user);
    }

    nhead = snprintf (head+nproto, sizeof(head)-nproto, "%d %s" NL,
                      status, minute_http_response_text(status))+nproto;

    // TODO don't write() directly, use some chunk/iobuf function instead to
    // prepend the status. That way we have a single point of contact to
    // the file descriptors.
    if(0 >= write (state->outfd, head, nhead)) {
      close(state->outfd);
      state->outfd = -1;
    }

    if ((resp.head.flags&httpd_connection_keep) == httpd_connection_keep)
    {
      if (resp.rq.server_protocol < http_1_1)
        minute_httpd_header (http_rsp_connection, "keep-alive",
          &resp.head.base);
    }
    else
    {
      if (resp.rq.server_protocol == http_1_1)
        minute_httpd_header (http_rsp_connection, "close", &resp.head.base);
    }

    // Chunked is always accepted in 1.1
    // TODO check TE if accepted when using 1.0; if not return error?
    if (resp.head.flags & httpd_te_chunked)
      minute_httpd_header (http_rsp_transfer_encoding, "chunked",
        &resp.head.base);

    minute_httpd_chunk (NL, 2, 0, &resp);
    // do not call response on HEAD request, or if we return a code implying
    // that there is nothing to be sent (e.g. no content or not modified)
    if (resp.rq.request_method != http_head) switch(status) {
      case http_no_content:
      case http_reset_content:
      case http_not_modified:
        break;
      default: {
        unsigned response =
          app->response(&resp.rq,
                        &resp.out.base,
                        &resp.in.base,
                        &state->text,
                        status,
                        user);
        headermark = state->out.write; // end of the headers.
        if (response && state->out.write == headermark)
        {
          // only send if the app payload returned non-zero, and it hasn't
          // written anything beyond the headers.
          minute_httpd_standard_body(status, &resp);
        }
      }
    }
  }

  minute_httpd_flush (&resp.out.base);
  minute_httpd_chunk_end (&resp);
  minute_httpd_in_discard (&resp);
  if ((resp.head.flags & httpd_connection_keep) == httpd_connection_keep)
    return httpd_client_ok_open;
  return httpd_client_ok_close;
}
