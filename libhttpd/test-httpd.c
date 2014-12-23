#include "libhttp/iobuf.h"
#include "libhttp/textint.h"
#include "libhttp/http.h"
#include "libhttp/http-headers.h"
#include "httpd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* This is not really a proper test. It doesn't actually check anything
   except that it doesn't crash (which is something I suppose). The output
   needs to be looked over manually to verify proper operation. */

static unsigned
test_head  (minute_http_rq     *rq,
            minute_httpd_head  *head,
            textint            *text,
            void               *user)
{
  char buffer[64];

  snprintf (buffer, sizeof(buffer), "path:%s,query:%s",
           &((char*)text->data)[rq->path], &((char*)text->data)[rq->query]);
  head->string (http_rsp_set_cookie, buffer, head);
  head->timestamp (http_rsp_last_modified, 1314524588, head);
  return 200;
}

static void
test_error   (minute_http_rq   *rq,
              unsigned          status,
              void             *user)
{
}

static unsigned
test_payload (minute_http_rq   *rq,
              minute_httpd_out *out,
              textint          *text,
              unsigned          status,
              void             *user)
{
  char x[64];
  out->write(x,
    snprintf (x, sizeof(x), "in payload, status: %d\n", status),
    out
  );
  return 0;
}

static int
test_inetd()
{
  minute_httpd_app app = {
    test_head,
    test_payload,
    test_error
  };
  return minute_httpd_handle (0, 1, &app, 0);
}

int run_test(int (*testfunc)(void), const char *request, int expected);

int
main (void)
{
  return
  run_test (test_inetd,
    "GET /test/uri?with&query%20string HTTP/1.1\r\n"
    "Host: minute.example.org\r\n"
    "\r\n"
    "GET / HTTP/1.1\r\n"
    "Connection: close\r\n"
    "\r\n", 0)
  ||
  run_test (test_inetd,
    "GSET / HTTP/1.1\r\n"
    "\r\n", -400)
  ;
}

int
run_test (int (*testfunc)(void),
          const char *request,
          int expected)
{
  int toch[2];
  int topa[2];
  pid_t child;
  int status;
  pipe(toch);
  pipe(topa);
  if ((child = fork())) {
    int to, from, c;
    char buffer[4096];
    /*parent*/
    close(toch[0]);
    close(topa[1]);
    to = toch[1];
    from = topa[0];
    write(to, request, strlen(request));
    while((c = read(from, buffer, 4096)) > 0) {
      write(1, buffer, c);
    }
    waitpid(child, &status, 0);
    if (WEXITSTATUS(status)) {
      printf ("Exit status: %d\n", WEXITSTATUS(status));
    }
    return WEXITSTATUS(status);
  } else {
    /*child*/
    close(0);
    close(1);
    dup(toch[0]);
    dup(topa[1]);
    close(topa[0]);
    close(topa[1]);
    close(toch[0]);
    close(toch[1]);

    exit(expected != testfunc());
  }
}
