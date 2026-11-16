#include "ui/banner_text.h"
#include <stdio.h>

void banner_headline(char *buf, size_t size, bool have_data, int waiting, int working, int done) {
    if (!buf || size == 0) return;
    if (!have_data)
        snprintf(buf, size, "Waiting for the PC");
    else if (waiting > 0)
        snprintf(buf, size, waiting == 1 ? "%d chat needs you" : "%d chats need you", waiting);
    else if (working > 0)
        snprintf(buf, size, "%d chat%s working", working, working == 1 ? "" : "s");
    else if (done > 0)
        snprintf(buf, size, "%d finished chat%s", done, done == 1 ? "" : "s");
    else
        snprintf(buf, size, "All quiet");
}

talpy_mood_t banner_mood(bool have_data, int waiting, int working, int done) {
    (void)done;
    if (!have_data) return TALPY_LOADING;
    if (waiting > 0) return TALPY_NEEDS;
    if (working > 0) return TALPY_WORKING;
    return TALPY_DONE;
}
