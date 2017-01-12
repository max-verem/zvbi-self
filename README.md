# zvbi-self

This set created for testing VBI data decoding before creating Teletext encoder.

## demo4.c

Basic VBI decoder using ZVBI. It accept sliced VBI data file (42 bytes chunks) for input. For example:

    ./demo4_42 dvbteletext.ts.telxcc 3 > dvbteletext.ts.telxcc.pages

    ./demo4_42 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages

where *3* is a number of bytes to skip. Files dvbteletext.ts.telxcc and teletextsubtitles.ts.telxcc has 45 bytes chunks. Files .telxcc are built by dumping data in [telxcc](https://github.com/petrkutalek/telxcc/releases) utility.

## demo5.c

Another one basic VBI decoder (Teletext subtitles based on a code from [telxcc](https://github.com/petrkutalek/telxcc/releases). Usage example:

    ./demo5 teletextsubtitles.ts.telxcc 3 > teletextsubtitles.ts.pages2

