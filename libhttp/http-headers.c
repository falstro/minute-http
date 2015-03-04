#include "http-headers.h"

const char*
http_request_header_names[] =
{
  "X-Unknown-Header",
  "Accept",
  "Accept-Charset",
  "Accept-Encoding",
  "Accept-Language",
  "Authorization",
  "Cache-Control",
  "Connection",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-MD5",
  "Content-Type",
  "Cookie",
  "Date",
  "Expect",
  "From",
  "Host",
  "If-Match",
  "If-Modified-Since",
  "If-None-Match",
  "If-Range",
  "If-Unmodified-Since",
  "Max-Forwards",
  "Origin",
  "Pragma",
  "Proxy-Authorization",
  "Range",
  "Referer",
  "TE",
  "Trailer",
  "Transfer-Encoding",
  "Upgrade",
  "User-Agent",
  "Via",
  "Warning"
};

int http_request_header_names_count =
  sizeof(http_request_header_names)/sizeof(http_request_header_names[0]);

const char*
http_response_header_names[] =
{
  "X-Unknown-Header",
  "Accept-Ranges",
  "Age",
  "Allow",
  "Cache-Control",
  "Connection",
  "Content-Encoding",
  "Content-Language",
  "Content-Length",
  "Content-Location",
  "Content-MD5",
  "Content-Disposition",
  "Content-Range",
  "Content-Type",
  "Date",
  "ETag",
  "Expires",
  "Last-Modified",
  "Link",
  "Location",
  "P3P",
  "Pragma",
  "Proxy-Authenticate",
  "Refresh",
  "Retry-After",
  "Server",
  "Set-Cookie",
  "Strict-Transport-Security",
  "Trailer",
  "Transfer-Encoding",
  "Vary",
  "Via",
  "Warning",
  "WWW-Authenticate"
};

int http_response_header_names_count =
  sizeof(http_response_header_names)/sizeof(http_response_header_names[0]);
