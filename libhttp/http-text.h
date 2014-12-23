#ifndef __MINUTE_HTTP_TEXT_H__
#define __MINUTE_HTTP_TEXT_H__

const char*   minute_http_version_text   (enum http_version version);
const char*   minute_http_response_text  (int code);

#ifdef USE_CONTENT_TYPES
const char*   minute_http_content_type    (content_type   ct);
const char*   minute_http_content_subtype (content_type   ct,
                                           const char    *option);
#endif

#endif /* idempotent include guard */
