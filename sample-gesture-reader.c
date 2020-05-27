#include <sgestures/event.h>

#include <stdio.h>
#include <stdlib.h>

int main() {
    listenForGestureEvents(-1);
    while(true) {
        readTouchEvent(0);
        GestureEvent* event = getNextGesture();
        if(event) {
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
