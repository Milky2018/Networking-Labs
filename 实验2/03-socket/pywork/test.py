from socket import *
from threading import *
from json import *
import sys
import os

index = 1
amount = 3
novel_dir = "./war_and_peace.txt"
with open(novel_dir, "r") as f:
    novel = f.read().lower()

length = len(novel)
start = int(length * (index / amount))
end = int(length * ((index + 1) / amount))
dic = {}
for l in range(ord('a'), ord('z') + 1):
    dic[chr(l)] = novel[start : end].count(chr(l))

print(length)
# length2 = os.path.getsize(novel_dir)
# print(length, length2)