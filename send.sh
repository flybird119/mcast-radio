#!/bin/bash

sox -S "$1" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -r -L $((44100*4)) | ./nadajnik -a 239.10.11.12 -n "Wysylacz"

