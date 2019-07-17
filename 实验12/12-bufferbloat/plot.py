# Most content of this file is copied form JF-D.
import matplotlib.pyplot as plt

###################################

qlen_list = [20, 50, 100, 150]
qlen_dir = ['qlen-20', 'qlen-50', 'qlen-100', 'qlen-150']

for qlen in qlen_list:
    with open('qlen-' + str(qlen) + '/cwnd.txt') as file:
        line = file.readline()
        x, y = [], []
        while line:
            if line.split()[1].split(':')[0] == '10.0.1.11':
                y.append(int(line.split()[6]))
                x.append(float(line.split()[0]))
            line = file.readline()
        plt.plot(x, y, label='qlen=' + str(qlen))

plt.ylabel('CWND(KB)')
plt.legend()
plt.savefig('qlen-crowd.png')
plt.show()

########################################
for qlen in qlen_list:
    with open('qlen-' + str(qlen) + '/qlen.txt') as file:
        line = file.readline()
        x, y = [], []
        while line:
            if len(line.split()) > 1:
                x.append(float(line.split()[0].split(',')[0]))
                y.append(int(line.split()[1]))
            line = file.readline()
    
        for i in range(1, len(x)):
            x[i] = x[i] - x[0]
        x[0] = 0.0
        plt.plot(x, y, label='qlen=' + str(qlen))

plt.ylabel('QLen: #(Packages)')
plt.legend()
plt.savefig('qlen-QLen.png')
plt.show()

########################################
for qlen in qlen_list:
    with open('qlen-' + str(qlen) + '/ping.txt') as file:
        line = file.readline()
        line = file.readline()
        x, y = [], []
        while line:
            y.append(float(line.split()[6].split('=')[1]))
            line = file.readline()
        x = list(range(len(y)))
        plt.plot(x, y, label='qlen=' + str(qlen))

plt.ylabel('RTT(ms)')
plt.legend()
plt.savefig('qlen-RTT.png')
plt.show()

########################################
scheme_list = ['taildrop', 'red', 'codel']
for scheme in scheme_list:
    with open(scheme + '/ping.txt') as file:
        line = file.readline()
        line = file.readline()
        x, y = [], []
        while line:
            y.append(float(line.split()[6].split('=')[1]))
            line = file.readline()
        x = list(range(len(y)))
        plt.plot(x, y, label=scheme)

plt.ylabel('RTT(ms)')
plt.legend()
plt.savefig('solve.png')
plt.show()