#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static unsigned int src_data_bit(unsigned char* src, int byte1_idx, int bit1_idx, int shift_left)
{
//    int b = 8 - bit1_idx;
    int b = bit1_idx - 1;
    unsigned int r = (src[byte1_idx - 1] & (1 << b))?1:0;
    return r << shift_left;
};

static int process45(unsigned char* src, int idx)
{
    unsigned int X =
        src_data_bit(src, 4, 2, 0) |
        src_data_bit(src, 4, 4, 1) |
        src_data_bit(src, 4, 6, 2);

    unsigned int Y =
        src_data_bit(src, 4, 8, 0) |
        src_data_bit(src, 5, 2, 1) |
        src_data_bit(src, 5, 4, 2) |
        src_data_bit(src, 5, 6, 3) |
        src_data_bit(src, 5, 8, 4);

    if(!X) X = 8;

    if(X == 8 && Y == 31)
        return 0;

    printf("{%5d} [%.2X %.2X %.2X] X=%Xh/Y=%d\n",
        idx, src[0], src[1], src[2], X, Y);

    if(Y == 0)
    {
        /* ets_300706e01p.pdf: Page 27-28 */
        unsigned int Page_Units =
                src_data_bit(src, 6, 2, 0) |
                src_data_bit(src, 6, 4, 1) |
                src_data_bit(src, 6, 6, 2) |
                src_data_bit(src, 6, 8, 3);

        unsigned int Page_Tens =
                src_data_bit(src, 7, 2, 0) |
                src_data_bit(src, 7, 4, 1) |
                src_data_bit(src, 7, 6, 2) |
                src_data_bit(src, 7, 8, 3);

        unsigned int S1 =
                src_data_bit(src, 8, 2, 0) |
                src_data_bit(src, 8, 4, 1) |
                src_data_bit(src, 8, 6, 2) |
                src_data_bit(src, 8, 8, 3);

        unsigned int S2 =
                src_data_bit(src, 9, 2, 0) |
                src_data_bit(src, 9, 4, 1) |
                src_data_bit(src, 9, 6, 2);

        unsigned int S3 =
                src_data_bit(src, 10, 2, 0) |
                src_data_bit(src, 10, 4, 1) |
                src_data_bit(src, 10, 6, 2) |
                src_data_bit(src, 10, 8, 3);

        unsigned int S4 =
                src_data_bit(src, 11, 2, 0) |
                src_data_bit(src, 11, 4, 1);

        unsigned int C4 = src_data_bit(src, 9, 8, 0);
        unsigned int C5 = src_data_bit(src, 11, 6, 0);
        unsigned int C6 = src_data_bit(src, 11, 8, 0);
        unsigned int C7 =  src_data_bit(src, 12, 2, 0);
        unsigned int C8 =  src_data_bit(src, 12, 4, 0);
        unsigned int C9 =  src_data_bit(src, 12, 6, 0);
        unsigned int C10 = src_data_bit(src, 12, 8, 0);
        unsigned int C11 = src_data_bit(src, 13, 2, 0);
        unsigned int C12 = src_data_bit(src, 13, 4, 0);
        unsigned int C13 = src_data_bit(src, 13, 6, 0);
        unsigned int C14 = src_data_bit(src, 13, 8, 0);

        printf("    [%X%X%X]\n", X, Page_Tens, Page_Units);

        printf("    Page_Units=%Xh Page_Tens=%Xh\n",
            Page_Units, Page_Tens);
        printf("    S1=%X S2=%X S3=%X S4=%X\n",
            S1, S2, S3, S4);
        printf("    C4=%X C5=%X C6=%X C7=%X C8=%X C9=%X C10=%X C11=%X C12=%X C13=%X C14=%X\n",
            C4, C5, C6, C7, C8, C9, C10, C11, C12, C13, C14);

    }
    else if (Y > 0 && Y <= 23)
    {
        int i;

        // Page 77

        printf("    7[");
        for(i = 0; i < 40; i++)
            printf("%.2X ", src[5 + i] & 0x7F);
        printf("]\n");

        printf("    8[");
        for(i = 0; i < 40; i++)
            printf("%.2X ", src[5 + i]);
        printf("]\n");

        printf("    C[");
        for(i = 0; i < 40; i++)
            printf("%c", src[5 + i] & 0x7F);
        printf("]\n");

    }
    else if (Y == 31)
    {
        printf("    Independent data services.\n");

        int i;

        // Page 77

        printf("    7[");
        for(i = 0; i < 40; i++)
            printf("%.2X ", src[5 + i] & 0x7F);
        printf("]\n");

        printf("    8[");
        for(i = 0; i < 40; i++)
            printf("%.2X ", src[5 + i]);
        printf("]\n");
    };

    printf("\n");

    return 0;
};


#define BS 42

int main(int argc, char** argv)
{
    FILE* f;
    int skip = 0, cnt = 0;

    if(argc < 2)
    {
        fprintf(stderr, "Please specify sliced vbi data file\n");
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
        skip = atoi(argv[2]);
    };

    while(!feof(f))
    {
        int l = skip + BS;
        unsigned char temp[1024];

        memset(temp, 0, sizeof(temp));
        l = fread(temp, 1, skip + BS, f);

        if(l == (skip + BS))
        {
//            printf("\n%s:%d: sending packet %5d\n", __FUNCTION__, __LINE__, cnt++);

            process45(temp, cnt++);
        }
        else if (l)
            fprintf(stderr, "%s:%d: fread(stdin) %d != %d\n", __FUNCTION__, __LINE__, skip + BS, l);
    };

    fclose(f);

    return 0;
};
