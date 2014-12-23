#include "textint.h"

unsigned
minute_textint_textsize (textint *txt)
{
  return txt->text;
}

unsigned
minute_textint_intsize (textint *txt)
{
  return (txt->size-txt->ints)/sizeof(int);
}

int
minute_textint_putc  (const char  c,
                      textint    *txt)
{
  unsigned lo = txt->text;
  unsigned hi = txt->ints;

  if(lo >= hi)
    return 0;

  ((char*)txt->data)[lo] = c;
  txt->text++;
  return 1;
}

int
minute_textint_puti  (const int   i,
                      textint    *txt)
{
  unsigned lo = txt->text;
  unsigned hi = txt->ints-sizeof(i);

  if(lo >= hi)
    return 0;

  ((int*)txt->data)[hi/sizeof(i)] = i;
  txt->ints -= sizeof(i);
  return 1;
}

int
minute_textint_geti  (int         offset,
                      textint    *text)
{
  unsigned o = (offset < 0 ? text->ints : text->size)/sizeof(int)-1;
  return ((int*)text->data)[o-offset];
}

char *
minute_textint_gets  (int         offset,
                      textint    *text)
{
  unsigned o = offset < 0 ? text->text : 0;
  return &((char*)text->data)[o+offset];
}

int
minute_textint_replacec  (int         offset,
                          const char  c,
                          textint    *text)
{
  unsigned o = offset < 0 ? text->text : 0;
  ((char*)text->data)[o+offset] = c;
  return 1;
}

int
minute_textint_replacei  (int         offset,
                          const int   i,
                          textint    *text)
{
  unsigned o = (offset < 0 ? text->ints : text->size)/sizeof(i)-1;
  ((int*)text->data)[o-offset] = i;
  return 1;
}
