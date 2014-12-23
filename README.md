Minute HTTP
===========

Minute HTTP is a minimal footprint, resource conscious, HTTP/1.1 server side
protocol library intended to be incorporated into other software to provide
built-in HTTP services. The limited resource consumption an lack of
dependencies could make it a good choice for embedded systems.

Neither `libminute-http` nor `libminute-httpd` allocate any memory except
minor stack allocations and static data such as the radix tries mentioned
below, and only work with client supplied buffers.

There are plenty of TODOs around the code. Be wary.

libminute-http
--------------

`libminute-http` is the HTTP protocol library, capable of parsing HTTP
requests. Known headers are recognized and -- if selected by the user -- its
value stored in a user defined buffer, with currently one exception (although
more will probably be added eventually), the `Connection` header is
interpreted. Unrecognized headers are currently ignored.

Parsing of the request is done using a hand written finite state machine, as
well as hand written, compacted, radix tries stored in linear tables to keep
the footprint to a minimum.

This library has no dependencies except a minimal set of the C standard
library: `memcpy` and `strlen`.

libminute-httpd
---------------

`libminute-httpd` provides a minimal HTTP service using `libminute-http`,
with client provided callbacks to handle the actual requests. Requests are
read from -- and responses are written to -- file descriptors using POSIX
scatter/gather I/O (readv and writev) using client provided memory blocks as
ring buffers.

There's currently no actual networking set up or threading code in this
library, which has to be provided by the surrounding application.

minuted
-------

`minuted` is a standalone HTTP server based on `libminute-httpd` and TCL. It
is intended as a proof of concept implementation using the minute libraries,
but might be useful for small setups. If nothing else it's got to have a
server that actually uses the libraries to see them in action and to get
immediate feedback regarding potential shortcomings and pitfalls with the
libraries.

`minuted` is a single application server, which means there'll only be one
active application -- per vhost -- handling the requests. The application,
which is a TCL script, is sourced at boot, and is not reread until the
configuration is reloaded. There's of course nothing keeping you from
creating a trampoline application which launches other applications from the
file system as needed.

Note that `minuted` does not make any attempt to maintain a minimal footprint,
which should be obvious by the presence of the TCL dependency.

There's currently no support for SSL in `minuted`, so if you're looking for
that, look elsewhere or consider contributing.

minuted configuration
=====================

The minuted configuration file format is actually a TCL script. This means
that all facilities provided by the TCL language can be used, like using
variables, loops and conditional constructs as well as including other files
simply by sourcing them.


Comments
--------

There are three accepted comment styles in the minuted configuration

  1. The TCL single line #-comment, note that this comment ignores the
     rest of the line regardless of braces or any other characters.
  2. C++-style //-comment. This is in reality a TCL command which ignores
     all arguments. This also means that braces can be used to make the comment
     multi-line. Note that braces need to be balanced.
  3. C-style /* */-comments. Same deal as with //-comments, this is in reality
     a TCL command, thus it'll be single line unless you use braces. To make
     sure we don't accidentally close the comment early, it checks that the
     last argument is a '*/' token. Furthermore, since it's actually a command
     and the braces need to be balanced, and thus these comments do nest.

Virtual hosts
-------------

Virtual hosts are set up using the vhost command,

    vhost name {body}

where the name matches the Host: header sent by the client. One excpetion to
this rule is the 'default' Vhost which -- if assigned to the active port --
will be used for unknown hosts (or if the Host: header is missing, e.g. if
the client is using HTTP/1.0).

The body of the vhost specify vhost specific settings.

### Vhost application

Minuted is a single-application server, thus only one application can be active
per vhost. It is specified using the application command

    application file-name

The application is a TCL script and is sourced once after the configuration has
been read. This means that you'll need to reload the configuration if the
application changes, or use a trampoline to explicitly source your applciation
script as needed. The application script must have at least two procs, 'head'
and 'payload'

    proc head {path query} {body}

The 'head' proc determines the outcome of the request and returns the status
code (i.e. 200 if all went well). The return value may be a list where the
first element is the numeric status code, the second element (if present) is
a dict containing headers to set, the rest will be ignored (but forwarded as-is
to the 'payload' function). The name 'head' might appear somewhat missleading,
as everything that could affect the outcome of the request (and therefore the
status code) would have to be in here, which usually boils down to most of the
actual processing except actually writing the response, which is delegated to
'payload'. No actual output can be produced here. Note that in case of a HEAD
request, only 'head' is called.

    proc payload {path query status channel} {body}

The 'payload' proc produces the response payload. The 'status' variable is the
result returned by the 'head' function and the 'channel' is the writable
channel to write the payload to. Remember to configure 'channel' to binary if
you're sending binary data.

To pass information from head to payload, append it to the list returned from
head, which will passed on as-is to the payload function; do not use global
variables, as that is sure to break at some point when request handling gets
interleaved within the same process.

Listening
---------

Listening sockets are created using the listen command,

    listen bind-address port {vhosts}

where 'bind-adress' can be a specific interface or 0.0.0.0 or * for all ipv4,
or :: for all ipv6. Vhosts lists the vhosts to be associated with this address,
don't forget to list 'default' if you want it to be used. Specify vhosts as '+'
to use all available vhosts, regardless of where they're declared.

Examples
--------

A minimal minuted configuration could look like this

    vhost default {
      application /var/www/default.tap
    }

    listen :: 80 +

Where `/var/www/default.tap` would be the file sourced as application. A
minimal application could look like this

    proc head {path query} {
      set headers [dict create content-type text/html]
      set data "Some data"
      return [list 200 $headers $data]
    }

    proc payload {path query status channel} {
      set data [lindex $status 2]
      puts $channel [subst {
        <html>
          <head>
            <title>It works!</title>
          </head>
          <body>
            <h1>It works!</h1>
            <p>Status: [lindex $status 0]</p>
            <p>Path: $path</p>
            <p>Query: $query</p>
            <p>Data: $data</p>
          </body>
        </html>
      }]
    }
