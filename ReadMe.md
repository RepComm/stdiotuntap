
# stdiotuntap

Configure tun / tap devices using nothing but json over stdio!

## libs
- [libtuntap](https://github.com/LaKabane/libtuntap) as tun/tap driver
- [tiny-json](https://github.com/rafagafe/tiny-json) as a json parser
- [jsonwrite.c](./blob/master/src/jsonwrite.c) as (in-house 😎) json stringify
- [b64.c](https://github.com/littlstar/b64.c) as base64 encoder for packets in json

## State
Operational!

Can now both send and receive IP traffic thru tunnel to standard input/output over JSON/base64 strings. See [protocol.d.ts](./protocol.d.ts) for typescript definitions of JSON format and some example JSON messages to talk to this software over terminal

Minimum viable product is here and ready for testing!

Implemented so far:
- json commands input / response output thru stdio
- json stringify lib impl
- tun device create, up, down, destroy commands
- origin and destination IP packets decoded

## TODO
- json commands:
  - device subscribe/unsubscribe (all on by default currently)
- fix device pool non-refreshable behaviour
- rewrite b64.c to not be so weird

## what
tf is this?

This program allows creation and usage of tun (and later on, tap) devices using a standard input output aka terminal connection.

What is tun/tap you ask?
Essentially a fake router connected to the machine running the tun/tap service, but all the traffic is forwarded to the software running tun/tap fake network device.

A tun/tap device will show up in `ifconfig`/`ipconfig`/`ip a` commands like one of your other network devices, but is not a physical device with a real MAC address.
