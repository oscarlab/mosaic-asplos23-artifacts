from telnetlib import Telnet
import sys
import time

username = b"root\n"
password = b"s\n"

server = 'localhost'
#port = 3456
port = int(sys.argv[1])

time.sleep(10)
#gups command
commands = [b"/m5 exit\r\n", b"/seq-list  -s 15 -e 15\r\n", b"/m5 exit\r\n" ]

with Telnet(server, port) as tn:
    tn.read_until(b"login: ")
    tn.write(username)
    tn.read_until(b"Password: ")
    tn.write(password)
    for c in commands:
        tn.write(c)
