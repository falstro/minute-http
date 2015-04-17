#include "tap.h"
#include "config.h"
#include "main.h"

#include "libhttp/http.h"
#include "libhttp/http-headers.h"
#include "libhttp/textint.h"
#include "libhttpd/httpd.h"

#include <errno.h>
#include <strings.h>
#include <string.h>

typedef struct tap_runtime tap_runtime;
typedef struct tap_vhost tap_vhost;

//TODO intern strings...
static const char *s_head     = "head";
static const char *s_payload  = "payload";
static const char *s_response = "response";
static const char *s_tap_io   = "tap-io";
static const char *s_default  = "default";

typedef struct
tap_rq_data
{
  // set by handle()
  tap_runtime  *rs;
  int           listenId;

  // set by head()
  tap_vhost    *vhost;
  Tcl_Obj      *o_path;
  Tcl_Obj      *o_query;
  Tcl_Obj      *o_host;
  enum
  http_method   method;

  // set throughout
  Tcl_Obj      *status;
  int           code;
}
tap_rq_data;

typedef struct
tap_request_base
{
  minute_http_rq     *rq;
  textint            *text;
  tap_rq_data        *rqd;
}
tap_request_base;
typedef struct
tap_request_head
{
  tap_request_base    base;
  minute_httpd_head  *head;
}
tap_request_head;

typedef struct
tap_request_resp
{
  tap_request_base    base;
}
tap_request_resp;

static int
minuted_tap_close_proc   (ClientData  instanceData,
                          Tcl_Interp *tcl)
{
  minute_httpd_out *out = instanceData;
  out->flush(out);
  return 0;
}

static int
minuted_tap_inputclose_proc  (ClientData  instanceData,
                              Tcl_Interp *tcl)
{
  return 0;
}

static int
minuted_tap_input_proc   (ClientData  instanceData,
                          char       *buf,
                          int         toWrite,
                          int        *errorCodePtr)
{
  //TODO implement reading request payload
  return 0;
}

static int
minuted_tap_output_proc  (ClientData  instanceData,
                          const char *buf,
                          int         toWrite,
                          int        *errorCodePtr)
{
  minute_httpd_out *out = instanceData;
  return out->write(buf, toWrite, out);
}

static int
minuted_tap_no_out_proc  (ClientData  instanceData,
                          const char *buf,
                          int         toWrite,
                          int        *errorCodePtr)
{
  *errorCodePtr = EACCES;
  return -1;
}

static void
minuted_tap_watch_proc   (ClientData  instanceData,
                          int         mask)
{
}

static int
minuted_tap_handle_proc  (ClientData  instanceData,
                          int         direction,
                          ClientData *handlePtr)
{
  return EINVAL;
}

static
Tcl_ChannelType
minuted_tap_inout_channel = {
  "minuted-tap-inout",
  TCL_CHANNEL_VERSION_2,
  minuted_tap_close_proc, // Tcl_DriverCloseProc *closeProc;
  minuted_tap_input_proc, // Tcl_DriverInputProc *inputProc;
  minuted_tap_output_proc,// Tcl_DriverOutputProc *outputProc;
  NULL,                   // Tcl_DriverSeekProc *seekProc;              X
  NULL,                   // Tcl_DriverSetOptionProc *setOptionProc;    X
  NULL,                   // Tcl_DriverGetOptionProc *getOptionProc;    X
  minuted_tap_watch_proc, // Tcl_DriverWatchProc *watchProc;
  minuted_tap_handle_proc,// Tcl_DriverGetHandleProc *getHandleProc;
  NULL,                   // Tcl_DriverClose2Proc *close2Proc;          X
  NULL,                   // Tcl_DriverBlockModeProc *blockModeProc;    X
  NULL,                   // Tcl_DriverFlushProc *flushProc;            O
  NULL,                   // Tcl_DriverHandlerProc *handlerProc;        O
  NULL,                   // Tcl_DriverWideSeekProc *wideSeekProc;      X
  NULL,                   // Tcl_DriverThreadActionProc *threadActionProc; O
  NULL                    // Tcl_DriverTruncateProc *truncateProc;      O
};

static
Tcl_ChannelType
minuted_tap_input_channel = {
  "minuted-tap-input",
  TCL_CHANNEL_VERSION_2,
  minuted_tap_inputclose_proc, // Tcl_DriverCloseProc *closeProc;
  minuted_tap_input_proc, // Tcl_DriverInputProc *inputProc;
  minuted_tap_no_out_proc,// Tcl_DriverOutputProc *outputProc;
  NULL,                   // Tcl_DriverSeekProc *seekProc;              X
  NULL,                   // Tcl_DriverSetOptionProc *setOptionProc;    X
  NULL,                   // Tcl_DriverGetOptionProc *getOptionProc;    X
  minuted_tap_watch_proc, // Tcl_DriverWatchProc *watchProc;
  minuted_tap_handle_proc,// Tcl_DriverGetHandleProc *getHandleProc;
  NULL,                   // Tcl_DriverClose2Proc *close2Proc;          X
  NULL,                   // Tcl_DriverBlockModeProc *blockModeProc;    X
  NULL,                   // Tcl_DriverFlushProc *flushProc;            O
  NULL,                   // Tcl_DriverHandlerProc *handlerProc;        O
  NULL,                   // Tcl_DriverWideSeekProc *wideSeekProc;      X
  NULL,                   // Tcl_DriverThreadActionProc *threadActionProc; O
  NULL                    // Tcl_DriverTruncateProc *truncateProc;      O
};

static int
minuted_tap_binary_search(const char *s, const char *ls[], int sz)
{
  int lo = 0,
      hi = sz;

  while(lo < hi) {
    int mid = (hi+lo)/2;
    int diff = strcasecmp(s, ls[mid]);
    if (diff == 0)
      return  mid;
    else if (diff < 0)
      hi = mid;
    else
      lo = mid+1;
  }

  return -1;
}

static enum http_response_header
minuted_tap_response_header(const char *name)
{
  int h = minuted_tap_binary_search(name, http_response_header_names,
    http_response_header_names_count);
  return h > 0 ? (enum http_response_header) h : http_rsp_unknown_header;
}

static enum http_request_header
minuted_tap_request_header(const char *name)
{
  int h = minuted_tap_binary_search(name, http_request_header_names,
    http_request_header_names_count);
  return h > 0 ? (enum http_request_header) h : http_rq_unknown_header;
}

/* reset rqd for next request. */
static void
minuted_tap_reset (tap_rq_data *rqd)
{
  rqd->vhost = NULL;

  rqd->method = http_unknown_method;
  rqd->code   = 0;

  Tcl_Obj **refs[] = {
    &rqd->status,
    &rqd->o_path,
    &rqd->o_query,
    &rqd->o_host
  };
  for(int i = 0; i < sizeof(refs)/sizeof(refs[0]); ++i)
    if(*refs[i]) {
      Tcl_DecrRefCount(*refs[i]);
      *refs[i] = NULL;
    }
}

static unsigned
minuted_tap_status (tap_rq_data *rqd)
{
  tap_vhost *v = rqd->vhost;
  Tcl_Obj *resObj, *statusObj;
  int resultLen;
  int res;

  if(!(resObj = Tcl_GetObjResult(v->tcl))) {
    error("Status processing yielded no result.");
    res = 500;
  } else if(Tcl_ListObjLength(v->tcl, resObj, &resultLen) != TCL_OK) {
    error("Status processing yielded non-list result.");
    res = 500;
  } else if(Tcl_ListObjIndex(v->tcl, resObj, 0, &statusObj) != TCL_OK) {
    error("Status unable to get status object.");
    res = 500;
  } else if(Tcl_GetIntFromObj(v->tcl, statusObj, &res) != TCL_OK) {
    error("Status processing yielded non-integer status.");
    res = 500;
  }

  Tcl_Obj *old = rqd->status;
  if((rqd->status = resObj))
    Tcl_IncrRefCount(rqd->status);
  if(old)
    Tcl_DecrRefCount(old);

  rqd->code = res;
  return res;
}

static int
tap_tcl_add_header   (tap_request_head *trq,
                      Tcl_Interp       *tcl,
                      Tcl_Obj          *header,
                      Tcl_Obj          *value)
{
  enum http_response_header h;

  h = minuted_tap_response_header(Tcl_GetString(header));

  switch(h) {
    case http_rsp_unknown_header:
      Tcl_AddErrorInfo(tcl, "unknown header");
      return TCL_ERROR;
    // TODO handle timestamps.
    default:
      trq->head->string(h, Tcl_GetString(value), trq->head);
  }

  return TCL_OK;
}
static int
tap_tcl_get_header   (tap_request_base *trq,
                      Tcl_Interp       *tcl,
                      Tcl_Obj          *header)
{
  enum http_request_header h;

  h = minuted_tap_request_header(Tcl_GetString(header));

  switch(h) {
    case http_rq_unknown_header:
      Tcl_AddErrorInfo(tcl, "unknown header");
      return TCL_ERROR;

    case http_rq_content_length:
      if(trq->rq->flags & http_content_length)
        Tcl_SetObjResult(tcl, Tcl_NewIntObj(trq->rq->content_length));
      break;
    case http_rq_transfer_encoding:
      if(trq->rq->flags & http_transfer_chunked)
        Tcl_SetResult(tcl, "chunked", TCL_STATIC);
      break;
    default:
    {
      textint *text = trq->text;
      int i, ints = minute_textint_intsize(text);
      char *value;
      for(i = 0; i < ints; i += 2)
        if(minute_textint_geti(i, text) == h) {
          value = minute_textint_gets(minute_textint_geti(i+1, text), text);
          Tcl_SetResult(tcl, value, TCL_STATIC);
          break;
        }
    }
  }

  return TCL_OK;
}
static int
tap_tcl_headers_meta (ClientData  clientData,
                      Tcl_Interp *tcl,
                      int         objc,
                      Tcl_Obj    *const objv[])
{
  static const char *cmds[] = {
    "add-header",
    "get-header"
  };
  tap_request_head *trq = clientData;
  if(objc < 2) {
    Tcl_WrongNumArgs(tcl, 1, objv, "command ?args?");
    return TCL_ERROR;
  }
  Tcl_Obj *cmd = objv[1];
  int cmdno = minuted_tap_binary_search(Tcl_GetString(cmd),
    cmds, sizeof(cmds)/sizeof(cmds[0]));
  switch(cmdno) {
    default:
    case -1:
      break;
    case 0: { // add-header
      if (objc != 4) {
        Tcl_WrongNumArgs(tcl, 2, objv, "header-name value");
        return TCL_ERROR;
      }
      return tap_tcl_add_header(trq, tcl, objv[2], objv[3]);
    } break;
    case 1: { // get-header
      if (objc != 3) {
        Tcl_WrongNumArgs(tcl, 2, objv, "header-name");
        return TCL_ERROR;
      }
      return tap_tcl_get_header(&trq->base, tcl, objv[2]);
    } break;
  }
  return TCL_OK;
}

static int
tap_tcl_response_meta(ClientData  clientData,
                      Tcl_Interp *tcl,
                      int         objc,
                      Tcl_Obj    *const objv[])
{
  static const char *cmds[] = {
    "get-header"
  };
  tap_request_resp *trq = clientData;
  if(objc < 2) {
    Tcl_WrongNumArgs(tcl, 1, objv, "command ?args?");
    return TCL_ERROR;
  }
  Tcl_Obj *cmd = objv[1];
  int cmdno = minuted_tap_binary_search(Tcl_GetString(cmd),
    cmds, sizeof(cmds)/sizeof(cmds[0]));
  switch(cmdno) {
    default:
    case -1:
      break;
    case 0: { // get-header
      if (objc != 3) {
        Tcl_WrongNumArgs(tcl, 2, objv, "header-name");
        return TCL_ERROR;
      }
      return tap_tcl_get_header(&trq->base, tcl, objv[2]);
    } break;
  }
  return TCL_OK;
}

static Tcl_Command
tap_create_meta  (Tcl_Interp     *tcl,
                  Tcl_ObjCmdProc *proc,
                  ClientData      cd,
                  Tcl_Obj       **name)
{
  char cmd[16];
  static unsigned cmdno = 0;
  *name = Tcl_NewStringObj(cmd, snprintf(cmd, sizeof(cmd), "mm%u", ++cmdno));
  return Tcl_CreateObjCommand(tcl, cmd, proc, cd, NULL);
}

static unsigned
minuted_tap_head (minute_http_rq     *rq,
                  minute_httpd_head  *head,
                  textint            *text,
                  void               *rsvoid)
{
  tap_rq_data *rqd = rsvoid;
  tap_runtime *rs = rqd->rs;

  Tcl_Interp *tcl = rs->tcl;
  int r,i;
  int res = 500;

  const char *path = rq->path ? minute_textint_gets(rq->path, text) : "";
  const char *query = rq->query ? minute_textint_gets(rq->query, text) : "";

  const char *host_header = s_default;

  int ints = minute_textint_intsize(text);
  for(i = 0; i < ints; i += 2)
    switch(minute_textint_geti(i, text)) {
      // look for host header to determine vhost.
      case http_rq_host: {
        host_header = minute_textint_gets(
          minute_textint_geti(i+1, text), text);
      }
    }

  Tcl_Obj *vhost, *id, *acthost;

  Tcl_Obj *vhosts = rs->vhostListen[rqd->listenId];

  Tcl_Obj *host = Tcl_NewStringObj(host_header, -1);
  Tcl_Obj *defhost = Tcl_NewStringObj(s_default, -1);

  Tcl_IncrRefCount(host);
  Tcl_IncrRefCount(defhost);
  if((acthost = host) &&
      (r = Tcl_DictObjGet(tcl, vhosts, host, &vhost)) != TCL_OK) {
    error("Internal vhostListen failure");
  } else if(!vhost && (acthost = defhost) &&
      (r = Tcl_DictObjGet(tcl, vhosts, defhost, &vhost)) != TCL_OK) {
    error("Internal default vhostListen failure");
  } else if ((r = Tcl_DictObjGet(tcl, rs->vhostMap, acthost, &id)) != TCL_OK) {
    error("Internal vhostMap failure");
  }
  Tcl_IncrRefCount(rqd->o_host = acthost);
  Tcl_DecrRefCount(defhost);
  Tcl_DecrRefCount(host);

  if(r != TCL_OK) {
    res = 500;
  } else if(!vhost) {
    //no such vhost
    info("Attempted to access unknown vhost");
    res = http_gone;
  } else if(!id) {
    // id should always be present at this point.
    error("Internal vhost id failure");
    res = 500;
  } else if(Tcl_GetIntFromObj(tcl, id, &i) != TCL_OK) {
    error("Internal vhost id not an int");
    res = 500;
  } else if(i < 0 || i > rs->nv) {
    error("Internal vhost id out of range");
    res = 500;
  } else {
    tap_vhost *v = rqd->vhost = &rs->v[i];

    //TODO interned strings.
    Tcl_Obj *o_proc = Tcl_NewStringObj(s_head, -1);
    Tcl_IncrRefCount(rqd->o_path = Tcl_NewStringObj(path, -1));
    Tcl_IncrRefCount(rqd->o_query = Tcl_NewStringObj(query, -1));
    Tcl_Obj *o_meta;

    tap_request_head trq = {{rq, text, rqd}, head};
    Tcl_Command meta = tap_create_meta(v->tcl, tap_tcl_headers_meta, &trq,
      &o_meta);

    Tcl_Obj *objv[] = {
      o_proc, rqd->o_path, rqd->o_query, o_meta
    };
    int objc = sizeof(objv)/sizeof(objv[0]);

    for(i = 0; i < objc; ++i)
      Tcl_IncrRefCount(objv[i]);

    r = (v->headers.objProc)(v->headers.objClientData, v->tcl, objc, objv);

    for(i = 0; i < objc; ++i)
      Tcl_DecrRefCount(objv[i]);

    Tcl_DeleteCommandFromToken(v->tcl, meta);

    if(r != TCL_OK) {
      error("Head processing failed: %s", Tcl_GetStringResult(v->tcl));
      res = 500;
    } else {
      res = minuted_tap_status(rqd);
    }
  }

  rqd->method = rq->request_method;

  return res;
}

static unsigned
minuted_tap_payload  (minute_http_rq     *rq,
                      minute_httpd_head  *head,
                      minute_httpd_in    *in,
                      textint            *text,
                      void               *rsvoid)
{
  int r,i;
  tap_rq_data *rqd   = rsvoid;
  tap_vhost   *v     = rqd->vhost;

  if(v->flags & TAP_NO_PAYLOAD)
    return 500;

  //TODO generate channel name.
  Tcl_Channel channel = Tcl_CreateChannel(
    &minuted_tap_input_channel, s_tap_io,
    in, TCL_READABLE
  );

  Tcl_Obj *o_proc = Tcl_NewStringObj(s_payload, -1);
  Tcl_Obj *o_channel = Tcl_NewStringObj(s_tap_io, -1);
  Tcl_Obj *o_meta;

  tap_request_head trq = {{rq, text, rqd}, head};
  Tcl_Command meta = tap_create_meta(v->tcl, tap_tcl_headers_meta, &trq,
    &o_meta);

  Tcl_Obj *objv[] = {
    o_proc, rqd->o_path, rqd->o_query,
    o_meta, o_channel, rqd->status
  };
  int objc = sizeof(objv)/sizeof(objv[0]);
  for(i = 0; i < objc; ++i)
    Tcl_IncrRefCount(objv[i]);
  Tcl_RegisterChannel(v->tcl, channel);

  r = (v->payload.objProc)(v->payload.objClientData, v->tcl, objc, objv);

  Tcl_UnregisterChannel(v->tcl, channel);
  for(i = 0; i < objc; ++i)
    Tcl_DecrRefCount(objv[i]);

  Tcl_DeleteCommandFromToken(v->tcl, meta);

  if(r != TCL_OK) {
    error("Payload processing failed: %s", Tcl_GetStringResult(v->tcl));
    return 500;
  }

  return minuted_tap_status(rqd);
}

static unsigned
minuted_tap_response (minute_http_rq   *rq,
                      minute_httpd_out *out,
                      textint          *text,
                      unsigned          status,
                      void             *rsvoid)
{
  int i,r;
  tap_rq_data *rqd   = rsvoid;
  tap_vhost   *v     = rqd->vhost;

  if (!v)
    return 1;

  //TODO generate channel name.
  //TODO read/write channel in response?
  Tcl_Channel channel = Tcl_CreateChannel(
    &minuted_tap_inout_channel, s_tap_io,
    out, TCL_WRITABLE
  );

  Tcl_Obj *o_proc = Tcl_NewStringObj(s_response, -1);
  Tcl_Obj *o_channel = Tcl_NewStringObj(s_tap_io, -1);
  Tcl_Obj *o_meta = Tcl_NewObj();
  tap_request_resp trq = {{rq, text, rqd}};

  Tcl_Command meta = tap_create_meta(v->tcl, tap_tcl_response_meta, &trq,
    &o_meta);

  Tcl_Obj *objv[] = {
    o_proc, rqd->o_path, rqd->o_query, o_meta, o_channel, rqd->status
  };
  int objc = sizeof(objv)/sizeof(objv[0]);
  for(i = 0; i < objc; ++i)
    Tcl_IncrRefCount(objv[i]);
  Tcl_RegisterChannel(v->tcl, channel);

  r = (v->response.objProc)(v->response.objClientData, v->tcl, objc, objv);

  Tcl_UnregisterChannel(v->tcl, channel);
  for(i = 0; i < objc; ++i)
    Tcl_DecrRefCount(objv[i]);

  Tcl_DeleteCommandFromToken(v->tcl, meta);

  if(r != TCL_OK) {
    error("Response processing failed: %s", Tcl_GetStringResult(v->tcl));
    return 1;
  }

  return 0;
}
static void
minuted_tap_error  (minute_http_rq   *rq,
                    unsigned          status,
                    void             *rsvoid)
{
  error("Request parsing failed: %d", status);
}

Tcl_Interp*
minuted_tap_create (Tcl_Interp *tcl,
                    Tcl_Obj    *name)
{
  Tcl_Interp *s = Tcl_CreateSlave(tcl, Tcl_GetString(name), 0);

  //TODO alias functions into slave interpreter, e.g. log
  return s;
}

unsigned
minuted_tap_handle (int           sock,
                    int           listenId,
                    tap_runtime  *tr)
{
  minute_httpd_app app = {
    minuted_tap_head,
    minuted_tap_payload,
    minuted_tap_response,
    minuted_tap_error
  };
  tap_rq_data rqd = {tr, listenId};

  minute_httpd_state state;

  char inbuf[0x100];
  char outbuf[0x400];
  char textbuf[0x400];
  int r;

  minute_httpd_init (sock, sock,
    minute_iobuf_init(sizeof(inbuf), inbuf),
    minute_iobuf_init(sizeof(outbuf), outbuf),
    minute_textint_init(sizeof(textbuf), textbuf),
    &state);

  // should be superfluous, but just in case something shouldn't be zero,
  // do a proper initial reset.
  minuted_tap_reset (&rqd);


  do {
    r = minute_httpd_handle(&app,&state,&rqd);

    //TODO move this to proper logging.
    const char *method;
    switch(r) {
      case httpd_client_ok_open:
      case httpd_client_ok_close: {
        const char *query = Tcl_GetString(rqd.o_query);
        switch(rqd.method) {
          default:
          case http_unknown_method: method = "???"; break;
          case http_get:      method = "GET";     break;
          case http_post:     method = "POST";    break;
          case http_put:      method = "PUT";     break;
          case http_delete:   method = "DELETE";  break;
          case http_head:     method = "HEAD";    break;
          case http_options:  method = "OPTIONS"; break;
          case http_trace:    method = "TRACE";   break;
          case http_connect:  method = "CONNECT"; break;
        }
        acclog("%s %s %s%s%s %d",
          Tcl_GetString(rqd.o_host), method,
          Tcl_GetString(rqd.o_path),
          *query?"?":"", query,
          rqd.code);
      }
      break;
    }

    minuted_tap_reset (&rqd);
  } while(r == httpd_client_ok_open);

  return r;
}
