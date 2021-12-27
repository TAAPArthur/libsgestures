#include <sgestures/event.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* const argv[]) {
    GestureMask mask = argc > 1 ?  atoi(argv[1]) : GestureEndMask;
    listenForGestureEvents(mask);
    while(readTouchEvent(STDIN_FILENO) > 0);
    return 0;
}
