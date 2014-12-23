#include "config.h"
#include "main.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <assert.h>

enum config_state_string_names
{
  cs__errorinfo,
  cs_application,
  cs_eval,
  cs_namespace,
  cs_source,
  cs_nsMinuted,
  cs_nsMinutedVhost,
  cs_COUNT
};

struct
configure_state
{
  Tcl_Interp *tcl;

  /* interned strings */
  Tcl_Obj  *string[cs_COUNT];
  Tcl_Obj  *current;

  /* parser 'global' state - careful, single thread only */
  configuration *conf;
};

static int
vhost_tcl_application  (ClientData  clientData,
                        Tcl_Interp *tcl,
                        int         objc,
                        Tcl_Obj    *const objv[])
{
  if(objc != 2) {
    Tcl_WrongNumArgs(tcl, 1, objv, "file-name");
    return TCL_ERROR;
  }

  configure_state *cs = clientData;

  Tcl_Obj* file = objv[1];
  if(Tcl_FSAccess(file, R_OK)) {
    Tcl_AppendObjToErrorInfo(tcl, file);
    Tcl_AddErrorInfo(tcl, ": file not found or insufficient privileges.");
    return TCL_ERROR;
  }

  Tcl_DictObjPut(tcl, cs->current, cs->string[cs_application], file);

  return TCL_OK;
}

static int
minuted_tcl_vhost  (ClientData  clientData,
                    Tcl_Interp *tcl,
                    int         objc,
                    Tcl_Obj    *const objv[])
{
  int r;
  if(objc != 3) {
    Tcl_WrongNumArgs(tcl, 1, objv, "name body");
    return TCL_ERROR;
  }

  configure_state *cs = clientData;
  Tcl_Obj *name = objv[1];
  Tcl_Obj *body = objv[2];

  Tcl_Obj *exists;
  if((r = Tcl_DictObjGet(tcl, cs->conf->vhosts, name, &exists)) != TCL_OK)
    return r;
  if(exists) {
      Tcl_AppendObjToErrorInfo(tcl, name);
      Tcl_AddErrorInfo(tcl, ": vhost already exists");
      return TCL_ERROR;
  }

  Tcl_Obj *eval[] = {
    cs->string[cs_namespace],
    cs->string[cs_eval],
    cs->string[cs_nsMinutedVhost],
    body
  };

  Tcl_IncrRefCount(body);

  int neval = sizeof(eval)/sizeof(eval[0]);

  //probably null, but perhaps not in the future.
  Tcl_Obj *pcur = cs->current;

  cs->current = Tcl_NewDictObj();
  Tcl_IncrRefCount(cs->current);

  r = Tcl_EvalObjv(tcl, neval, eval, TCL_EVAL_GLOBAL);

  Tcl_DecrRefCount(body);

  if(r == TCL_OK)
  {
    Tcl_DictObjPut(tcl, cs->conf->vhosts, name, cs->current);
  }
  Tcl_DecrRefCount(cs->current);

  cs->current = pcur;
  return r;
}

static int
minuted_tcl_listen (ClientData  clientData,
                    Tcl_Interp *tcl,
                    int         objc,
                    Tcl_Obj    *const objv[])
{
  int i, ni, r;
  configure_state *cs = clientData;

  if(objc != 4) {
    Tcl_WrongNumArgs(tcl, 1, objv, "bind-address port vhosts");
    return TCL_ERROR;
  }

  Tcl_Obj *vhosts = objv[3];
  Tcl_Obj *key = Tcl_NewListObj(2, objv+1);

  Tcl_IncrRefCount(key);

  Tcl_Obj *vhostdict;
  if((r = Tcl_DictObjGet(tcl, cs->conf->listen, key, &vhostdict)) != TCL_OK)
    return r;

  if(! vhostdict) {
    vhostdict = Tcl_NewDictObj();
    Tcl_DictObjPut(tcl, cs->conf->listen, key, vhostdict);
  }

  Tcl_DecrRefCount(key);

  if((r = Tcl_ListObjLength(tcl, vhosts, &ni)) != TCL_OK)
    return r;

  if(ni == 1 && 0 == strcmp("+", Tcl_GetString(vhosts))) {
    //use all.
    //TODO clear the vhostdict, mark listener as universal or something.
    //need to avoid adding more vhosts to it, and makes it easier at runtime
    //to determine if request should be processed.
  } else for(i = 0; i < ni; ++i) {
    Tcl_Obj *n, *o;
    if((r = Tcl_ListObjIndex(tcl, vhosts, i, &n)) != TCL_OK)
      return r;
    if(! n) // outside list, shouldn't happen.
      continue;
    if((r = Tcl_DictObjGet(tcl, cs->conf->vhosts, n, &o)) != TCL_OK)
      return r;
    if(! o) {
      Tcl_AppendObjToErrorInfo(tcl, n);
      Tcl_AddErrorInfo(tcl, ": unknown vhost");
      return TCL_ERROR;
    } else {
      Tcl_DictObjPut(tcl, vhostdict, n, o);
      assert(o->refCount > 1);
    }
  }
  return r;
}

/** Function for syntactic suger comment blocks of the configuration.
    In global namespace to allow to be used in any namespace. */
static int
_tcl_comment   (ClientData  clientData,
                Tcl_Interp *tcl,
                int         objc,
                Tcl_Obj    *const objv[])
{
  return TCL_OK;
}

static int
_tcl_comment_c (ClientData  clientData,
                Tcl_Interp *tcl,
                int         objc,
                Tcl_Obj    *const objv[])
{
  if(0 == strcmp("*/", Tcl_GetString(objv[objc-1])))
    return TCL_OK;
  Tcl_AddErrorInfo(tcl, "Comment block does not end with '*/'");
  return TCL_ERROR;
}


configure_state*
minuted_configure_init (Tcl_Interp *tcl)
{
  int i;
  configure_state *cs = calloc(1, sizeof(*cs));

  cs->tcl = tcl;

  // increase ref count immediately
# define CREATE_STRING(name, value) \
  cs->string[name] = Tcl_NewStringObj (value, -1)

  CREATE_STRING (cs__errorinfo,   "-errorinfo");
  CREATE_STRING (cs_application,  "application");
  CREATE_STRING (cs_eval,         "eval");
  CREATE_STRING (cs_namespace,    "namespace");
  CREATE_STRING (cs_source,       "source");

  CREATE_STRING (cs_nsMinuted,       "::Minuted");
  CREATE_STRING (cs_nsMinutedVhost,  "::Minuted::Vhost");

  for(i = 0; i < cs_COUNT; ++i)
    Tcl_IncrRefCount(cs->string[i]);

  Tcl_CreateNamespace (tcl, "::Minuted", NULL, NULL);
  Tcl_CreateNamespace (tcl, "::Minuted::Vhost", NULL, NULL);
# define CREATE_COMMAND(name, proc) \
    Tcl_CreateObjCommand(tcl, name, proc, cs, NULL)

  CREATE_COMMAND("/*", _tcl_comment_c);
  CREATE_COMMAND("//", _tcl_comment);
  CREATE_COMMAND("disabled", _tcl_comment);
  CREATE_COMMAND("::Minuted::listen", minuted_tcl_listen);
  CREATE_COMMAND("::Minuted::vhost", minuted_tcl_vhost);
  CREATE_COMMAND("::Minuted::Vhost::application", vhost_tcl_application);

  return cs;
}

void
minuted_configure_destroy (configure_state *cs)
{
  int i;
  /* Drop our strings */
  for (i = 0; i < cs_COUNT; ++i)
    if(cs->string[i])
      Tcl_DecrRefCount(cs->string[i]);

  free(cs);
}

int
minuted_configure  (const char       *filename,
                    configuration    *c,
                    configure_state  *cs)
{
  Tcl_Interp *tcl = cs->tcl;

  cs->conf = c;

  if(c->vhosts)
    Tcl_DecrRefCount(c->vhosts);
  if(c->listen)
    Tcl_DecrRefCount(c->listen);

  if(! filename)
    return 0;

  Tcl_Obj *file   = Tcl_NewStringObj (filename, -1);

  c->vhosts = Tcl_NewDictObj();
  Tcl_IncrRefCount(c->vhosts);

  c->listen = Tcl_NewDictObj();
  Tcl_IncrRefCount(c->listen);

  Tcl_Obj *read[] = {
    cs->string[cs_namespace],
    cs->string[cs_eval],
    cs->string[cs_nsMinuted],
    cs->string[cs_source],
    file
  };

  int nread = sizeof(read)/sizeof(read[0]);
  int r;

  Tcl_IncrRefCount(file);

  if(Tcl_FSAccess(file, R_OK)) {
    Tcl_AppendObjToErrorInfo(tcl, file);
    Tcl_AddErrorInfo(tcl, ": file not found or insufficient privileges.");
    r = TCL_ERROR;
  } else {
    info("Loading configuration: %s", filename);
    r = Tcl_EvalObjv(tcl, nread, read, TCL_EVAL_GLOBAL);
  }

  Tcl_DecrRefCount(file);

  switch(r)
  {
  case TCL_OK:
    break;
  case TCL_ERROR: {
    Tcl_Obj* options = Tcl_GetReturnOptions(tcl, r);
    Tcl_Obj* errorinfo;
    Tcl_DictObjGet(NULL, options, cs->string[cs__errorinfo], &errorinfo);
    if(errorinfo)
      error(Tcl_GetString(errorinfo));
    else
      error(Tcl_GetStringResult(tcl));
    error("Unable to read configuration file.");
  } return 1;
  // config files shouldn't exit using break/continue/return.
  case TCL_BREAK:
    error("Configuration ended with break");
    return 2;
  case TCL_CONTINUE:
    error("Configuration ended with continue");
    return 2;
  case TCL_RETURN:
    error("Configuration ended with return");
    return 2;
  default:
    return 3;
  }

  cs->conf = NULL;
  return 0;
}
