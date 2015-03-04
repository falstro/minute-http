/* Tool for brute-forcing a patricia trie into a single table as compactly as
  possible, given a list of strings to match, the character to index-mapping
  is currently hardcoded (ctoi below).

  Currently, it will attempt to brute force the entire solution space (except
  where smaller solutions have already been found), and thus takes quite a
  while to finish. To stop early, a signal handler for sigint is registered.
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAXSZ 64
#define TBLSZ 28

#define VISITED     0x02

struct tblentry_s;
typedef struct node_s
{
  char       *elem;
  int         limit;
  int         flags;
  int         next;
  int         check;
  uint64_t    mask;
  const
  char       *terminal;
  struct
  tblentry_s *te;
  struct
  node_s *tbl[TBLSZ];
}
node;

typedef struct tblentry_s
{
  node   *node;
  int     offset;
}
tblentry;

int
ctoi(char c)
{
  if(c >= 'a' && c <= 'z')
    return c - 'a';
  else if(c >= 'A' && c <= 'Z')
    return c - 'A';
  else if(c == '-')
    return 'z'-'a'+1;
  else return 'z'-'a'+2;
}

void
split(node *root, int offs)
{
  int i = ctoi(root->elem[offs]);
  node *n = calloc(1, sizeof(*n));

  memcpy(n, root, sizeof(*root));
  n->elem = strdup(root->elem+offs);
  root->elem[offs] = 0;
  bzero(root->tbl, sizeof(root->tbl));
  root->tbl[i] = n;
  root->mask  = 1u<<i;
  root->terminal = 0;
  root->limit = i+1;
}

int
insert(node* root, const char *terminal, const char *buf, int buflen)
{
  char *s = root->elem;
  while(*s && *s == *buf)
    s++, buf++, buflen--;

  if(!*buf) {
    if(!*s) {
      if(root->terminal)
        fprintf(stderr, "WARNING: Duplicate terminal\n");
    }
    split(root, s-root->elem);
  } else if(!*s) {
    int i = ctoi(*buf);
    if(!root->tbl[i]) {
      node *n = root->tbl[i] = calloc(1, sizeof(*n));
      root->mask |= 1u<<i;
      n->elem = strdup(buf);
      n->terminal = terminal;
      if(i+1 > root->limit)
        root->limit = i+1;
    } else return insert(root->tbl[i], terminal, buf, buflen);
  } else {
    int i = ctoi(*buf);
    split(root, s-root->elem);

    node *n = calloc(1, sizeof(*n));
    n->elem = strdup(buf);
    n->terminal = terminal;
    root->tbl[i] = n;
    root->mask |= 1u<<i;

    if(i+1 > root->limit)
      root->limit = i+1;
  }

  return 0;
}

void
dump(node* root, int indent)
{
  int i;
  for(i = 0; i < indent; ++i)
    printf(" ");
  printf("%s%s(%d)\n", root->elem, root->terminal?":":"",root->check);
  indent += strlen(root->elem);
  for(i = 0; i < root->limit; ++i)
    if(root->tbl[i])
      dump(root->tbl[i], indent);
}

node**
walk(node* root, node** tbl)
{
  int i, j;
  node** full = tbl;
  *(tbl++) = root;
  for(i = 0; i < root->limit; ++i)
    if(root->tbl[i]) {
      tbl = walk(root->tbl[i], tbl);
    }
  for(i = 0; i < MAXSZ; ++i)
    if(full[i]) {
      full[i]->next = i;
      for(j = 0; j < full[i]->limit; ++j)
        if(full[i]->tbl[j])
          full[i]->tbl[j]->check = i;
    } else
      break;
  return tbl;
}

int stop = 0;

void sigint(int x) { stop = 1; }

/* TODO use monte-carlo instead? current header list ends up very compact
  after only a few iterations, so I don't think there's much to gain at
  this point.*/
int
fit(node **list, tblentry *table, uint64_t mask, int lo, int hi, int l)
{
  int i, limit = hi, accepted = 0, empty = 1;
  tblentry mintbl[MAXSZ] = {};
  for(i = 0; i < MAXSZ; ++i) {
    int o, j;
    node *n = list[i];
    if(! n)
      break;
    if(n->flags & VISITED)
      continue;
    if(!n->limit)
      continue; // no children to be placed.

    empty = 0;

    n->flags |= VISITED;
    for(o = 0; o + TBLSZ < limit && ! stop; ++o) {
      if(mask & (n->mask<<o))
        continue;

      {
        int sz, nlow = TBLSZ + o; //n->limit + o;
        if(nlow < lo)
          nlow = lo;
        tblentry ntbl[MAXSZ];
        memcpy (ntbl, table, sizeof(ntbl));

        for(j = 0; j < n->limit; ++j) {
          if(n->tbl[j]) {
            ntbl[j+o].node = n->tbl[j];
            ntbl[j+o].offset = o;
          }
        }

        sz = fit(list, ntbl, mask | (n->mask<<o), nlow, limit, l+1);
        if(sz < limit) {
          accepted = 1;
          limit = sz;
          memcpy(mintbl, ntbl, sizeof(ntbl));
        }
      }

      break; // always place at first possible location.
    }
    n->flags ^= VISITED;
  }

  if(empty) {
    fprintf(stderr, "Found solution at size %d\n", lo);
    return lo;
  } else if(accepted) {
    memcpy(table, mintbl, sizeof(mintbl));
  }
  return limit;
}

void
fixate(tblentry *table, int limit)
{
  int i;
  for(i = 0; i < MAXSZ; ++i)
    if(table[i].node)
      table[i].node->te = &table[i];
}

int
_dumptbloffset(node *n)
{
  int i;
  for(i = 0; i < n->limit; ++i)
    if(n->tbl[i])
      return n->tbl[i]->te->offset;
  return 0;
}

void
dumptbl(tblentry *tbl)
{
  int i, first = 1;
  int sz = TBLSZ;
  for(i = 0; i < sz; ++i) {
    printf("%s\n  ", first?"":",");
    if(i < MAXSZ && tbl[i].node) {
      char buf[32];
      int next = tbl[i].node->limit ? tbl[i].node->next : 99;
      int offset = _dumptbloffset(tbl[i].node);
      if (offset+TBLSZ > sz)
        sz = offset+TBLSZ;

      sprintf(buf, "\"%s\",", tbl[i].node->elem);
      printf("{%-21s%2d,%2d,%2d, %s}", buf,
        tbl[i].node->check, next, offset,
        tbl[i].node->terminal?tbl[i].node->terminal:"0");
    } else {
      printf("{%-21s 0, 0, 0, 0}","NULL,");
    }
    first = 0;
  }
}

int
main(int argc, char **argv)
{
  node root = {"", 1};
  node *list[MAXSZ] = {};
  tblentry table[MAXSZ] = {};

  char buffer[64];
  int  ln = 0;
  int  limit;
  int  prefixlen = 0;
  const char *prefix = "";

  if(argc > 1) {
    prefix = argv[1];
    prefixlen = strlen(prefix);
  }

  while(fgets(buffer, 64, stdin))
  {
    ln++;
    int len = strlen(buffer);
    if(!len) {
      continue;
    } else if(buffer[len-1] != '\n') {
      fprintf(stderr, "WARNING: No new line on line %d, line too long?\n", ln);
      len++; //compensate chopping the new line character
    } else if(buffer[len-2] == '\r') {
      len--;
    }

    buffer[--len] = 0; //chop end of line character

    {
      char *p, *terminal = malloc(strlen(buffer)+prefixlen+1);
      *terminal = 0;
      strcat(terminal, prefix);
      strcat(terminal, buffer);
      for(p = terminal; *p; ++p)
        if(!isalnum(*p))
          *p='_';
      insert(&root, terminal, buffer, len);
    }
  }

  signal(SIGINT, sigint);

  walk(&root, list);
  dump(&root, 0);
  limit = fit(list, table, 0, 0, MAXSZ, 0);
  if(limit == MAXSZ) {
    fprintf(stderr, "Failed to fit table.\n");
    return 1;
  }
  fprintf(stderr, "Table limit: %d\n", limit);
  fixate(table, limit);
  printf("{");
  dumptbl(table);
  printf("\n}\n");
  return 0;
}
