/*
 * Reserved console space.
 *
 * This was moved from the console code so that we can localize it
 * in one place.  This was necessary for dealing with the background
 * color, which we need to have for the hungapp drawing.  These are
 * stored in the extra-window-bytes of each console.
 */
#define GWL_CONSOLE_WNDALLOC  (3 * sizeof(DWORD))
#define GWL_CONSOLE_PID       0
#define GWL_CONSOLE_TID       4
#define GWL_CONSOLE_BKCOLOR   8