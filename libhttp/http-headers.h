#ifndef __HTTP_HEADERS_H__
#define __HTTP_HEADERS_H__

enum
http_request_header
{
  http_rq_unknown_header = 0,
  http_rq_accept,
  http_rq_accept_charset,
  http_rq_accept_encoding,
  http_rq_accept_language,
  http_rq_authorization,
  http_rq_cache_control,
  http_rq_connection,
  //http_rq_content_encoding,
  //http_rq_content_language,
  //http_rq_content_length,
  //http_rq_content_location,
  //http_rq_content_md5,
  //http_rq_content_type,
  http_rq_cookie,
  http_rq_date,
  http_rq_expect,
  http_rq_from,
  http_rq_host,
  //http_rq_if_match,
  http_rq_if_modified_since,
  http_rq_if_none_match,
  http_rq_if_range,
  http_rq_if_unmodified_since,
  http_rq_max_forwards,
  //http_rq_origin,
  http_rq_pragma,
  http_rq_proxy_authorization,
  http_rq_range,
  http_rq_referer,
  http_rq_te,
  http_rq_trailer,
  http_rq_transfer_encoding,
  http_rq_upgrade,
  http_rq_user_agent,
  http_rq_via,
  http_rq_warning
};


enum
http_response_header
{
  http_rsp_unknown_header = 0,
  http_rsp_accept_ranges,
  http_rsp_age,
  http_rsp_allow,
  http_rsp_cache_control,
  http_rsp_connection,
  http_rsp_content_encoding,
  http_rsp_content_language,
  http_rsp_content_length,
  http_rsp_content_location,
  http_rsp_content_md5,
  http_rsp_content_disposition,
  http_rsp_content_range,
  http_rsp_content_type,
  http_rsp_date,
  http_rsp_etag,
  http_rsp_expires,
  http_rsp_last_modified,
  http_rsp_link,
  http_rsp_location,
  http_rsp_p3p,
  http_rsp_pragma,
  http_rsp_proxy_authenticate,
  http_rsp_refresh,
  http_rsp_retry_after,
  http_rsp_server,
  http_rsp_set_cookie,
  http_rsp_strict_transport_security,
  http_rsp_trailer,
  http_rsp_transfer_encoding,
  http_rsp_vary,
  http_rsp_via,
  http_rsp_warning,
  http_rsp_www_authenticate
};

extern const char*
http_request_header_names[];
extern int
http_request_header_names_count;

extern const char*
http_response_header_names[];
extern int
http_response_header_names_count;

#endif /* idempotent include guard */
