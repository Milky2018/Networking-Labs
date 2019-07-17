#!/usr/bin/python

import sys
import string
import socket
from time import sleep

input_data_name = 'client-input.dat'
output_data_name = 'server-output.dat'

def server(port):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    s.bind(('0.0.0.0', int(port)))
    s.listen(3)
    
    while True:
        cs, addr = s.accept()
        print(addr)
        file = open(output_data_name, mode='w+')
        data = cs.recv(1000)
        while data:
            file.write(data.decode('utf-8'))
            data = cs.recv(1000)

        file.close()
        cs.close()
        
    s.close()


def client(ip, port):
    s = socket.socket()
    s.connect((ip, int(port)))
    file = open(input_data_name, 'r')
    
    buf = file.read(500)
    while buf:
        s.send(buf.encode('utf-8'))
        buf = file.read(500)
    
    file.close()
    s.close()

if __name__ == '__main__':
    if sys.argv[1] == 'server':
        server(sys.argv[2])
    elif sys.argv[1] == 'client':
        client(sys.argv[2], sys.argv[3])
