#!/bin/bash

for N in {1..50}
do
    ./client 127.0.0.1 9898 $(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 16 | head -n 1) &
done
wait