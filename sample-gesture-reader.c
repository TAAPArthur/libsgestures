#include <sgestures/event.h>

#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* const argv[]) {
    GestureMask mask = argc > 1 ?  atoi(argv[1]) : GestureEndMask;
    listenForGestureEvents(mask);
    while(readTouchEvent(STDIN_FILENO) > 0);
    return 0;
}
