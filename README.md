# ProntoScript

ProntoScript is based on JavaScript 1.6 and is used in some Philips Pronto Pro
and Marantz programmable remote controls. It includes a number of extensions
and libraries to the original JavaScript:

* Implement (asynchroneous) event handling, as required for non-blocking network
  communication).
* Access to serial and network devices.
* Access to the remote control UI and Activities.

The official method of developing for these remotes is to use their official
development environments. These include simulators with ProntoScript execution
support. However, these development environments are no longer supported and
only operate on Windows platforms.

The ProntoScript engine provided here is a re-implementation that is able to
run on most modern devices that support a C++ compiler and POSIX compliant
libraries. This includes Linux, MacOS and (with some slight modifications due
to POSIX) Windows as well.

It's main aim is to enhance development capabilities to run ProntoScript scripts
directly on the development platform without having to incorporate it in a
remote control configuration or run it in a remote control simulator.

## Building

The original JavaScript source code tar ball has only a partial build system and has therefore been changed by adding an autoconf/automake based build system. In general, building the prontoscript would consist of
```
autoreconf -fiv
./configure --disable-dependency-tracking
make
```

## Architecture

Automated tests can be run after a successfull buid with
```
make check
```

## Architecture

The architecture of the replication of ProntoScript is presented as
```
   ----------------------------------
  | Javascript 1.6                   |
  |                 ---------------- |
  |                | Event  | Socket |
  |                |---------------- |
  |                |      Select     |
   ----------------------------------
```
At the core is the JavaScript 1.6, forked off the Historic SpiderMonkey github
site. This appears to be the same as used in the Philips ProntoEdit Professional
development environment and most likely also in the remote controls themselves.

This official JavaScript code has been extended with the ProntoScript Libraries
to allow the scripts to be operational on any environment. This would enable
the development and testing of scripts to be done in isolation without a need
for ProntoEdit Professional. The scripts developed in this manner would also
be made available as-is to ProntoEdit Professional for inclusion in a remote
control configuration.

The main change is made to the script invocation of the ProntoScript based
scripts as it would need to cater for asynchroneous behaviour. This means that
the script may start the event handling (e.g. connecting to a remote server
over TCP), but not block the remaining execution of the script. Only when a
connection could be established, this would trigger the execution of a user
defined callback function to communicate with the remote server. The handling
of all asynchroneous events will *only* happen after the script has been
execution, and will *only* finish until there are no events pending to be
handled. At that point the script will exit.

## Compatibility

Not all of the API provided by the official ProntoScript is available in this
re-implementation. The following is a status of compatibility:

| ProntoScript Class | Class Members | Compatibilty                           |
|:-------------------|:--------------|:---------------------------------------|
| Activity           | *             | Not implemented.                       |
| CF                 | *             | Not implemented.                       |
| Diagnostics        | *             | Not implemented.                       |
| DNSResolver        | *             | Not implemented.                       |
| Extender           | *             | Not implemented.                       |
| GUI                | *             | Not implemented.                       |
| Image              | *             | Not implemented.                       |
| Input              | *             | Not implemented.                       |
| Page               | *             | Not implemented.                       |
| Relay              | *             | Not implemented.                       |
| Serial             | *             | Not implemented.                       |
| System             | include()     | Implemented.                           |
|                    | print()       | Implemented.                           |
|                    | *             | Not implemented.                       |
| TCPSocket          | connected     | Implemented.                           |
|                    | onClose       | Implemented<sup>0</sup>.               |
|                    | onConnect     | Implemented<sup>0</sup>.               |
|                    | onData        | Implemented<sup>0</sup>.               |
|                    | onIOError     | Implemented<sup>0</sup>.               |
|                    | connect()     | Implemented.                           |
|                    | close()       | Implemented.                           |
|                    | write()       | Implemented.                           |
|                    | read()        | Implemented.                           |
|                    | *             | Not implemented.                       |
| UDPSocket          | onData        | Implemented<sup>0</sup>.               |
|                    | close()       | Implemented.                           |
|                    | send()        | Implemented.                           |
|                    | *             | Not implemented.                       |
| Widget             | *             | Not implemented.                       |

<sup>0</sup> Partial compatibility. Refer to the subsection below for more
details.

### TCPSocket

The `TCPSocket` callback functions would require the use of `this` when calling
methods or accessing properties on the associated socket instance.

### UDPSocket

The `UDPSocket` callback functions would require the use of `this` when calling
methods or accessing properties on the associated socket instance.

## Miscellaneous

A number of miscellaneous API is provided to support this ProntoScript project.

| Miscellaneous Class | Class Members | Description                            |
|:--------------------|:--------------|:---------------------------------------|
| JSUnit              | add           | Add a test-case                        |
|                     | assert        | Execute an assertion                   |
|                     | events        | Run all events until none are left     |
|                     | run           | Run all test cases                     |

## References

* [Javascript 1.6](https://github.com/Historic-Spidermonkey-Source-Code/JavaScript-1.6.0.git)

