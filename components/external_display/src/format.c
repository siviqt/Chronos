#include <stdio.h>
#include "external_display.h"
const char* external_status_text(external_status_t s) {
    static const char* const names[] = {"Disabled",
                                        "Initializing",
                                        "Not detected - select Nokia manually",
                                        "Nokia initialized - connection unverified",
                                        "Connected - VFD",
                                        "Configuration error",
                                        "Initialization failed",
                                        "Communication error"};
    return (unsigned)s < sizeof(names) / sizeof(*names) ? names[s] : "Unavailable";
}
void external_vfd_format(const external_frame_t* f, char text[13]) {
    const char* prefix = f->source == EXT_STOPWATCH   ? "SW "
                         : f->source == EXT_COUNTDOWN ? "TM "
                         : f->source == EXT_PACER     ? "PC "
                         : f->source == EXT_ALARMS    ? "AL "
                                                      : "";
    snprintf(text, 13, "%s%.*s", prefix, 12 - (int)strlen(prefix), f->value);
    if (f->source == EXT_STOPWATCH || f->source == EXT_COUNTDOWN) {
        unsigned minutes, seconds, tenths;
        int      end = 0;
        /* Whole seconds avoid the tube's almost invisible decimal glyph.
         * Four minute digits still leave column 12 free for the state. */
        if (sscanf(f->value, "%4u:%2u.%1u%n", &minutes, &seconds, &tenths, &end) == 3 &&
            f->value[end] == '\0' && minutes <= 9999 && seconds < 60)
            snprintf(text, 13, "%s%02uM%02uS", prefix, minutes, seconds);
    }
    if (f->source == EXT_WORLD && f->value[2] == ':' && f->value[5] == ':') {
        if (strlen(f->value) > 8)
            snprintf(text, 13, "%.3s %.5s %.2s", f->title, f->value, f->value + 9);
        else
            snprintf(text, 13, "%.3s %.8s", f->title, f->value);
    }
    size_t n = strlen(text);
    memset(text + n, ' ', 12 - n);
    text[12] = 0;
    /* Preserve a compact state indicator even on a single-line display. */
    if (f->source == EXT_STOPWATCH || f->source == EXT_COUNTDOWN || f->source == EXT_PACER) {
        char marker = !strcmp(f->detail, "Running")    ? 'R'
                      : !strcmp(f->detail, "Paused")   ? 'P'
                      : !strcmp(f->detail, "Complete") ? 'C'
                      : !strcmp(f->detail, "Stopped")  ? 'S'
                      : !strcmp(f->detail, "Ready")    ? 'I'
                                                       : 0;
        if (marker) text[11] = marker;
    }
    /* The tube renders ':' as full-width bars, not clock dots. Keep the
     * semantic frame intact for the LCD/Nokia and adapt only the VFD text. */
    for (unsigned i = 0; i < 12; ++i) {
        if (text[i] == ':') text[i] = ' ';
        if ((unsigned char)text[i] < 32 || (unsigned char)text[i] > 126) text[i] = '?';
    }
}
