#!/bin/bash
eth=enp4s0f1 

nmcli dev set $eth managed no
sudo ip link set up dev $eth
sudo ip addr add 192.168.97.2/24 dev $eth
