#!/bin/sh

res=`ps aux|grep java|grep AgnssClient|grep -v grep|awk '{print $2}'`

if [ -n "$res" ]
then
echo "AgnssClient is running"
else
echo "AgnssClient is starting"
#注意：必须有&让其后台执行，否则没有pid生成
java -jar /home/agps/bin/AgnssClient.jar &    

# 将jar包启动对应的pid写入文件中，为停止时提供pid   
echo $! > /var/run/AgnssClient.pid  
echo "AgnssClient start ok"
fi