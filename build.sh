#!/bin/bash

gcc -Wall -O0 -g -ggdb -o demo7 demo7.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi
./demo7 Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext 3 > Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.data7 2>Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.data72
./demo7 dvbteletext.ts.telxcc 3 > dvbteletext.ts.telxcc.data72
./demo7 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.telxcc.data72
exit 0

gcc -Wall -O0 -g -ggdb -o demo4_42 demo4.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi
./demo4_42 dvbteletext.ts.telxcc 3 > dvbteletext.ts.telxcc.pages
./demo4_42 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages

gcc -Wall -O0 -g -ggdb -o demo5 demo5.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi
./demo5 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages2

gcc -Wall -O0 -g -ggdb -o demo6 demo6.c -L/usr/local/zvbi/lib -I/usr/local/zvbi/include -lzvbi
./demo6 Omneon_PCM_OP47_Mentalist.mxf.vanc Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext > Omneon_PCM_OP47_Mentalist.mxf.vanc.dump
./demo6 C0001.MXF.vanc > C0001.MXF.vanc.dump
./demo6 XDCAM608708.mxf.vanc > XDCAM608708.mxf.vanc.dump
./demo4_42 Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext 3 >Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.pages1 2>Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.pages2
./demo5 Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext 3 >Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.pages3 2>Omneon_PCM_OP47_Mentalist.mxf.vanc.teletext.pages4

