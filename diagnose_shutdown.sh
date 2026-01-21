#!/bin/bash
./build/bin/server_prototype -v > shutdown_test.log 2>&1 &
PID=$!
echo "Started server with PID $PID"
sleep 10
echo "Sending SIGINT to $PID"
kill -INT $PID
sleep 10
if ps -p $PID > /dev/null; then
    echo "Server still running after 10s. Sending SIGTERM."
    kill -TERM $PID
    sleep 5
fi

if ps -p $PID > /dev/null; then
    echo "FAILED: Server completely hung."
    kill -9 $PID
else
    echo "SUCCESS: Server exited."
fi

echo "--- LOG CONTENT ---"
cat shutdown_test.log
rm shutdown_test.log
