#ifndef __MINUTED_TAP_H__
#define __MINUTED_TAP_H__

#include <tcl8.5/tcl.h>

struct configuration;

struct tap_vhost
{
  Tcl_Interp  *tcl;

  Tcl_CmdInfo  head;
  Tcl_CmdInfo  payload;
};

struct tap_runtime
{
  Tcl_Interp           *tcl;
  struct configuration *c;
  Tcl_Obj              *vhostMap;
  Tcl_Obj             **vhostListen;

  struct tap_vhost     *v;
  int                   nv;
};

Tcl_Interp* minuted_tap_create (Tcl_Interp *parent,
                                Tcl_Obj    *name);

unsigned  minuted_tap_handle (int                 sock,
                              int                 listenId,
                              struct tap_runtime *tr);

#endif /* idempotent include guard */
