from socket import *
from threading import *
from json import *

def worker_begin():
    # server
    server = socket(AF_INET, SOCK_STREAM)
    host = '0.0.0.0'
    post = 12345
    server.bind((host, post))

    server.listen(1)

    while True:
        client, addr = server.accept()
        print("Connection with: " + str(addr) + "is OK!")
        msg = loads(client.recv(1024))
        with open(msg['file_dir'], "r") as novel:
            raw_string = novel.read()[msg['start'] : msg['end']].lower()
        
        dic = {}
        for char in range(ord('a'), ord('z') + 1):
            dic[chr(char)] = raw_string.count(chr(char))

        cooked_string = dumps(dic).encode()
        print(cooked_string)
        client.send(cooked_string)
        client.close()


if __name__ == "__main__":
    worker_begin()