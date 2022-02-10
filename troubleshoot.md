
#### `Can't open /dev/net/tun` 
Ref: https://www.kernel.org/doc/html/latest/networking/tuntap.html
Exerpt:
> `mkdir /dev/net` (if it doesn't exist already)

> `mknod /dev/net/tun c 10 200`

> `chmod 0666 /dev/net/tun`

> There’s no harm in allowing the device to be accessible by non-root users, since CAP_NET_ADMIN is required for creating network devices or for connecting to network devices which aren’t owned by the user in question.

#### linux real network device has lots of extra inet6 after setting default route to created tunnel, causes `ping google.com` to send lots of DUP! responses
Restart network adapter on ubuntu:
`sudo /etc/init.d/network-manager restart`

#### linux set tunnel as default route
`sudo route add default gw 10.0.0.1 tun0`
replace 10.0.0.1 with ip of tun0
replace tun0 with the ifname of your  tunnel

#### vscode c++ format remove newline before open bracket
Reference: https://stackoverflow.com/a/50606364/8112809
In root object of `.vscode/settings.json`
`"C_Cpp.clang_format_fallbackStyle": "{ BasedOnStyle: Google }",`

Possibly also:
`"C_Cpp.vcFormat.newLine.beforeOpenBrace.namespace": "sameLine",`
`"C_Cpp.vcFormat.newLine.beforeOpenBrace.type": "sameLine",`
`"C_Cpp.vcFormat.newLine.beforeOpenBrace.lambda": "sameLine",`
`"C_Cpp.vcFormat.newLine.beforeOpenBrace.function": "sameLine",`
`"C_Cpp.vcFormat.newLine.beforeOpenBrace.block": "sameLine",`