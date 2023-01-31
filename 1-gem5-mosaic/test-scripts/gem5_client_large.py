from telnetlib import Telnet
import sys

username = b"root\n"
password = b"s\n"

server = 'localhost'
#port = 3456
port = int(sys.argv[1])
appname = str(sys.argv[2])

if appname == "graph500":
    commands = [b"/m5 exit\r\n", b"/seq-list -s 15 -e 15\r\n", b"/m5 exit\r\n" ]
elif appname == "hello":
    commands = [b"/m5 exit\r\n", b"/hello 15\r\n", b"/m5 exit\r\n" ]
elif appname == "xsbench":
    commands = [b"/m5 exit\r\n", b"/XSBench -t 1 -g 2000 -p 40000\r\n", b"/m5 exit\r\n" ]
elif appname == "btree":
    commands = [b"/m5 exit\r\n", b"/BTree 70000000 100\r\n", b"/m5 exit\r\n" ]
elif appname == "gups":
    commands = [b"/m5 exit\r\n", b"/gups 15\r\n", b"/m5 exit\r\n" ]
else:
    commands = [b"/m5 exit\r\n", b"/seq-list -s 15 -e 15\r\n", b"/m5 exit\r\n" ]

with Telnet(server, port) as tn:
    tn.read_until(b"login: ")
    tn.write(username)
    tn.read_until(b"Password: ")
    tn.write(password)
    for c in commands:
        tn.write(c)
