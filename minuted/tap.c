#include "tap.h"
#include "config.h"
#include "main.h"

#include "libhttp/http.h"
#include "libhttp/http-headers.h"
#include "libhttp/textint.h"
#include "libhttpd/httpd.h"

#include <errno.h>
#include <strings.h>

typedef struct tap_runtime tap_runtime;
typedef struct tap_vhost tap_vhost;

//TODO intern strings...
static const char *s_head = "head";
static const char *s_payload = "payload";
static const char *s_tap_io = "tap-io";
static const char *s_default = "default";

typedef struct
tap_rq_data
{
  // set by handle()
  tap_runtime  *rs;
  int           listenId;

  // set by head()
  tap_vhost    *vhost;
  Tcl_Obj      *status;
}
tap_rq_data;

static int
minuted_tap_close_proc   (ClientData  instanceData,
                          Tcl_Interp *tcl)
{
  minute_httpd_out *out = instanceData;
  out->flush(out);
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


Tcl_ChannelType minuted_tap_inout_channel = {
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

//TODO Unused, we should allow head function to read the payload, but not write.
//Currently the header may not read at all.
Tcl_ChannelType minuted_tap_input_channel = {
  "minuted-tap-input",
  TCL_CHANNEL_VERSION_2,
  minuted_tap_close_proc, // Tcl_DriverCloseProc *closeProc;
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

  // if connection is kept alive and the status from previous call has not
  // been discarded yet; drop it here.
  if(rqd->status != NULL) {
    Tcl_DecrRefCount(rqd->status);
    rqd->status = NULL;
  }

  const char *path = rq->path ? minute_textint_gets(rq->path, text) : "";
  const char *query = rq->query ? minute_textint_gets(rq->query, text) : "";

  const char *host_header = s_default;

  int ints = minute_textint_intsize(text);
  for(i = 0; i < ints; i += 2)
    switch(minute_textint_geti(i, text)) {
      case http_rq_host: {
        host_header = minute_textint_gets(
          minute_textint_geti(i+1, text), text);
      }
    }

  const char *method;
  switch(rq->request_method) {
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
  Tcl_IncrRefCount(acthost);
  Tcl_DecrRefCount(defhost);
  Tcl_DecrRefCount(host);

  if(r != TCL_OK) {
    res = 500;
  } else if(!vhost) {
    //no such vhost
    info("Attempted to access unknown vhost");
    res = 410; // Gone.
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
    int resultLen;
    tap_vhost *v = rqd->vhost = &rs->v[i];

    //TODO interned strings
    Tcl_Obj *o_proc = Tcl_NewStringObj(s_head, -1);
    Tcl_Obj *o_path = Tcl_NewStringObj(path, -1);
    Tcl_Obj *o_query = Tcl_NewStringObj(query, -1);

    Tcl_Obj *objv[] = {o_proc, o_path, o_query};
    int objc = sizeof(objv)/sizeof(objv[0]);

    Tcl_Obj *resObj = NULL;
    Tcl_Obj *statusObj;

    for(i = 0; i < objc; ++i)
      Tcl_IncrRefCount(objv[i]);

    r = (v->head.objProc)(v->head.objClientData, v->tcl, objc, objv);
    if(r != TCL_OK) {
      error("HEAD processing failed: %s", Tcl_GetStringResult(v->tcl));
      res = 500;
    } else if(!(resObj = Tcl_GetObjResult(v->tcl))) {
      error("HEAD processing yielded no result.");
      res = 500;
    } else if(Tcl_ListObjLength(v->tcl, resObj, &resultLen) != TCL_OK) {
      error("HEAD processing yielded non-list result.");
      res = 500;
    } else if(Tcl_ListObjIndex(v->tcl, resObj, 0, &statusObj) != TCL_OK) {
      error("HEAD unable to get status object.");
      res = 500;
    } else if(Tcl_GetIntFromObj(v->tcl, statusObj, &res) != TCL_OK) {
      error("HEAD processing yielded non-integer status.");
      res = 500;
    } else {
      if(resultLen > 1) {
        Tcl_Obj *hds;
        Tcl_Obj *k, *val;
        Tcl_DictSearch ds;
        int done;
        if(Tcl_ListObjIndex(v->tcl, resObj, 1, &hds) != TCL_OK) {
          error("HEAD unable to get headers object.");
          res = 500;
        } else if(Tcl_DictObjFirst(v->tcl,hds,&ds,&k,&val,&done) != TCL_OK) {
          error("HEAD header object not a dict");
          res = 500;
        } else for(; !done; Tcl_DictObjNext(&ds,&k,&val,&done)) {
          const char *name = Tcl_GetString(k);
          const char *value = Tcl_GetString(val);
          enum http_response_header header =
            minuted_tap_response_header(name);

          if(header == http_rsp_unknown_header) {
            warn("Application header unrecognized: %s: %s", name, value);
          } else {
            //TODO handle timestamps (head->timestamp()).
            head->string(header, value, head);
          }
        }
      }
    }

    if(!(rqd->status = resObj))
      rqd->status = Tcl_NewIntObj(res);
    Tcl_IncrRefCount(rqd->status);
    for(i = 0; i < objc; ++i)
      Tcl_DecrRefCount(objv[i]);
  }

  //TODO move this to proper logging.
  acclog("%s %s %s%s%s %d",
    Tcl_GetString(acthost), method, path, *query?"?":"", query, res);
  Tcl_DecrRefCount(acthost);

  return res;
}

static unsigned
minuted_tap_payload  (minute_http_rq   *rq,
                      minute_httpd_out *out,
                      textint          *text,
                      unsigned          status,
                      void             *rsvoid)
{
  int i,r;
  tap_rq_data *rqd   = rsvoid;
  tap_vhost   *v     = rqd->vhost;

  const char *path = minute_textint_gets(rq->path, text);
  const char *query = minute_textint_gets(rq->query, text);

  if (!v)
    return 1;

  //TODO Should be TCL_READABLE too eventually, for reading the request body.
  Tcl_Channel channel = Tcl_CreateChannel(
    &minuted_tap_inout_channel, s_tap_io,
    out, TCL_WRITABLE
  );

  //TODO no need to recreate these here, keep them from the head processing.
  Tcl_Obj *o_proc = Tcl_NewStringObj(s_payload, -1);
  Tcl_Obj *o_path = Tcl_NewStringObj(path, -1);
  Tcl_Obj *o_query = Tcl_NewStringObj(query, -1);
  Tcl_Obj *o_status = rqd->status;
  Tcl_Obj *o_channel = Tcl_NewStringObj(s_tap_io, -1);

  Tcl_Obj *objv[] = {o_proc, o_path, o_query, o_status, o_channel};
  int objc = sizeof(objv)/sizeof(objv[0]);
  for(i = 0; i < objc; ++i)
    Tcl_IncrRefCount(objv[i]);

  Tcl_RegisterChannel(v->tcl, channel);
  r = (v->payload.objProc)(v->payload.objClientData, v->tcl, objc, objv);
  Tcl_UnregisterChannel(v->tcl, channel);

  for(i = 0; i < objc; ++i)
    Tcl_DecrRefCount(objv[i]);

  if(r != TCL_OK) {
    error("Payload processing failed: %s", Tcl_GetStringResult(v->tcl));
    return 1;
  }

  return 0;
}
static void
minuted_tap_error  (minute_http_rq   *rq,
                    unsigned          status,
                    void             *rsvoid)
{
  // tap_rq_data *rqd   = rsvoid;
  // tap_vhost   *v     = rqd->vhost;
  error("Request parsing failed: %d", status);
}

unsigned
minuted_tap_handle (int           sock,
                    int           listenId,
                    tap_runtime  *tr)
{
  minute_httpd_app app = {
    minuted_tap_head,
    minuted_tap_payload,
    minuted_tap_error
  };
  tap_rq_data rqd = {tr};
  unsigned r = minute_httpd_handle(sock, sock, &app, &rqd);

  if(rqd.status)
    Tcl_DecrRefCount(rqd.status);

  return r;
}
