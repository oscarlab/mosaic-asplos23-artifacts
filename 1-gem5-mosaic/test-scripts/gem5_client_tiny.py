from telnetlib import Telnet
import sys

username = b"root\n"
password = b"s\n"

server = 'localhost'
#port = 3456
port = int(sys.argv[1])
appname = str(sys.argv[2])


#gups command
#commands = [b"/m5 exit\r\n", b"/hello 15\r\n", b"/m5 exit\r\n" ]
#graph500 command
#commands = [b"/m5 exit\r\n", b"./Graph500/seq-list/seq-list -s 15 -e 15\r\n", b"/m5 exit\r\n" ]

commands = [b"/m5 exit\r\n", b"/seq-list -s 4 -e 4\r\n", b"/m5 exit\r\n" ]

if appname == "graph500":
    commands = [b"/m5 exit\r\n", b"/seq-list -s 4 -e 4\r\n", b"/m5 exit\r\n" ]
elif appname == "hello":
    commands = [b"/m5 exit\r\n", b"/hello 15\r\n", b"/m5 exit\r\n" ]
elif appname == "xsbench":
    commands = [b"/m5 exit\r\n", b"/XSBench -t 1 -g 20 -p 4\r\n", b"/m5 exit\r\n" ]
elif appname == "btree":
    commands = [b"/m5 exit\r\n", b"/BTree 1 1\r\n", b"/m5 exit\r\n" ]
elif appname == "gups":
    commands = [b"/m5 exit\r\n", b"/gups 1\r\n", b"/m5 exit\r\n" ]
else:
    commands = [b"/m5 exit\r\n", b"/seq-list -s 4 -e 4\r\n", b"/m5 exit\r\n" ]

#print(commands)

with Telnet(server, port) as tn:
    tn.read_until(b"login: ")
    tn.write(username)
    tn.read_until(b"Password: ")
    tn.write(password)
    for c in commands:
        tn.write(c)
