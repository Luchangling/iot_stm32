#!/bin/sh

res=`ps aux|grep java|grep AgnssClient|grep -v grep|awk '{print $2}'`

if [ -n "$res" ]
then
echo "AgnssClient is running"
else
echo "AgnssClient is starting"
#ע�⣺������&�����ִ̨�У�����û��pid����
java -jar /home/agps/bin/AgnssClient.jar &    

# ��jar��������Ӧ��pidд���ļ��У�Ϊֹͣʱ�ṩpid   
echo $! > /var/run/AgnssClient.pid  
echo "AgnssClient start ok"
fi