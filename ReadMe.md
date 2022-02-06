
# stdiotuntap

Configure tun / tap devices using nothing but json over stdio!

Uses [libtuntap](https://github.com/LaKabane/libtuntap) as a shared c library

And [tiny-json](https://github.com/rafagafe/tiny-json) as a json parser

## State
Unusable

Implemented so far:
- json parsing from stdin
- tun device created / functional / tested
- able to grab origin and destination IP addresses of packets in english

## TODO
- json command listening
  - create device
  - init device
  - start device
  - pub/sub aka read device
  - destroy device
  - write to device
  - graceful shutdown
- json output
  - log info
  - notify device events
- silence libtuntap to prevent stdout pollution


## what
tf is this?

This program allows creation and usage of tun (and later on, tap) devices using a standard input output aka terminal connection.

What is tun/tap you ask?
Essentially a fake router connected to the machine running the tun/tap service, but all the traffic is forwarded to the software running tun/tap fake network device.

A tun/tap device will show up in `ifconfig`/`ipconfig`/`ip a` commands like one of your other network devices, but is not a physical device with a real MAC address.