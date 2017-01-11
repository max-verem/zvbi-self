#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libzvbi.h>

static void vbi_event_proc(vbi_event *ev, void *user_data)
{
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
    int r, skip = 0, cnt = 0;
    double t = 0.0;
    vbi_decoder *vbi;

    if(argc > 1)
    {
        skip = atoi(argv[1]);
    };

    vbi = vbi_decoder_new();

    r = vbi_event_handler_register(vbi, ~0, vbi_event_proc, NULL);
    fprintf(stderr, "%s:%d: vbi_event_handler_register=%d\n", __FUNCTION__, __LINE__, r);

    while(!feof(stdin))
    {
        int l = skip + BS;
        unsigned char temp[1024];

        memset(temp, 0, sizeof(temp));
        l = fread(temp, 1, skip + BS, stdin);

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

    r = vbi_event_handler_register(vbi, 0, vbi_event_proc, NULL);

    vbi_decoder_delete(vbi);

    return 0;
};
