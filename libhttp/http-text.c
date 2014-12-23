#include "http.h"
#include "http-text.h"

const char*
minute_http_version_text  (enum http_version version)
{
  switch(version) {
    case http_1_1:
      return "HTTP/1.1";
    case http_unknown_version:
    case http_1_0:
    default:
      return "HTTP/1.0";
  }
}
const char*
minute_http_response_text  (int code)
{
  switch (code) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";

    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoritative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";

    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 306: break; // reserved.
    case 307: return "Temporary Redirect";

    case 400: return "Bad Request";
    case 401: return "Not Authorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timed Out";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Request Entity Too Large";
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested Range Not Satisfiable";
    case 417: return "Expectation Failed";

    case 418: return "I'm a teapot";

    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 505: return "HTTP Version Not Supported";
  }
  switch (code/100) {
    case 1: return "Informational";
    case 2: return "Successful";
    case 3: return "Redirection";
    case 4: return "Client Error";
    case 5: return "Server Error";
  }
  return "Unknown";
}

#ifdef USE_CONTENT_TYPES
const char*
minute_http_content_type    (content_type ct)
{
  switch (ct & CONTENT_TYPE_MASK) {
    case ct_unknown:      break;
    case ct_application:  return "application";
    case ct_audio:        return "audio";
    case ct_image:        return "image";
    case ct_message:      return "message";
    case ct_model:        return "model";
    case ct_multipart:    return "multipart";
    case ct_text:         return "text";
    case ct_video:        return "video";
  }
  return "unknown";
}
const char*
minute_http_content_subtype (content_type   ct,
                             const char    *option)
{
  switch (ct)
  {
    case ct_application_javascript:   return "javascript";
    case ct_application_json:         return "json";
    case ct_application_octet_stream: return "octet-stream";
    case ct_application_pdf:          return "pdf";
    case ct_application_xml:          return "xml";
    case ct_application_atom_xml:     return "atom+xml";
    case ct_application_rdf_xml:      return "rdf+xml";
    case ct_application_xhtml_xml:    return "xhtml+xml";
    case ct_image_gif:                return "gif";
    case ct_image_jpeg:               return "jpeg";
    case ct_image_png:                return "png";
    case ct_image_tiff:               return "tiff";
    case ct_multipart_alternative:    return "alternative";
    case ct_multipart_digest:         return "digest";
    case ct_multipart_form_data:      return "data";
    case ct_multipart_mixed:          return "mixed";
    case ct_text_css:                 return "css";
    case ct_text_html:                return "html";
    case ct_text_plain:               return "plain";
    case ct_text_xml:                 return "xml";
    default:
      break;
  }
  return option?option:"unknown";
}
#endif
