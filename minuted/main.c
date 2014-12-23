#include "config.h"
#include "main.h"
#include "minuted.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include <stdarg.h>
#include <mqueue.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void
version ()
{
  fprintf (stdout, "minuted/" VERSION " - a standalone minute HTTP daemon\n");
}

static void
usage (int help, const char *argv0)
{
  const char *b = strrchr(argv0, '/');
  FILE* out = help ? stdout : stderr;
  if (b) b++; else b = argv0;
  fprintf (out, "Usage:\t%s [-f config][-d pidfile][-hv]\n", b);
}


mqd_t static_log_mqd = -1;

//TODO proper log file handling.
//TODO pack request data onto log message, so we know what vhost, client, etc.
void
logs (char type, const char *msg, int len)
{
  struct tm tm;
  time_t t = time(0);
  gmtime_r(&t, &tm);
  char timebuf[32];
  fwrite(timebuf, 1, strftime(timebuf, 32, "%FT%TZ ", &tm), stderr);

  switch(type) {
    case 'F': fputs("[FATAL] ", stderr); break;
    case 'E': fputs("[ERROR] ", stderr); break;
    case 'W': fputs(" [WARN] ", stderr); break;
    case 'I': fputs(" [info] ", stderr); break;
    case 'D': fputs("[debug] ", stderr); break;
  }

  fwrite(msg, 1, len, stderr);
  fputs("\n", stderr);
}

static void
logva (char type, const char *msg, va_list ap)
{
  char buf[512];
  int len = vsnprintf(buf+1, sizeof(buf)-1, msg, ap);

  mqd_t mqd = static_log_mqd;
  if(mqd < 0) {
    logs(type, buf+1, len);
  } else {
    buf[0] = type;
    if(mq_send(mqd, buf, len+1, 0)) {
      //not much else we can do here.
      perror("mq_send");
    }
  }
}

void
fatal  (int code, const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  logva('F', msg, ap);
  va_end(ap);
  exit (code);
}

void
error  (const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  logva('E', msg, ap);
  va_end(ap);
}

void
warn   (const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  logva('W', msg, ap);
  va_end(ap);
}

void
info   (const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  logva('I', msg, ap);
  va_end(ap);
}

static int debug_enabled = 0;

void
debug  (const char *msg, ...)
{
  if(debug_enabled) {
    va_list ap;
    va_start(ap, msg);
    logva('D', msg, ap);
    va_end(ap);
  }
}

void
acclog (const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  logva('A', msg, ap);
  va_end(ap);
}

int
main (int     argc,
      char  **argv)
{
  int opt, r;

  const char *config = SERVER_CONFIG;
  const char *pidfile = NULL;
  int daemonize = 0;
  configuration c = {};

  while ((opt = getopt (argc, argv, "f:hvd:u:D")) != -1) {
    switch (opt) {
    case 'f':
      config = optarg;
      break;
    case 'd':
      pidfile = optarg;
      daemonize = 1;
      break;
    case 'D':
      debug_enabled = 1;
      break;
    case 'v':
      version();
      return 0;
    case 'h':
      version();
      usage(1, argv[0]);
      return 0;
    default:
      usage(0, argv[0]);
      return 1;
    }
  }

  Tcl_Interp *tcl = Tcl_CreateInterp();
  configure_state *cstate = minuted_configure_init(tcl);

  while(1) {
    if((r = minuted_configure (config, &c, cstate)))
      return r;

    if(daemonize) {
      daemonize = 0;
      int pidfd = creat(pidfile, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH);
      if(pidfd < 0) {
        error("Unable to create pidfile.");
        break;
      }

      pid_t pid = fork();
      if(pid > 0) {
        char pids[16];
        int l = snprintf(pids, 16, "%d\n", pid);
        write(pidfd, pids, l);
        close(pidfd);
        setsid();
        //TODO close fd 0, 1, 2 or redirect to /dev/null, along with proper
        //logging. Tee until reaching this point perhaps?
        break;
      }
      close(pidfd);
    }

    r = minuted_serve(&c, tcl);
    switch(r)
    {
      case 0:
        info("Server shut down.");
        break;
      case SIGHUP:
        continue;
      case -1:
        error("Unable to serve connections.");
        break;
      default:
        error("Server crashed.");
    }
    break;
  }

  minuted_configure_destroy(cstate);
  Tcl_DeleteInterp(tcl);

  return 0;
}
