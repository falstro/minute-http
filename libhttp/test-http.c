#include "iobuf.h"
#include "textint.h"
#include "http.h"
#include "http-headers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <assert.h>

#define BIT(x) (1ul<<(x))

int
main (void)
{
  char message[] =
    "GET /test/uri?with&query-string HTTP/1.1\r\n"
    "Host: minute.example.org\r\n"
    "Connection: close\r\n"
    "Expect: 100-continue\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Content-Length: 27\r\n"
    "\r\n"
    "1b\r\n"
    "This data shouldn't be read\r\n"
    "0\r\n"
    "\r\n";
  unsigned payloadOffset = strstr(message,"\r\n\r\n")-message+4;
  char textbuf[0x40];

  iobuf   input = {0, sizeof(message)-1, 0xff, IOBUF_EOF, message};
  textint text = {0, 0x40, 0x40, textbuf};

  minute_http_rqs rqs;
  minute_http_rq  request = {};

  int             result;

  minute_http_init(MINUTE_ALL_HEADERS, &input, &text, &rqs);

  result = minute_http_read (&request, &rqs);
  if (result) {
    fprintf (stderr, "minute_http_read: %d\n", result);
    exit (1);
  }

  assert(request.request_method == http_get);
  assert(request.server_protocol == http_1_1);
  assert(request.flags & http_connection_close);
  assert(request.flags & http_expect_continue);
  assert(request.flags & http_transfer_chunked);
  assert(request.flags & http_content_length);
  assert(27 == request.content_length);
  assert(0 == strcmp(&textbuf[request.path], "/test/uri"));
  assert(0 == strcmp(&textbuf[request.query], "with&query-string"));
  assert(input.read == payloadOffset);
  assert(minute_textint_intsize(&text) == 2);
  assert(minute_textint_geti(0, &text) == http_rq_host);
  int hosti = minute_textint_geti(1, &text);
  assert(0 == strcmp(&textbuf[hosti], "minute.example.org"));
  assert(0 == strcmp("Warning",http_request_header_names[http_rq_warning]));
  return 0;
}
