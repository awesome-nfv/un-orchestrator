#! /bin/bash

#This gives time to the UN to configure the network namespace of the container
sleep 3

#To change the rules of the firewall, edit the next rows and/or add new rows
brctl addbr br0
brctl addif br0 eth0
brctl addif br0 eth1
ifconfig br0 up

brctl show br0

iptables -I FORWARD -m physdev --physdev-in eth0 --physdev-out eth1 -d 8.8.8.8 -j DROP

echo "Firewall started"

while true
do
	sleep 1
done

