#ifndef __MINUTED_CONFIG_H__
#define __MINUTED_CONFIG_H__

#define VERSION           "0.1"
#define SERVER_CONFIG     "/etc/minuted.conf"

#include <tcl8.5/tcl.h>

typedef struct configure_state configure_state;

typedef struct
configuration
{
  Tcl_Obj  *vhosts;
  Tcl_Obj  *listen;
}
configuration;

configure_state*  minuted_configure_init     (Tcl_Interp       *tcl);
int               minuted_configure          (const char       *filename,
                                              configuration    *config,
                                              configure_state  *state);
void              minuted_configure_destroy  (configure_state  *state);


#endif /* idempotent include guard */
