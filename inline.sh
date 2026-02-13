#!/bin/bash

stty -echo -icanon
socat TCP-LISTEN:4444,reuseaddr SYSTEM:'(./multiplex config-mult.txt; nc -N localhost 4440)',nofork 2>/dev/null &
make run-vnc > /dev/null 2>&1 &
pid=$!
trap "kill $pid; stty echo icanon" EXIT TERM INT QUIT
# stderr and stdout for multiplexed UART
nc -lp 4441 >&2 < /dev/null &
nc -lp 4440
# terminal for plain mode UART
nc -lp 4440
exit 0
