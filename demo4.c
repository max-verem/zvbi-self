#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libzvbi.h>

static void vbi_event_proc(vbi_event *ev, void *user_data)
{
    if(VBI_EVENT_TTX_PAGE == ev->type)
    {
        vbi_page page;
        vbi_decoder *vbi = (vbi_decoder *)user_data;

        fprintf(stderr, "%s:%d: ev->type=VBI_EVENT_TTX_PAGE\n"
            "\tev.ttx_page.pgno=%d (0x%X)\n"
            "\tev.ttx_page.subno=%d\n"
            "\tev.ttx_page.pn_offset=%d\n"
            "\tev.ttx_page.roll_header=%d\n"
            "\tev.ttx_page.header_update=%d\n"
            "\tev.ttx_page.clock_update=%d\n"
            ,__FUNCTION__, __LINE__,

            ev->ev.ttx_page.pgno, ev->ev.ttx_page.pgno,
            ev->ev.ttx_page.subno,
            ev->ev.ttx_page.pn_offset,
            ev->ev.ttx_page.roll_header,
            ev->ev.ttx_page.header_update,
            ev->ev.ttx_page.clock_update
        );

        if(!vbi_fetch_vt_page(vbi, &page, ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno, VBI_WST_LEVEL_1, 25, 0))
            fprintf(stderr, "%s:%d: vbi_fetch_vt_page failed\n",
                __FUNCTION__, __LINE__);
        else
        {
            char buf[30 * 80 * 2];

            memset(buf, 0, sizeof(buf));

            vbi_print_page(&page, buf, sizeof(buf), "utf-8", 1, 1);

            printf("-----------------------------------------\n");
            printf("%s\n", buf);
            printf("-----------------------------------------\n");

            vbi_unref_page(&page);
        };
    }
    else
        fprintf(stderr, "%s:%d: ev->type=%d\n", __FUNCTION__, __LINE__, ev->type);

#if 0
    const vbi_program_id *pid;
    user_data = user_data; /* unused, no warning please */
    pid = ev->ev.prog_id;
    printf ("Received PIL %s/%02X on LC %u.\n",
        pil_str (pid->pil), pid->pty, pid->channel);
#endif
}

#define BS 42

int main(int argc, char** argv)
{
    FILE* f;
    int r, skip = 0, cnt = 0;
    double t = 0.0;
    vbi_decoder *vbi;

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

    vbi = vbi_decoder_new();

    r = vbi_event_handler_register(vbi, ~0, vbi_event_proc, vbi);
    fprintf(stderr, "%s:%d: vbi_event_handler_register=%d\n", __FUNCTION__, __LINE__, r);

    while(!feof(f))
    {
        int l = skip + BS;
        unsigned char temp[1024];

        memset(temp, 0, sizeof(temp));
        l = fread(temp, 1, skip + BS, f);

        if(l == (skip + BS))
        {
            vbi_sliced sliced;

            sliced.id = VBI_SLICED_TELETEXT_B;
            sliced.line = 0;
            memcpy(sliced.data, temp + skip, BS);

            vbi_decode(vbi, &sliced, 1, t);
            fprintf(stderr, "%s:%d: sent packet %5d\n", __FUNCTION__, __LINE__, cnt++);
            t += 0.04;
        }
        else if (l)
            fprintf(stderr, "%s:%d: fread(stdin) %d != %d\n", __FUNCTION__, __LINE__, skip + BS, l);
    };

    fclose(f);

    r = vbi_event_handler_register(vbi, 0, vbi_event_proc, NULL);

    vbi_decoder_delete(vbi);

    return 0;
};
