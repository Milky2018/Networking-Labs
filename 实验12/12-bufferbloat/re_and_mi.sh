sudo python2 reproduce_bufferbloat.py -q 20
sudo python2 reproduce_bufferbloat.py -q 50
sudo python2 reproduce_bufferbloat.py -q 100
sudo python2 reproduce_bufferbloat.py -q 150
sudo python2 mitigate_bufferbloat.py -a taildrop
sudo python2 mitigate_bufferbloat.py -a red
sudo python2 mitigate_bufferbloat.py -a codel
python plot.py