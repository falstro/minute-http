#ifndef __MINUTED_MAIN_H__
#define __MINUTED_MAIN_H__

//TODO log functions should have some kind of meta data structure
void  fatal  (int         error,
              const char *msg, ...);
void  error  (const char *msg, ...);
void  warn   (const char *msg, ...);
void  info   (const char *msg, ...);
void  debug  (const char *msg, ...);

//TODO access log should not be free-text.
void  acclog (const char *msg, ...);

void  logs   (char        type,
              const char *msg,
              int         len);

#endif /* idempotent include guard */
