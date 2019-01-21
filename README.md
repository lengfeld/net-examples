# Advanced network examples in C for Linux

Collection of advanced Linux network examples in C to demonstrate non-obvious
but useful features of the Linux/POSIX network API:

To try out the examples you need

* gcc
* make
* netcat (commandline tool `nc`)

To build the examples just execute

    $ make

*NOTE:* Currently there is only one example. Maybe there will be more in the
future.

## Example 1: Get local IP address of a TCP/UDP connection

When writing network servers it's often a good choice to listen for incoming
connections and packages on all available interfaces. Binding the socket to
only a specific interface or IP address is possible, but then the program logic
has to deal with changes in the network configuration. Interfaces can go up and
down or vanished completely and IP addresses can be removed and added. Using
the kernel feature to listen on all available interfaces makes the code simpler
and easier to read.

So you write your server code and bind the server socket to a specific port and
to the unspecific IP address 'in6addr_any' to listen on all available
interfaces.  Of course the code sets the socket option 'IPV6_V6ONLY' to false
to serve IPv6 and IPv4 (with IPv4-mapped IPv6 addresses) connections.

But maybe you have a new problem now. When the protocol that your server
implements allows to link other resources on the your host, think of an HTTP
URL and link, but does not allow relative links, e.g. because it's a HTTP
redirect to another port and protocol on your host, the server has to know the
local IP address or hostname.

Mostly our device or server does not have a fully qualified domain name and you
don't want to force your users to configure one.  Additionally you know that
there is *not* single IP address of a host. It depends on the network interface
(ethernet, wireless, ...) and may change over time (DHCP and IPv6
autoconfigure).

So the question remains: How to know the local IP address of your server that
can be included into a response message to the client and this local IP address
is reachable for the client?

Infact on the network protocol level (IP, TCP and UDP) answering this question
is easy. A TCP and UDP connection consists of the tuple: (client IP address,
client port, server IP address, server port). When the server's OS kernel has
an open TCP connection or receives an UDP packet, the kernel knows the local
server IP address that the client was connecting to! The information is
available on the network protocol level. The open questions is: How to get this
information in your server program?

In the C language the OS API exists. For a TCP socket you can use
`getsockname()`, see `man 2 getsockname`. The file `tcp-server.c` contains
example code. For UDP packets you can use the socket option `IPV6_RECVPKTINFO`,
see `man 7 ipv6`. See example in file `udp-server.c`.

For the Java language it seems that there is no API for that. E.g. the
stack overflow question

    Stack Overflow: Java, DatagramPacket receive, how to determine local ip interface
    https://stackoverflow.com/a/31966181

purposed a really bad solution for that problem. To get the local IP address,
the code should open a new connection back to the client and then read out
the local IP address via `sock.getLocalAddress()`.

Pointed conclusion: Only the C system APIs are the *real* APIs. Everything else
is just an incomplete wrapper.


### Try out the TCP example

First choose a local listing port (here '8080') and start the TCP server in a
shell:

    $ ./tcp-server 8080
    # is running

It starts and waits for incoming TCP connections. Now open a second terminal
window and use the netcat utility to connect to the server. You should try
different local IP addresses (IPv4 and IPv6):

    $ nc localhost 8080
    $ nc 127.0.0.1 8080
    $ nc 192.168.178.26 8080
    $ nc -6 ::1 8080
    $ nc -6 2001:aaaa:bbbb:0:cccc:eeee:eeee:ffff 8080

In the terminal window, in which the server is running, you should see

    $ ./tcp-server 8080
    peer ::ffff:127.0.0.1 (47920)  ->  local ::ffff:127.0.0.1 ( 8080)
    peer ::ffff:127.0.0.1 (47922)  ->  local ::ffff:127.0.0.1 ( 8080)
    peer ::ffff:192.168.178.26 (45114)  ->  local ::ffff:192.168.178.26 ( 8080)
    peer ::1 (47116)  ->  local ::1 ( 8080)
    peer 2001:aaaa:bbbb:0:cccc:eeee:eeee:ffff (48490)  ->  local 2001:aaaa:bbbb:0:cccc:eeee:eeee:ffff ( 8080)

The server prints a line for every connection. A line contains the IP address
and port of the connecting peer/client and then the local interface IP address
and port of the server.

Now use a second machine on your network to connect to the same server.

    other-host$ nc 192.168.178.26 8080

The server should print

    $ ./tcp-server 8080
    peer ::ffff:192.168.178.50 (42231)  ->  local ::ffff:192.168.178.26 ( 8080)

The address `192.168.178.50` should be the IP address of your *other-host*
machine. The IP address `192.168.178.26` is the IP address of the local
interface on your server machine.


### Try out the UDP example

Similar to the TCP example above. Start the server program:

    $ ./udp-server 8080
    # is running

In a second terminal window start netcat, type `test` and then press the enter
key:

    $ nc -u ::1 8080
    test<press enter>
    $

In the shell running the server program you can see now:

    $ ./udp-server 8080
    peer ::1 (47029)  ->  local ::1 ( 8080)
    Server 5 received bytes of data from the client.

The server program prints two lines for every connection. The first line
contains the peer/remote/client IP address and port and then the local
interface IP address and port. The second line is a diagnostic message showing
the count of bytes in the received UDP packet of the client. Here the client
has sent five bytes, four printable characters and a newline character.

After you tried an UDP connection on the localhost over the loopback interface,
you should try netcat with different local IP addresses (IPv4 and IPv6) and
from another machine on your network.


# License

The C source code in this repository is *unlicensed*. See the text in file
`UNLICENSE` for details. It means that you are free to do what every you want
with the source code (e.g. also use it in proprietary software), without any
obligations, but also without any warrenty of any kind. Again see the text in
file `UNLICENSE` for the exact terms. The code is intended to be copied&pasted
in your own applications.

[Unlicense Yourself: Set Your Code Free](https://unlicense.org/)


# Further reading

Documentation and interesting articles about networking:

    Title: ipv6 - Linux IPv6 protocol implementation
    $ man 7 ipv6
    http://man7.org/linux/man-pages/man7/ipv6.7.html

    Title: How TCP Sockets Work
    https://eklitzke.org/how-tcp-sockets-work

    Title: How TCP backlog works in Linux
    http://veithen.io/2014/01/01/how-tcp-backlog-works-in-linux.html
