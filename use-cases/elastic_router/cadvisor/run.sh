#!/bin/sh
if [ -z ${PORT+x} ];
then cport=8080
else cport=$PORT
fi
if [ -z ${NAME+x} ];
then cname=cadvproxy
else cname=$NAME
fi
if [ -z ${KEY+x} ];
then ckey=a
else ckey=$KEY
fi
/usr/bin/cadvisor -port=$cport -logtostderr &
while : ; do
  if curl --fail -X GET "127.0.0.1:$cport"; then
      break;
  fi
  echo "."
  sleep 1;
done
echo "Starting DoubleDecker proxy" 
echo "python3 main.py $cname -k /keys/$ckey -p $cport"
python3 main.py $cname -k /keys/$ckey -p $cport -d $DEALER_PORT
