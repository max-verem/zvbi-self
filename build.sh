#!/bin/bash

#gcc -Wall -O0 -g -ggdb -o demo4_42 demo4.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi
#./demo4_42 dvbteletext.ts.telxcc 3 > dvbteletext.ts.telxcc.pages
#./demo4_42 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages

gcc -Wall -O0 -g -ggdb -o demo5 demo5.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi

#./demo5 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages2
