#!/bin/bash
for i in {1..10}
do
   echo "starting client $i"
   nc localhost 9000
done