#include "iobuf.h"
#include "textint.h"
#include "http.h"
#include "http-headers.h"

#include <errno.h>
#include <stddef.h>

/* RFC: 2616 */

typedef enum http_header http_header;
typedef enum
{
  http_unknown_keyword = 0,
  http_proto
}
http_keywords;

typedef struct patricia
{
  const char   *prefix;
  unsigned char check;
  unsigned char next;
  unsigned char offset;
  // padding...
  unsigned      terminal;
}
patricia;
typedef struct triestate
{
  // unsigned state; // current state
  unsigned poff;  // prefix offset
  unsigned slot;  // current table slot
}
triestate;
                      // Check
static const          //    Next
patricia headers[] =  //       Offset
{                     //          Terminal
  {"a",                  0, 1, 4, 0},
  {NULL,                 0, 0, 0, 0},
  {"c",                  0, 8,13, 0},
  {"date",               0,99, 0, http_rq_date},
  {"expect",             0,99, 0, http_rq_expect},
  {"from",               0,99, 0, http_rq_from},
  {"ccept",              1, 2, 0, http_rq_accept},
  {"host",               0,99, 0, http_rq_host},
  {"if-",                0,26,30, 0},
  {"charset",            3,99, 0, http_rq_accept_charset},
  {"e",                 41,99, 0, http_rq_te},
  {"encoding",           3,99, 0, http_rq_accept_encoding},
  {"max-forwards",       0,99, 0, http_rq_max_forwards},
  {"ache-control",       8,99, 0, http_rq_cache_control},
  {"origin",             0,99, 0, http_rq_origin},
  {"pr",                 0,35,25, 0},
  {"atch",              27,99, 0, http_rq_if_match},
  {"r",                  0,38,28, 0},
  {"language",           3,99, 0, http_rq_accept_language},
  {"t",                  0,41, 6, 0},
  {"u",                  0,46,31, 0},
  {"via",                0,99, 0, http_rq_via},
  {"warning",            0,99, 0, http_rq_warning},
  {"ra",                41,43,21, 0},
  {"uthorization",       1,99, 0, http_rq_authorization},
  {"agma",              35,99, 0, http_rq_pragma},
  {"-",                  2, 3, 7, 0},
  {"o",                  8,10,23, 0},
  {"ange",              38,99, 0, http_rq_range},
  {"iler",              43,99, 0, http_rq_trailer},
  {"odified-since",     27,99, 0, http_rq_if_modified_since},
  {"anguage",           15,99, 0, http_rq_content_language},
  {"eferer",            38,99, 0, http_rq_referer},
  {"encoding",          13,99, 0, http_rq_content_encoding},
  {"nsfer-encoding",    43,99, 0, http_rq_transfer_encoding},
  {"ength",             15,99, 0, http_rq_content_length},
  {"n",                 10,11,25, 0},
  {"okie",              10,99, 0, http_rq_cookie},
  {"nection",           11,99, 0, http_rq_connection},
  {"oxy-authorization", 35,99, 0, http_rq_proxy_authorization},
  {"l",                 13,15,31, 0},
  {"md5",               13,99, 0, http_rq_content_md5},
  {"m",                 26,27,16, 0},
  {"none-match",        26,99, 0, http_rq_if_none_match},
  {"tent-",             11,13,29, 0},
  {"ocation",           15,99, 0, http_rq_content_location},
  {"pgrade",            46,99, 0, http_rq_upgrade},
  {"range",             26,99, 0, http_rq_if_range},
  {"type",              13,99, 0, http_rq_content_type},
  {"ser-agent",         46,99, 0, http_rq_user_agent},
  {"unmodified-since",  26,99, 0, http_rq_if_unmodified_since},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0},
  {NULL,                 0, 0, 0, 0}
};
static const patricia patinitial_headers = { "", 0, 0, 0, 0 };

// Methods: state 0, offset 0
// Protocol: state 10, offset 1
// Connection header values: state 20, offset 2
// Transfer-Encoding values: state 25, offset 3
// Expect header values: state 30, offset 0
                     //Check
static const         //  Next
patricia utility[] = //     Offset                     Offsets:
{                    //        Terminal                0123
  {0,                 0, 0, 0, 0},                  // a
  {0,                 0, 0, 0, 0},                  // ba
  {"CONNECT",         0,99, 0, http_connect},       // cba
  {"DELETE",          0,99, 0, http_delete},        // dcba
  {"close",          20,99, 0, http_connection_close},//dcb
  {"chunked",        25,99, 0, http_transfer_chunked},//edc
  {"GET",             0,99, 0, http_get},           // gfe
  {"HEAD",            0,99, 0, http_head},          // hgf
  {"HTTP",           10,99, 0, http_proto},         // ihg
  {0,                 0, 0, 0, 0},                  // jih
  {0,                 0, 0, 0, 0},                  // kji
  {0,                 0, 0, 0, 0},                  // lkj
  {"keep-alive",     20,99, 0, http_connection_keep},//mlk
  {0,                 0, 0, 0, 0},                  // nml
  {"OPTIONS",         0,99, 0, http_options},       // onm
  {"P",               0, 1, 2, 0},                  // pon
  {"OST",             1,99, 0, http_post},          // qpo
  {0,                 0, 0, 0, 0},                  // rqp
  {0,                 0, 0, 0, 0},                  // srq
  {"TRACE",           0,99, 0, http_trace},         // tsr
  {0,                 0, 0, 0, 0},                  // uts
  {0,                 0, 0, 0, 0},                  // vut
  {"UT",              1,99, 0, http_put},           // wvu
  {0,                 0, 0, 0, 0},                  // xwv
  {0,                 0, 0, 0, 0},                  // yxw
  {0,                 0, 0, 0, 0},                  // zyx
  {"100-continue",   30,99, 0, http_expect_continue},//?zyx
  {0,                 0, 0, 0, 0},                  //  ?zy
  {0,                 0, 0, 0, 0},                  //   ?z
  {0,                 0, 0, 0, 0},                  //    ?
};

static const patricia patinitial_connection = { "", 0, 20, 2, 0 };
static const patricia patinitial_transfer = { "", 0, 25, 3, 0 };
static const patricia patinitial_expect = { "", 0, 30, 0, 0 };

typedef enum
{
  h_initial,
  h_method,
  h_method_sp,
  h_path, h_query, h_path_sp,
  h_prot, h_prot_slash,
  h_prot_http_x, h_prot_http_x_dot, h_prot_http_x_x,
  h_cr, h_nl, h_newline, h_skipline,
  h_header, h_header_unknown, h_escaped_1, h_escaped_2,
  h_value_lead, h_cont,
  h_value,
  h_head_connection,
  h_head_expect,
  h_head_transfer_encoding,
  h_head_flag,
  h_error_not_implemented,
  h_error_bad_request,
  h_error_internal_server_error
}
http_request_state;

typedef enum
{
  hrf_collect_header = 0x1
}
http_request_flags;

typedef enum
{
  hm_get = 0,
  hm_delete,
  hm_head,
  hm_post,
  hm_put,
  hm_trace
}
http_request_method;

static unsigned
utility_trie_map (char c)
{
  if (c >= 'A' && c <= 'Z')
    return c-'A';
  else if (c >= 'a' && c <= 'z')
    return c-'a';
  else
    return 'Z'-'A'+1;
}
static unsigned
header_trie_map (char c)
{
  if (c >= 'a' && c <= 'z')
    return c-'a';
  else if (c == '-')
    return 'z'-'a'+1;
  else return 'z'-'a'+2;
}

static int
walk_trie(int c, unsigned offset_c,
          const patricia *tree, const patricia *initial,
          triestate *tstate)
{
  const patricia *p;
  int       pc;

  if (tstate->poff) {
    p = &tree[tstate->slot];
    pc = p->prefix[tstate->poff];
    if (!pc && !c) {
      return p->terminal;
    }
  } else {
    p = initial;
    pc = 0;
  }
  if (! pc) {
    int slot = p->offset + offset_c;
    if (tree[slot].prefix
        && tree[slot].prefix[0] == c
        && tree[slot].check == p->next)
    {
      tstate->poff = 1;
      tstate->slot = slot;
    }
    else
    {
      return -1;
    }
  }
  else if (pc == c) {
    ++ tstate->poff;
  } else {
    return -1;
  }
  return 0;
}

                /* RFC 2396, Appendix A */
static inline int
uri_hex(int c)
{
  return ((c >= 'a' && c <= 'f')
        ||(c >= 'A' && c <= 'F')
        ||(c >= '0' && c <= '9'));
}
static inline int
uri_hex_conv(int c)
{
  if (c >= 'a' && c <= 'f')
    return c + 10 - 'a';
  else if (c >= 'A' && c <= 'F')
    return c + 10 - 'a';
  else
    return c - '0';
}
static inline int
uri_alphanum(int c)
{
  return ((c >= 'a' && c <= 'z')
        ||(c >= 'A' && c <= 'Z')
        ||(c >= '0' && c <= '9'));
}
static inline int
uri_mark(int c)
{
  return ((c == '-')||(c == '_')||(c == '.')
        ||(c == '!')||(c == '~')
        ||(c == '*')||(c == '\'')
        ||(c == '(')||(c == ')'));
}
static inline int
uri_unreserved(int c)
{
  return (uri_alphanum(c) || uri_mark(c));
}
static inline int
uri_reserved(int c)
{
  return ((c == ';')||(c == '/')
        ||(c == '?')||(c == ':')
        ||(c == '@')||(c == '&')
        ||(c == '=')||(c == '+')
        ||(c == '$')||(c == ','));
}
static inline int
uri_pchar(int c)
{
  return (uri_unreserved(c)
        ||(c == ':')||(c == '@')||(c == '&')
        ||(c == '=')||(c == '+')
        ||(c == '$')||(c == ','));
}
static inline int
uri_uric(int c)
{
  return uri_unreserved(c) || uri_reserved(c);
}

void
minute_http_init (unsigned          hmask,
                  iobuf            *io,
                  textint          *text,
                  minute_http_rqs  *s)
{
  minute_http_rqs m = {
    0,          // flags
    {0,0},      // httpv
    {0,0},      // tries
    io->read,   // m
    0,          // esc
    h_initial,  // st
    h_initial,  // est
    0,          // nl
    hmask,      // hmask
    io,
    text
  };

  //let index zero be a nul byte; if strings have index zero, they're
  //invalid, if dereferenced they'll produce the empty string.
  ((char*)text->data)[0] = 0;
  text->text = 1;
  text->ints = text->size;
  *s = m;
}

unsigned
minute_http_read (minute_http_rq   *rq,
                  minute_http_rqs  *rqs)
{
  minute_http_rqs s = *rqs; // avoid aliasing penalties
  struct iobuf *io = s.io;

  char *buf = io->data;
  unsigned mask = io->mask;
  unsigned ioflags = io->flags;
  unsigned e = io->write;
  unsigned b = io->read;
  int c = 0;

  const patricia *patinitial_flag = NULL;
  triestate tstate = {s.tries[0], s.tries[1]};

# ifdef DEBUG_MINUTE_HTTP_READ
#   include <stdio.h>
#   define D(x) fprintf(stderr,x "\n")
# else
#   define D(x)
# endif

# define H_EOF (-1)
# define shift(x) do{D("shift("#x")");s.st=(x);}while(0)
# define reset(x) do{D("reset("#x")");s.st=(x);goto top;}while(0)
  D("start");
  while (1) {
    if (s.m == e) {
      if (ioflags & IOBUF_EOF) {
        c = H_EOF;
      } else {
        s.tries[0] = tstate.poff;
        s.tries[1] = tstate.slot;
        io->read = b;
        *rqs = s;
        return EAGAIN;
      }
    } else
      c = buf[s.m&mask];

    top: switch (s.st) {
      case h_initial:
        if (c == '\n' || c == '\r')
          break;
        tstate.poff = 0;
        tstate.slot = 0;
        s.esc = 0;
        reset (h_method);
      case h_method: {
        patricia initial = { "", 0, 0, 0, 0 };
        int r = walk_trie (c == ' ' ? 0 : c, utility_trie_map(c),
                           utility, &initial, &tstate);
        switch (r) {
          case http_get:
          case http_head:
          case http_post:
          case http_put:
          case http_options:
          case http_delete:
          case http_trace:
          case http_connect:
            rq->request_method = r;
            shift (h_method_sp);
            break;
          case -1:
            reset (h_error_bad_request);
          case 0:
            break;
          default:
            reset (h_error_internal_server_error);
        }
        // method name string is not needed.
        b = s.m;
      } break;
      case h_method_sp:
        if (c != ' ') {
          b = s.m;
          rq->path = s.text->text;
          reset (h_path);
        }
        break; // consume all spaces.
      case h_path:
        if (uri_pchar(c)
          ||(c == '/')
          ||(c == ';'))
        {
          /* RFC 2396, Appendix A */
          if (1 != minute_textint_putc(c, s.text))
            return 414;
          b = s.m;
        } else if (c == '%') {
          s.est = h_path;
          shift (h_escaped_1);
          b = s.m;
        } else if (c == '?') {
          if (1 != minute_textint_putc(0, s.text))
            return 414;
          b = s.m;
          rq->query = s.text->text;
          shift (h_query);
        } else if (c == ' ') {
          if (1 != minute_textint_putc(0, s.text))
            return 414;
          b = s.m;
          shift (h_path_sp);
        } else reset (h_error_bad_request);
        break;
      case h_escaped_1:
        if (! uri_hex(c))
          reset (h_error_bad_request);
        s.esc = uri_hex_conv(c);
        shift (h_escaped_2);
        break;
      case h_escaped_2: {
        char escaped;
        if (! uri_hex(c))
          reset (h_error_bad_request);
        escaped = (s.esc << 4) | uri_hex_conv(c);
        if (1 != minute_textint_putc(escaped, s.text))
          return 414;
        s.esc = 0;
        shift (s.est);
      } break;
      case h_query:
        if (uri_uric(c)) {
          /* RFC 2396, Appendix A */
          if (1 != minute_textint_putc(c, s.text))
            return 414;
          b = s.m;
        } else if (c == '%') {
          s.est = h_query;
          shift (h_escaped_1);
          b = s.m;
        } else if (c == ' ') {
          if (1 != minute_textint_putc(0, s.text))
            return 414;
          b = s.m;
          shift (h_path_sp);
        } else reset (h_error_bad_request);
        break;
      case h_path_sp:
        if (c == ' ')
          break;
        tstate.poff = 0;
        tstate.slot = 0;
        reset (h_prot);
        break;
      case h_prot: {
        patricia initial = { "", 0, 10, 1, 0 };
        int r = walk_trie (c == '/' ? 0 : c, utility_trie_map(c),
                           utility, &initial, &tstate);
        switch (r) {
          case http_proto:
            shift (h_prot_slash);
            break;
          case -1:
            return 505;
          case 0:
            break;
          default:
            reset (h_error_internal_server_error);
        }
        // 'HTTP' string is not needed.
        b = s.m;
      } break;
      case h_prot_slash:
        if (c >= '0' && c <= '9') {
          s.httpv[0] = s.httpv[0]*10 + (c - '0');
        } else reset (h_prot_http_x);
        break;
      case h_prot_http_x:
        if (c == '.') shift (h_prot_http_x_dot);
        else reset (h_error_bad_request);
        break;
      case h_prot_http_x_dot:
        if (c >= '0' && c <= '9')
          {
            s.httpv[1] = s.httpv[1]*10 + (c - '0');
          }
        else reset (h_prot_http_x_x);
        break;
      case h_prot_http_x_x:
        if (s.httpv[0] == 0) {
          if (s.httpv[1] == 9)
            rq->server_protocol = http_0_9;
        } else if (s.httpv[0] == 1) {
          if (s.httpv[1] == 0)
            rq->server_protocol = http_1_0;
          else if (s.httpv[1] == 1)
            rq->server_protocol = http_1_1;
        }

        if (rq->server_protocol == http_unknown_version)
          return 505; // version not supported.

        if (c == '\r') shift (h_cr);
        else if (c  == '\n') {
          ++s.nl;
          shift (h_nl);
        }
        else reset (h_error_bad_request);
        break;
      // accept both CRLFs and lone LFs as newline.
      case h_cr:
        if (c == '\n') {
          ++s.nl;
          shift (h_nl);
        } else
          reset (h_error_bad_request);
        break;
      case h_nl:
        b = s.m;
        if (c == '\r') { shift (h_cr); }
        else if (c == '\n') {
          ++s.nl;
          shift (h_nl);
        }
        else if (c == ' ' || c == '\t') {
          s.nl = 0;
          shift (h_cont);
        } else {
          s.nl = 0;
          b = s.m;
          tstate.poff = 0;
          tstate.slot = 0;
          reset (h_header);
        }

        break;
      case h_header: {
        int r;
        if (c >= 'A' && c <= 'Z')
          c = c-'A'+'a'; // convert these to lower case
        r = walk_trie (c == ':' ? 0 : c, header_trie_map(c),
                           headers, &patinitial_headers, &tstate);
        if (r < 0) {
          // TODO unknown header
          /*
          if (!(s.hmask & 1)) {
            //TODO write iobuf -> text
            reset (h_header_unknown)
          } else
          */
            reset (h_skipline);
        } else if (r > 0) {
          tstate.poff = 0;
          tstate.slot = 0;
          switch (r) {
            case http_rq_connection:
              s.est = h_head_connection;
              shift (h_value_lead);
              break;
            case http_rq_expect:
              s.est = h_head_expect;
              shift (h_value_lead);
              break;
            case http_rq_transfer_encoding:
              s.est = h_head_transfer_encoding;
              shift (h_value_lead);
              break;
            default: {
              unsigned mask = 1u<<r;
              if (s.hmask & mask) {
                if (1 != minute_textint_puti (r, s.text))
                  return 413;
                if (1 != minute_textint_puti (s.text->text, s.text))
                  return 413;
                s.flags |= hrf_collect_header;
                s.est = h_value;
                shift (h_value_lead);
              } else {
                s.flags &= ~hrf_collect_header;
                s.est = h_skipline;
                shift (h_skipline);
              }
            } break;
          }
        }
        // else keep going.
        // header name string is only needed if we're collecting unknown headers
        if (!(s.hmask & 1))
          b = s.m;
      } break;
      case h_head_connection:
        patinitial_flag = &patinitial_connection;
        reset(h_head_flag);
        break;
      case h_head_expect:
        patinitial_flag = &patinitial_expect;
        reset(h_head_flag);
        break;
      case h_head_transfer_encoding:
        patinitial_flag = &patinitial_transfer;
        reset(h_head_flag);
        break;
      case h_head_flag: {
        int r;
        if (c >= 'A' && c <= 'Z')
          c = c-'A'+'a';
        r = walk_trie ((c == '\r' || c == '\n') ?  0 : c, utility_trie_map(c),
                        utility, patinitial_flag, &tstate);
        if (r < 0) {
          // unknown value, skip it.
          reset (h_skipline);
        } else if (r > 0) {
          rq->flags |= r;
          b = s.m; // we will not be needing this value.
          reset (h_skipline);
        }
        // else, keep going.
      } break;
      case h_cont:
        // replace previous null-termination
        // with a single space and continue.
        if (s.flags & hrf_collect_header) {
          minute_textint_replacec(-1, ' ', s.text);
          reset (h_value_lead);
        } else
          reset (h_skipline);
        break;
      case h_value_lead:
        if ( c != ' ' && c != '\t' ) {
          b = s.m;
          reset (s.est);
        }
        break;
      case h_value:
        b = s.m;
        if (c == '\r') shift (h_cr);
        else if (c == '\n') { ++s.nl; shift (h_nl); }
        else if (c == H_EOF) { s.nl = 99; }
        else if (c == ' ' || c == '\t') {
          // turn all whitespace into a single space
          if (1 != minute_textint_putc(' ', s.text))
            return 413;
          // consume any following whitespace using the
          // value_lead state..
          s.est = s.st;
          shift (h_value_lead);
          break;
        } else {
          if (1 != minute_textint_putc(c, s.text))
            return 413;
          break;
        }
        // only reached if the last two elses didn't execute
        // (i.e. we get her on \r, \n, or eof.)
        if (1 != minute_textint_putc(0, s.text))
          return 413;
        break;
      case h_skipline:
        b = s.m;
        if (c == '\r') shift (h_cr);
        else if (c == '\n') { ++s.nl; shift (h_nl); }
        else if (c == H_EOF) { s.nl = 99; }
        break;
      case h_error_not_implemented:
        return 501;
      case h_error_bad_request:
        return 400;
      case h_error_internal_server_error:
      default: // internal server error
        return 500;
    }

    ++s.m;

    if (s.nl > 1) {
      // we're done, we don't need to backtrack anymore.
      io->read = s.m;
      *rqs = s;
      return 0;
    }
  }
# undef reset
# undef shift
# undef BUFMASK
# undef BUFSZ

  return 500;
}

