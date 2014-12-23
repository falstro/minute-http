#ifndef __MINUTE_MINUTED_H__
#define __MINUTE_MINUTED_H__

#include <tcl8.5/tcl.h>

typedef struct configuration configuration;

int       minuted_serve    (configuration  *c,
                            Tcl_Interp     *tcl);

void      minuted_accepted ();
void      minuted_closed   ();
#endif /* idempotent include guard */
