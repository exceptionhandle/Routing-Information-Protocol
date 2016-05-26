#!/bin/sh

hexdump -e '4/1 "%5u "' -e '"\n"' response.pkt 
