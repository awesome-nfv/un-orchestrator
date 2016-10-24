#! /bin/bash
#useful link:
#	http://www.cyberciti.biz/faq/howto-debian-ubutnu-set-default-gateway-ipaddress/

#Assign the ip address to the port usd by the dhcp server
ifconfig eth0 192.168.4.2/24

#start the DHCP server
service dnsmasq start
echo "DHCP service started"

#start the SSH server
#service ssh start
#echo "ssh service started"
