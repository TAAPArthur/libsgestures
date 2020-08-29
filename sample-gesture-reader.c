#include <sgestures/event.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* const argv[]) {
    GestureMask mask = argc > 1 ?  atoi(argv[1]): GestureEndMask;
    listenForGestureEvents(mask);
    while(true) {
        readTouchEvent(0);
        GestureEvent* event;
        while(event = getNextGesture()) {
            printf("%s", getGestureMaskString(event->flags.mask));
            for(int i = 0; i< getNumOfTypes(event->detail); i++) {
                printf(" %s", getGestureTypeString(getGestureType(event->detail, i)));
            }
            printf("\n");
            free(event);
        }
    }
    return 0;
}
