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
libraries. This include Linux, MacOS and (with some slight modifications due
to POSIX) Windows as well.

## Building

The original JavaScript source code tar ball has only a partial build system and has therefore been changed by adding an autoconf/automake based build system. In general, building the prontoscript would consists of
```
autoreconf -fiv
./configure --disable-dependency-tracking
make
```

## Architecture

The architecture of the replication of ProntoScript is presented as
```
   ----------------------------------
  | Javascript 1.6                   |
  |                 ---------------- |
  |                | Event  | Socket |
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
|                    | print()       | Implemented.
|                    | *             | Not implemented.                       |
| TCPSocket          | *             | Not implemented.                       |
| UDPSocket          | *             | Not implemented.                       |
| Widget             | *             | Not implemented.                       |

## References

* [Javascript 1.6](https://github.com/Historic-Spidermonkey-Source-Code/JavaScript-1.6.0.git)

