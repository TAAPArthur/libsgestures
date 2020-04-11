#include <sgestures/gestures.h>
#include <thread>

static GestureEvent* currentEvent;
void processGestures() {
    while(true) {
        currentEvent = waitForNextGesture();
        triggerGestureBinding(*currentEvent);
        free(currentEvent);
    }
}

bool isTouchEventReady(int32_t fd);
void readGestures() {
    while(true) {
        readTouchEvent(0);
    }
}

int main() {
    listenForGestureEvents(-1);
    std::thread thread(readGestures);
    thread.detach();
    processGestures();
    return 0;
}
