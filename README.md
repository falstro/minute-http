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

I've tried to maintain doxygen comments throughout the code, however there's
currently make target or doxygen configuration to generate any standalone
documentation. There's also currently no real 'usage' description for the
libraries, but feel free to look around the `minuted` code for examples how
they're used. For a usage description of `minuted`, see below.

libminute-http
--------------

`libminute-http` is the HTTP protocol library, capable of parsing HTTP
requests. Known headers are recognized and -- if selected by the user -- their
values stored in a user defined buffer. The exceptions to this rule is that
the `Connection`, `Content-Length`, `Expect` and `Transfer-Encoding` headers
are interpreted and stored in the request structure.  More interpreted headers
will likely be added in the future. Unrecognized headers are currently
ignored.

Parsing of the request is done using a hand written finite state machine, as
well as compacted radix tries stored in linear tables to keep the footprint to
a minimum.

This library has no dependencies except a minimal set of the C standard
library: `memcpy` and `strlen`.

libminute-httpd
---------------

`libminute-httpd` provides a minimal HTTP service using `libminute-http`,
with client provided callbacks to handle the actual requests. Requests are
read from -- and responses are written to -- file descriptors using POSIX
scatter/gather I/O (readv and writev) operating on client provided memory
blocks as ring buffers.

There's currently no actual networking set up or threading code in this
library, which has to be provided by the surrounding application.

minuted
-------

`minuted` is a standalone HTTP server based on `libminute-httpd` and TCL. It
is intended as a proof of concept implementation using the minute libraries,
but might be useful for small setups. If nothing else it's good to have a
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

  1. The TCL single line `#`-comment, note that this comment ignores the
     rest of the line regardless of braces or any other characters.
  2. C++-style `//`-comment. This is in reality a TCL command which ignores
     all arguments. This also means that braces can be used to make the comment
     multi-line. Note that braces need to be balanced.
  3. C-style `/* */`-comments. Same deal as with `//`-comments, this is in
     reality a TCL command, thus it'll be single line unless you use braces. To
     make sure we don't accidentally close the comment early, it checks that
     the last argument is a `*/` token. Furthermore, since it's actually a
     command the braces need to be balanced, and thus these comments do nest
     as opposed to their C brethren. (If you're reading the markdown source,
     note that the backticks are for the markdown, and shouldn't actually be
     used in the config.)

Virtual hosts
-------------

Virtual hosts are set up using the vhost command,

    vhost name {body}

where the name matches the Host: header sent by the client. One exception to
this rule is the `default` Vhost which -- if assigned to the active port --
will be used for unknown hosts (or if the Host: header is missing, e.g. if
the client is using HTTP/1.0).

The body of the vhost specify vhost specific settings.

### Vhost application

Minuted is a single-application server, thus only one application can be active
per vhost. It is specified using the application command

    application file-name

The application is a TCL script and is sourced once after the configuration has
been read. This means that you'll need to reload the configuration if the
application changes, or use a trampoline to explicitly source your application
script as needed. The application script must have at least two procs,
`headers` and `payload`

    proc headers {path query meta} {body}

The `headers` proc determines the outcome of the request and returns the status
code (i.e. 200 if all went well). The return value may be a list where the
first element is the numeric status code, the rest will be ignored (but
forwarded as-is to the `payload` and `response` functions). The name
`headers` might appear somewhat misleading, as everything that could affect
the outcome of the request (and therefore the status code) would have to be in
here, which usually boils down to most of the actual processing except actually
writing the response, which is delegated to `response`. No actual output can be
produced here. No client payload may be read here either, if the application
suspects a payload is coming, it should return 100 (the HTTP code for
'Continue') which will result in the payload function being called with an
input channel. Note that this function will be called regardless even if there
is no payload to be read, also note that we have to return a 100 status for
the `payload` function to be called, even if the client didn't specify the
appropriate 'Expect'-header. If the client did specify the header, the httpd
library will send a 100 Continue message at this point to allow the client to
send the remainder of the request.

The meta object/function currently supports two functions, `get-header` and
`add-header` and can be accessed as follows:

    $meta add-header header-name header-value
    $meta get-header header-name

For example

    $meta add-header content-type application/json
    set ims [$meta get-header if-modified-since]

Note that add-header is not available in the `response` function, although
support for trailers might be added in the future.

    proc payload {path query meta channel status} {body}

The `payload` proc may read the client payload. The `status` variable is the
result returned by the `headers` function and the `channel` is the readable
channel to read the payload from. The payload returns a status just like the
`headers` function, and will be treated like a list where the first entry is
the http code to send and the rest will be ignored for the `response` method
to process.

A word of caution, if you expect trailers (i.e. headers being appended at the
end of a chunked request body) in the client request, these will not be parsed
and thus not available from the meta object until the entire payload has been
read.

Note that in case of a HEAD request, only `headers` and (if requested by
returning 100) `payload` are called.

    proc response {path query meta channel status} {body}

The `response` proc produces the response payload. The `status` variable is the
result returned by the `headers` or `payload` functions and the `channel` is
the writable channel to write the response to. Remember to configure `channel`
to binary if you're sending binary data.

To pass information from head to payload, append it to the list returned from
head, which will passed on as-is to the payload function; do not use global
variables, as that is sure to break at some point when request handling gets
interleaved within the same process.

Listening
---------

Listening sockets are created using the listen command,

    listen bind-address port {vhosts}

where `bind-adress` can be a specific interface or 0.0.0.0 or * for all ipv4,
or :: for all ipv6. `Vhosts` lists the vhosts to be associated with this
address, don't forget to list `default` if you want it to be used. Specify
vhosts as `+` to use all available vhosts, regardless of where they're
declared. You'll need to use a separate `listen` command for each
bind-address.

Examples
--------

A minimal minuted configuration could look like this

    vhost default {
      application /var/www/default.tap
    }

    listen :: 80 +

Where `/var/www/default.tap` would be the file sourced as application. A
minimal application could look like this. The payload function is optional,
and is only called if the headers function returns 100 (meaning continue, i.e.
we're expecting to read the client payload before determining the 'real' code).

    proc headers {path query meta} {
      $meta add-header content-type text/html
      set data "Some data"
      return [list 100 $data]
    }

    proc payload {path query meta channel status} {
      # read client payload here
      lset $status 0 200
      return $status
    }

    proc payload {path query meta channel status} {
      set data [lindex $status 1]
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
