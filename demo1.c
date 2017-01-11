#include <stdio.h>
#include <string.h>

#include <libzvbi.h>

#include "demo1.h"

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

int main(int argc, char** argv)
{
    int r, i;
    double t = 0.0;
    vbi_decoder *vbi;

    vbi = vbi_decoder_new();

    r = vbi_event_handler_register(vbi, ~0, vbi_event_proc, NULL);
    fprintf(stderr, "%s:%d: vbi_event_handler_register=%d\n", __FUNCTION__, __LINE__, r);

    for(i = 0; demo_data[i * demo_stride] != 0xFF; i++)
    {
        vbi_sliced sliced;

        sliced.id = VBI_SLICED_TELETEXT_B;
        sliced.line = 0;
        memcpy(sliced.data, demo_data + i * demo_stride + 2, demo_stride - 2);
        vbi_decode(vbi, &sliced, 1, t);
        fprintf(stderr, "%s:%d: sent packet %5d\n", __FUNCTION__, __LINE__, i);
        t += 0.04;
    };

    r = vbi_event_handler_register(vbi, 0, vbi_event_proc, NULL);

    vbi_decoder_delete(vbi);

    return 0;
};
