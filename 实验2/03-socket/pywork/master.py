from socket import *
from threading import *
from json import *

novel_dir = "./war_and_peace.txt"
config_dir = "./workers.conf"
alphabet = {}
for char in range(ord('a'), ord('z') + 1):
    alphabet[chr(char)] = 0

with open(novel_dir, "r") as f:
    novel_len = len(f.read())

lock = Lock()


class Master(Thread):
    def __init__(self, client_no, client_ip, amount):
        Thread.__init__(self)
        self.client_no = client_no
        self.client_ip = client_ip
        self.amount = amount


    def run(self):
        client = socket(AF_INET, SOCK_STREAM)
        post = 12345
        host = self.client_ip
        client.connect((host, post))

        start = int(novel_len * (self.client_no / self.amount))
        end = int(novel_len * ((self.client_no + 1) / self.amount))

        # print(start, end)
        msg = dumps({
            'start': start,
            'end': end,
            'file_dir': novel_dir
        })
        # print(msg.encode())
        client.send(msg.encode())

        ans = loads(client.recv(1024))
        lock.acquire()
        for letter in ans.keys():
            alphabet[letter] += ans[letter]
        lock.release()

        client.close()


def master_begin():
    # client
    config_list = []
    with open(config_dir, "r") as config:
        for line in config:
            config_list.append(line.replace('\n', ''))

    n = len(config_list)

    masters = []
    try:
        for i in range(n):
            masters.append(Master(i, config_list[i], n))
    except:
        print("Failed to create a master thread")

    for master in masters:
        master.start()

    for master in masters:
        master.join()

    # print(alphabet)
    for k, v in alphabet.items():
        print(k, " ", v)
    

if __name__ == "__main__":
    master_begin()