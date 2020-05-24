#!/bin/sh
PID=$(cat /var/run/AgnssClient.pid)
kill -9 $PID