
#### `Can't open /dev/net/tun` 
Ref: https://www.kernel.org/doc/html/latest/networking/tuntap.html
Exerpt:
> `mkdir /dev/net` (if it doesn't exist already)

> `mknod /dev/net/tun c 10 200`

> `chmod 0666 /dev/net/tun`

> There’s no harm in allowing the device to be accessible by non-root users, since CAP_NET_ADMIN is required for creating network devices or for connecting to network devices which aren’t owned by the user in question.