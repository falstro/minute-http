#include "main.h"
#include "minuted.h"
#include "tap.h"
#include "config.h"

//TODO transitive include?
#include "libhttp/http.h"
#include "libhttpd/httpd.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

#include <semaphore.h>
#include <mqueue.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/wait.h>

static const char *s_application = "application";
static const char *s_headers = "headers";
static const char *s_payload = "payload";
static const char *s_response = "response";

static const char *s__minuted = "/minuted";

extern mqd_t static_log_mqd;

/* SIGUP handler, set the flag and be done. */
static volatile sig_atomic_t sighup_flag = 0;
static void
sighup(int sig, siginfo_t *si, void *uc)
{
  sighup_flag = 1;
}

static volatile sig_atomic_t sigterm_flag = 0;
static void
sigterm(int sig, siginfo_t *si, void *uc)
{
  sigterm_flag = 1;
}

/* SIGCHLD handler, don't need to actually do anything here, just make sure
   mq_receive exist with EINTR. */
static void
sigchld(int sig, siginfo_t *si, void *uc)
{}

typedef struct
runstate
{
  struct tap_runtime  tap;

  int                *ssocks;
  int                 nssocks;
}
runstate;

static int
minuted_serve_listen (runstate *rs)
{
  configuration *c = rs->tap.c;
  Tcl_Interp *tcl = rs->tap.tcl;

  Tcl_Obj *k, *v;
  Tcl_DictSearch ds;
  int i, done;
  if(Tcl_DictObjFirst(tcl, c->listen, &ds, &k, &v, &done) != TCL_OK)
    return -1;
  for(i = 0; !done; ++i, Tcl_DictObjNext(&ds, &k, &v, &done)) {
    Tcl_Obj *addr, *srvc;
    if(Tcl_ListObjIndex(tcl, k, 0, &addr) != TCL_OK)
      return -1;
    if(Tcl_ListObjIndex(tcl, k, 1, &srvc) != TCL_OK)
      return -1;

    int vhostsz;
    if(Tcl_DictObjSize(tcl, v, &vhostsz) != TCL_OK)
      return -1;

    //listen to all vhosts.
    if(vhostsz == 0)
      v = c->vhosts;

    int gaie;
    struct addrinfo hints = {};
    struct addrinfo *ai = 0;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    gaie = getaddrinfo(Tcl_GetString(addr), Tcl_GetString(srvc), &hints, &ai);

    if(gaie) {
      error("%s: %s", Tcl_GetString(addr), gai_strerror(gaie));
      return -1;
    }


    int s = socket(ai->ai_family, SOCK_STREAM, ai->ai_protocol);
    if(s < 0) {
      error("Failed to create socket: %s", strerror(errno));
      freeaddrinfo(ai);
      return -1;
    }

    int soopt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &soopt, sizeof(soopt));

    if(bind(s, ai->ai_addr, ai->ai_addrlen))
    {
      error("Unable to bind: %s", strerror(errno));
      freeaddrinfo(ai);
      return -1;
    }

    if(listen(s, 10))
    {
      error("Unable to listen: %s", strerror(errno));
      freeaddrinfo(ai);
      return -1;
    }

    freeaddrinfo(ai);
    rs->ssocks[i] = s;
    rs->tap.vhostListen[i] = v;
  }

  return 0;
}

static int
minuted_serve_load (runstate *rs)
{
  configuration *c = rs->tap.c;
  Tcl_Interp *tcl = rs->tap.tcl;

  Tcl_Obj *name, *vhost;
  //TODO interned strings.
  Tcl_Obj *application = Tcl_NewStringObj(s_application, -1);
  Tcl_DictSearch ds;
  int r, i, done, res = 0;

  if(Tcl_DictObjFirst(tcl, c->vhosts, &ds, &name, &vhost, &done) != TCL_OK)
    return -1;

  Tcl_IncrRefCount(application);

  for(i = 0; !done; ++i, Tcl_DictObjNext(&ds, &name, &vhost, &done)) {
    Tcl_Obj *app;
    if((r = Tcl_DictObjGet(tcl, vhost, application, &app))
        != TCL_OK)
    {
      res = -1;
      break;
    }

    if(!app) {
      error("No application defined");
      res = -1;
      break;
    } else {
      Tcl_Interp *s = minuted_tap_create(tcl, name);
      //TODO move this to tap.c?

      if(Tcl_EvalFile(s, Tcl_GetString(app))) {
        //TODO full stack trace? at least the file name?
        error(Tcl_GetStringResult(s));
        res = -1;
        break;
      }

      if(Tcl_GetCommandInfo(s, s_headers, &rs->tap.v[i].headers) != 1) {
        error("Application has no headers function");
        res = -1;
        break;
      }

      if(Tcl_GetCommandInfo(s, s_payload, &rs->tap.v[i].payload) != 1) {
        info("Application has no payload function");
        rs->tap.v[i].flags |= TAP_NO_PAYLOAD;
      }

      if(Tcl_GetCommandInfo(s, s_response, &rs->tap.v[i].response) != 1) {
        error("Application has no response function");
        res = -1;
        break;
      }

      rs->tap.v[i].tcl = s;
      Tcl_DictObjPut(tcl, rs->tap.vhostMap, name, Tcl_NewIntObj(i));
      info("Application loaded: %s", Tcl_GetString(app));
    }
  }

  Tcl_DecrRefCount(application);
  return res;
}

static int
minuted_serve_processor(runstate *rs,
                        sem_t    *sem)

{
  struct pollfd *fds = calloc(rs->nssocks, sizeof(*fds));
  int i, res = 0;
  int nfds = rs->nssocks;

  for(i = 0; i < nfds; ++i) {
    struct pollfd fd = {rs->ssocks[i], POLLIN|POLLPRI, 0};
    fds[i] = fd;
  }

  while(!sighup_flag) {
    int r, sock;
    struct sockaddr_in raddr;
    socklen_t socklen = sizeof (raddr);

    if(sem_wait(sem)) {
      if(errno == EINTR)
        continue;
      res = 3;
      break;
    }
    r = poll (fds, nfds, 60 * 1000);

    if (0> r) {
      if(errno == EINTR)
        continue;
      res = 4;
      sem_post(sem);
      break;
    } else if (r == 0) {
      sem_post(sem);
    } else {
      for(i = 0; i < nfds; ++i) {
        if (fds[i].revents & (POLLIN|POLLPRI)) {
          sock = accept (fds[i].fd, (struct sockaddr*)&raddr, &socklen);
          int e = errno;

          sem_post(sem);
          minuted_accepted();

          if (0> sock) {
            if(e == EAGAIN || e == EINTR)
              break;
            error(strerror(e));
          } else {
            minuted_tap_handle(sock, i, &rs->tap);
            close (sock);
            minuted_closed();
            break;
          }
        }
      }
    }
  }

  if(sighup_flag) {
    debug("SIGHUP received; child");
    sighup_flag = 0;
  }

  free(fds);
  if(res)
    error("child exiting: %d", res);
  return res;
}

typedef struct
minuted_forks
{
  int     npids;
  int     active;
  pid_t  *pids;
}
minuted_forks;

//TODO collect these in a child-process management struct instead.
static int
minuted_fork       (minuted_forks *forks,
                    runstate *rs,
                    sem_t    *sem,
                    mqd_t     mqd)
{
  pid_t pid;
  if(forks->active >= forks->npids) {
    info("Process limit reached");
    return -1;
  } else if(0 == (pid = fork())) {
    signal(SIGPIPE, SIG_IGN);
    static_log_mqd = mqd;
    int r = minuted_serve_processor(rs, sem);
    static_log_mqd = -1;
    sem_close(sem);
    mq_close(mqd);
    exit(r);
    //TODO restart children as required if they exit/load increases.
  } else if (pid > 0) {
    forks->pids[forks->active++] = pid;
    debug("Launched child process: %d", pid);
  } else {
    error("Unable to fork worker process: %s", strerror(errno));
    return -1;
  }
  return 0;
}
static int
minuted_wait       (minuted_forks *forks,
                    runstate *rs,
                    sem_t    *sem,
                    mqd_t     mqd,
                    int       allowRestart)
{
  int w = 0, status;
  while(forks->active && (w = waitpid (-1, &status, WNOHANG)) > 0)
  {
    int i, found = 0;
    for (i = 0; i < forks->active; ++i) if(w == forks->pids[i]) {
      found = 1;
      forks->pids[i] = forks->pids[--forks->active];
      forks->pids[forks->active] = 0;
      break;
    }
    if(found) {
      int restart = allowRestart;
      if(WIFSIGNALED(status)) {
        error("child terminated: %d signal: %d", w, WTERMSIG(status));
      } else if(WEXITSTATUS(status)) {
        info("child exited: %d status: %d", w, WEXITSTATUS(status));
      } else {
        restart = 0;
      }

      if(restart)
        minuted_fork(forks, rs, sem, mqd);
    } else {
      //this is fine let it disappear
      info("stale child exited: %d status: %s", w, WEXITSTATUS(status));
    }
  }

  if (w < 0) {
    error("waitpid failed: %s", strerror(errno));
  }

  return 0;
}

static int
minuted_mq_receive (mqd_t   mqd,
                    char   *msgbuf,
                    size_t  msgbuf_len,
                    const struct timespec *abs_timeout)
{
  int r;
  if(abs_timeout)
    r = mq_timedreceive(mqd, msgbuf, msgbuf_len, 0, abs_timeout);
  else
    r = mq_receive(mqd, msgbuf, msgbuf_len, 0);
  if(r < 0) {
    if(errno == EINTR) {
      //loop and handle SIGCHLD.
      return 0;
    } else if(errno == ETIMEDOUT) {
      return -1;
    } else {
      return -2;
    }
  } else if(r) {
    msgbuf[r] = 0;
    //intercept log messages here.
    switch(msgbuf[0]) {
      case 'A':
      case 'F':
      case 'E':
      case 'W':
      case 'I':
      case 'D':
        logs(msgbuf[0], msgbuf+1, r-1);
        r = 0;
        break;
    }
  }

  return r;
}

static int
minuted_control_pack(char *buf, char type, pid_t pid)
{
  int i, l = 0;
  buf[l++] = type;
  for(i = 0; i < sizeof(pid); ++i) {
    buf[l++] = pid&0xff;
    pid >>= 8;
  }
  return l;
}

static int
minuted_control_unpack(char *buf, char *type, pid_t *pid)
{
  int i, l = 0;
  *type = buf[l++];
  *pid = 0;
  for(i = sizeof(pid)-1; i >= 0; --i) {
    *pid <<= 8;
    *pid |= buf[l+i]&0xff;
  }
  l += sizeof(pid);
  return l;
}

static void
minuted_control_message(char type)
{
  char buf[512];
  mqd_t mqd = static_log_mqd;
  if(mqd >= 0) {
    int len = minuted_control_pack(buf, type, getpid());
    if(mq_send(mqd, buf, len+1, 0)) {
      //not much else we can do here.
      perror("mq_send");
    }
  }
}

void
minuted_accepted()
{
  minuted_control_message('a');
}

void
minuted_closed()
{
  minuted_control_message('c');
}

#ifdef MINUTED_SINGLE_PROCESS
static int
minuted_serve_single   (runstate *rs)
{
  sem_t *sem = sem_open(s__minuted, O_CREAT|O_EXCL, 0600, 1);
  if(!sem) {
    const char *e = strerror(errno);
    error("Unable to create semaphore");
    error(e);
    return -1;
  }
  sem_unlink(s__minuted);

  signal(SIGPIPE, SIG_IGN);

  while(!sighup_flag && !sigterm_flag) {
    minuted_serve_processor(rs, sem);
  }

  sem_close(sem);
  return 0;
}
#else
static int
minuted_serve_prefork  (runstate *rs)
{
  int i, res = 0;
  sem_t *sem;
  mqd_t mqd;

  //TODO configurable number of processes
  int max_forks = 32;
  int min_spares = 3;
  int max_spares = 6;
  int init_forks = 6;

  int serving = 0;

  minuted_forks forks = {max_forks};
  forks.pids = calloc(forks.npids, sizeof(*forks.pids));

  /* The semaphore is needed for either of two reasons:
     1. Using blocking listener sockets, once a connection comes in, all
        child polls will wake up and all try to accept, thus all end up
        blocking except one. This is fine if you only have one socket to
        listen to, but we can't make that assumption.
     2. Using non-blocking listener has a related effect where all child
        polls wake up all attempting to accept, all but one get EAGAIN, and
        go back to the poll; however the accept has not yet gone through
        and we spin out of control. Actually crashing after a couple of
        hundred iterations on my system.
  */
  sem = sem_open(s__minuted, O_CREAT|O_EXCL, 0600, 0);
  if(!sem) {
    const char *e = strerror(errno);
    error("Unable to create semaphore");
    error(e);
    return -1;
  }
  sem_unlink(s__minuted);

  mqd = mq_open(s__minuted, O_RDWR|O_CREAT|O_EXCL, 0600, 0);
  if(mqd<0) {
    const char *e = strerror(errno);
    error("Unable to create message queue");
    error(e);
    sem_close(sem);
    return -1;
  }
  mq_unlink(s__minuted);

  struct mq_attr attr;
  if(mq_getattr(mqd, &attr)) {
    const char *e = strerror(errno);
    error("Unable to query message queue");
    error(e);
    mq_close(mqd);
    sem_close(sem);
    return -1;
  }

  struct sigaction sa = {};
  /* set up HUP handling to be inherited */
  sa.sa_sigaction = sighup;
  if(sigaction(SIGHUP, &sa, NULL))
    error("Failed to establish SIGHUP signal handler");

  for(i = 0; i < init_forks; ++i) {
    minuted_fork(&forks, rs, sem, mqd);
  }

  //treat TERM and INT as equal.
  sa.sa_sigaction = sigterm;
  if(sigaction(SIGTERM, &sa, NULL))
    error("Failed to establish SIGTERM signal handler");
  if(sigaction(SIGINT, &sa, NULL))
    error("Failed to establish SIGINT signal handler");

  sa.sa_sigaction = sigchld;
  sa.sa_flags = SA_NOCLDSTOP;
  if(sigaction(SIGCHLD, &sa, NULL))
    error("Failed to establish SIGCHLD signal handler");

  info("Starting request processing.");
  debug("Master PID: %d", getpid());
  sem_post(sem);

  char *msgbuf = calloc(attr.mq_msgsize + 1, 1);
  size_t msgbuf_len = attr.mq_msgsize;
  while(!sighup_flag && !sigterm_flag) {
    minuted_wait(&forks, rs, sem, mqd, 1);

    int r = minuted_mq_receive(mqd, msgbuf, msgbuf_len, 0);
    if(r < 0) {
      res = r < -1 ? r : -2;
      break;
    } else if (r > 0) {
      char type;
      pid_t pid;
      minuted_control_unpack(msgbuf, &type, &pid);
      debug("control message: %c %d", type, pid);
      switch(type) {
        case 'a': //accept
          //TODO keep track of who this is, in case the process crashes we
          //need to know if it was serving a request or was a spare.
          serving++;
          debug("connection accepted; serving %d", serving);
          if(forks.active - serving < min_spares) {
            minuted_fork(&forks, rs, sem, mqd);
          }
          break;
        case 'c': //close
          serving--;
          debug("connection closed; serving %d", serving);
          if(forks.active - serving > max_spares) {
            debug("Sending SIGHUP to %d", pid);
            kill(pid, SIGHUP);
          }
          break;
      default:
        error("Unknown message type '%c' received from child process.", type);
      }
    }
  }

  if(sighup_flag) {
    debug("SIGHUP received.");
    sighup_flag = 0;

    res = SIGHUP;
    /*
    for(i = 0; i < forks.active; ++i) {
      debug("Sending SIGHUP to: %d", forks.pids[i]);
      kill(forks.pids[i], SIGHUP);
    }
    //we're reloading, would prefer not having to kill our forks here, but the
    //children keep listening sockets open thus causing our bind to fail; so
    //TODO either close listening sockets in children on HUP or keep ours open.
    //TODO also, do not close message queue in that case.
    */
  }
  /* else -- do this every time for now. */
  {
    struct timespec ts;
    int r;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += 5;

    debug("Initiate shutdown.");
    debug("Sending SIGHUP.");
    for(i = 0; i < forks.active; ++i) {
      kill(forks.pids[i], SIGHUP);
    }

    while(-1 < (r = minuted_mq_receive(mqd, msgbuf, msgbuf_len, &ts))) {
      if(!r)
        minuted_wait(&forks, rs, sem, mqd, 0);
      if(!forks.active)
        break;
    }

    if(forks.active) {
      debug("Sending SIGTERM.");
      for(i = 0; i < forks.active; ++i) {
        kill(forks.pids[i], SIGTERM);
      }

      debug("Waiting.");
      for(i = 0; i < forks.active; ++i) {
        waitpid(forks.pids[i], 0, 0);
      }
    }

    debug("Shutdown complete.");
  }

  free(msgbuf);
  sem_close(sem);
  mq_close(mqd);

  return res;
}
#endif

int
minuted_serve  (configuration *c, Tcl_Interp *tcl)
{
  int i, r, res = -1;
  runstate rstate = {{tcl, c}};
  runstate *rs = &rstate;

  if(Tcl_DictObjSize(tcl, c->listen, &rs->nssocks) != TCL_OK)
    return -1;
  if(Tcl_DictObjSize(tcl, c->vhosts, &rs->tap.nv) != TCL_OK)
    return -1;

  rs->ssocks = calloc(rs->nssocks, sizeof(int));
  for(i = 0; i < rs->nssocks; ++i)
    rs->ssocks[i] = -1;

  rs->tap.vhostListen = calloc(rs->nssocks, sizeof(*rs->tap.vhostListen));
  rs->tap.v = calloc(rs->tap.nv, sizeof(*rs->tap.v));

  rs->tap.vhostMap = Tcl_NewDictObj();
  Tcl_IncrRefCount(rs->tap.vhostMap);

  if((r = minuted_serve_listen(rs))) {
    error("Failed to create listening sockets.");
  } else if((r = minuted_serve_load(rs))) {
    error("Failed to load applications.");
  } else {
    //TODO configurable fork/threading strategy?
#ifdef MINUTED_SINGLE_PROCESS
    res = minuted_serve_single(rs);
#else
    res = minuted_serve_prefork(rs);
#endif
  }

  Tcl_DecrRefCount(rs->tap.vhostMap);

  for(i = 0; i < rs->tap.nv; ++i)
    if(rs->tap.v[i].tcl != 0)
      Tcl_DeleteInterp(rs->tap.v[i].tcl);

  for(i = 0; i < rs->nssocks; ++i)
    if(rs->ssocks[i] >= 0)
      close(rs->ssocks[i]);

  free(rs->tap.v);
  free(rs->tap.vhostListen);
  free(rs->ssocks);
  return res;
}
