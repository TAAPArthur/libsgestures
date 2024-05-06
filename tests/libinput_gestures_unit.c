#define SCUTEST_DEFINE_MAIN
#define SCUTEST_IMPLEMENTATION
#include "scutest.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../event.h"
#include "../gestures-private.h"
#include "../gestures.h"
#include "../touch.h"
#include "../writer.h"

/**
 * Starting listening for and processing gestures.
 * One thread is spawned to listen for libinput TouchEvents and convert them into our custom GestureEvents
 * Another thread processes these GestureEvents and triggers GestureBindings
 *
 * @param paths only listen for these paths
 * @param num length of paths array
 * @param grab rather to get an exclusive grab on the device
 */
void startGestures(const char** paths, int num, bool grab);
void stopGestures();

SCUTEST_ERR(bad_path, 1) {
    const char* path = "/dev/null";
    startGestures(&path, 1, 1);
}

SCUTEST(udev_test) {
    signal(SIGALRM, stopGestures);
    alarm(1);
    startGestures(NULL, 0, 0);
}
static int fds[2];
static void func() {
    close(fds[0]);
}

SCUTEST(close_fd_test) {
    int out = dup(STDOUT_FILENO);
    pipe(fds);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    signal(SIGALRM, func);
    alarm(1);
    startGestures(NULL, 0, 0);
    dup2(out, STDOUT_FILENO);
}
