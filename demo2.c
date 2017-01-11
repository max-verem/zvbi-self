#include <stdio.h>

#include <libzvbi.h>

static void vbi_event_proc(vbi_event *ev, void *user_data)
{
    fprintf(stderr, "%s:%d: here\n", __FUNCTION__, __LINE__);
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
    int r;
    double t = 0.0;
    vbi_decoder *vbi;
    vbi_sliced sliced;

    vbi = vbi_decoder_new();

    r = vbi_event_handler_register(vbi, ~0, vbi_event_proc, NULL);

    while(!feof(stdin))
    {
        sliced.id = VBI_SLICED_TELETEXT_B;
        sliced.line = 0;
        fread(sliced.data, 1, 45, stdin);
        vbi_decode(vbi, &sliced, 1, t);
        fprintf(stderr, "%s:%d: sent\n", __FUNCTION__, __LINE__);
        t += 0.04;
    };

    r = vbi_event_handler_register(vbi, 0, vbi_event_proc, NULL);

    vbi_decoder_delete(vbi);

    return 0;
};
