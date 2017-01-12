#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static const char* get_wrapping_type(int idx)
{
    switch(idx)
    {
        case 1: return "Frame (Interlaced or progressive segmented frame)";
        case 2: return "Field 1";
        case 3: return "Field 2";
        case 4: return "Progressive frame";
    };

    return "<unknown>";
};

static const char* get_payload_sample_coding(int idx)
{
    switch(idx)
    {
        case 1: return "1-bit component luma samples";
        case 2: return "1-bit component color difference samples";
        case 3: return "1-bit component luma and color difference samples";
        case 4: return "8-bit component luma samples";
        case 5: return "8-bit component color difference samples";
        case 6: return "8-bit component luma and color difference samples";
        case 7: return "10-bit component luma samples";
        case 8: return "10-bit component color difference samples";
        case 9: return "10-bit component luma and color difference samples";
        case 10: return "Reserved";
        case 11: return "Reserved";
        case 12: return "Reserved";
    };

    return "<unknown>";
};

static uint64_t r_8(FILE* f)
{
    uint32_t r = 0;
    fread(&r, 1, 1, f);
    return r;
};


static uint64_t r_16be(FILE* f)
{
    uint64_t r1 = r_8(f);
    uint64_t r2 = r_8(f);

    return (r1 << 8) | (r2 << 0);
};

static uint64_t r_32be(FILE* f)
{
    uint64_t r1 = r_8(f);
    uint64_t r2 = r_8(f);
    uint64_t r3 = r_8(f);
    uint64_t r4 = r_8(f);

    return (r1 << 24) | (r2 << 16) | (r3 << 8) | (r4 << 0);
};

static uint16_t parity8_calc(uint16_t s)
{
    uint16_t i, c, m;

    for(i = 0, c = 0, m = 1; i < 8; i++, m <<= 1)
        c ^= (s & m) ? 1 : 0;

    return c;
};

int main(int argc, char** argv)
{
    FILE *f, *f_teletext = NULL;
    int cnt = 0;

    if(argc < 2)
    {
        fprintf(stderr, "Please specify vbi_vanc_smpte_436M data file\n");
        return 1;
    };

    if(!strcmp("-", argv[1]))
        f = stdin;
    else
        f = fopen(argv[1], "rb");
    if(!f)
    {
        fprintf(stderr, "Failed to open [%s]\n", argv[1]);
        return 1;
    };

    if(argc > 2)
    {
        f_teletext = fopen(argv[2], "wb");
    };

    while(!feof(f))
    {
        int l, number_of_lines;

        number_of_lines = r_16be(f);

        printf("[%5d] Frame data %d vbi lines\n", cnt++, number_of_lines);

        for(l = 0; l < number_of_lines; l++)
        {
            uint16_t o8[1024], o10[1024], o16[1024];
            int p, o, line_no, wrapping_type, payload_sample_coding, payload_sample_count, payload_sample_count2, unknown1, unknown2;

            printf("    [%3d] line (0x%.8lX)\n", l, ftell(f));

            line_no = r_16be(f);
            wrapping_type = r_8(f);
            payload_sample_coding = r_8(f);
            payload_sample_count = r_16be(f);
            unknown1 = r_32be(f);
            unknown2 = r_32be(f);
            payload_sample_count2 = (payload_sample_count + 3) & ~3;

            for(p = 0, o = 0; p < payload_sample_count2; p++)
            {
                o8[p] = r_8(f);

                o16[p] = o8[p] | (parity8_calc(o8[p]) << 8) | ((!parity8_calc(o8[p])) << 9);

                if(!(p % 4))
                {
                };
            };

            printf("    Line Number: %d\n", line_no);
            printf("    Wrapping Type: %d (%s)\n", wrapping_type, get_wrapping_type(wrapping_type));
            printf("    Payload Sample Coding: %d (%s)\n", payload_sample_coding, get_payload_sample_coding(payload_sample_coding));
            printf("    Payload Sample Count: %d (padded is %d)\n", payload_sample_count, payload_sample_count2);
            printf("    Unknown1: 0x%.4X (%d)\n", unknown1, unknown1);
            printf("    Unknown2: 0x%.4X (%d)\n", unknown2, unknown2);

            // dump 8-bits original
            printf("    Data:\n" "       ");
            for(p = 0; p < payload_sample_count2; p++)
                printf("  %.2X", o8[p]);
            printf("\n");

            // dump 10-bits extended frop 8-bits
            printf("    Data (with parities):\n" "       ");
            for(p = 0; p < payload_sample_count2; p++)
                printf(" %.3X", o16[p]);
            printf("\n");

            if(0x43 == o8[0] && 0x02 == o8[1])
            {
                int p, o, q;

                printf("    [Subtitling distribution package (SDP) - OP47 Free TV Australia]\n");

                printf("    HEADER:\n");
                printf("        DID: %.2Xh (%.3Xh)\n",          o8[0], o16[0]);
                printf("        SDID: %.2Xh (%.3Xh)\n",         o8[1], o16[1]);
                printf("        DC: %.2Xh (%.3Xh) (%d)\n",      o8[2], o16[2], o8[2]);

                printf("    UDW:\n");
                printf("        IDENTIFIER: %.2Xh (%.3Xh)\n",   o8[3], o16[3]);
                printf("        IDENTIFIER: %.2Xh (%.3Xh)\n",   o8[4], o16[4]);
                printf("        LENGTH: %.2Xh (%.3Xh) (%d)\n",  o8[5], o16[5], o8[5]);
                printf("        FORMAT CODE: %.2Xh (%.3Xh)\n",   o8[6], o16[6]);
#define VBI_Packet_1_Descriptor(I) \
                if(o8[I]) \
                { \
                    printf("            LINE NUMBER: %d\n",   o8[I] & 31); \
                    printf("            FIELD No: %d\n",   (o8[I] & 80)?1:0); \
                }
                printf("        VBI Packet 1 Descriptor: %.2Xh (%.3Xh)\n",   o8[7], o16[7]);
                VBI_Packet_1_Descriptor(7);
                printf("        VBI Packet 2 Descriptor: %.2Xh (%.3Xh)\n",   o8[8], o16[8]);
                VBI_Packet_1_Descriptor(8);
                printf("        VBI Packet 3 Descriptor: %.2Xh (%.3Xh)\n",   o8[9], o16[9]);
                VBI_Packet_1_Descriptor(9);
                printf("        VBI Packet 4 Descriptor: %.2Xh (%.3Xh)\n",   o8[10], o16[10]);
                VBI_Packet_1_Descriptor(10);
                printf("        VBI Packet 5 Descriptor: %.2Xh (%.3Xh)\n",   o8[11], o16[11]);
                VBI_Packet_1_Descriptor(11);

                for(p = 0, o = 12; p < 5; p++)
                {
                    if(!o8[7 + p])
                        continue;

                    printf("        VBI Packet %d Data:\n            ",   p + 1);

                    for(q = 0; q < 45; q++)
                    {
                        if(f_teletext)
                            fwrite(o8 + o, 1, 1, f_teletext);

                        printf("  %.2X", o8[o++]);
                    };

                    printf("\n");
                };

                printf("        FOOTER ID: %.2Xh (%.3Xh)\n",   o8[o], o16[o]); o++;
                printf("        FSC: %.2Xh (%.3Xh)\n",   o8[o], o16[o]); o++;
                printf("        FSC: %.2Xh (%.3Xh)\n",   o8[o], o16[o]); o++;
                printf("        SDP CHECKSUM: %.2Xh (%.3Xh)\n",   o8[o], o16[o]); o++;
                printf("        {%d}\n", o);
            }
            else if(0x61 == o8[0] && 0x01 == o8[1])
            {
                printf("    [EIA 708D Data mapping into VANC space]\n");
            }
            else
                printf("    [UNKNOWN VANC]\n");

            printf("\n");
        };

//        return 0;
    };

    fclose(f);

    if(f_teletext)
        fclose(f_teletext);

    return 0;
};
