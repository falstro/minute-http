#ifndef __MINUTED_TAP_H__
#define __MINUTED_TAP_H__

#include <tcl8.5/tcl.h>

struct configuration;

#define TAP_NO_PAYLOAD 0x01

struct tap_vhost
{
  Tcl_Interp *tcl;

  unsigned    flags;

  Tcl_CmdInfo headers;
  Tcl_CmdInfo payload;
  Tcl_CmdInfo response;
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
