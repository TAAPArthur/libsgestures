#include <assert.h>
#include <stdlib.h>
#include <scutest/tester.h>
#include <signal.h>
#include <unistd.h>

#include <stdio.h>

#include "event.h"
#include "gestures-private.h"
#include "gestures.h"
#include "touch.h"
#include "writer.h"

SCUTEST_ERR(bad_path, 1) {
    const char* path = "/dev/null";
    startGestures(&path, 1, 1);
}

SCUTEST(udev_test) {
    signal(SIGALRM, stopGestures);
    alarm(1);
    startGestures(NULL, 0, 0);
}

SCUTEST(close_fd_test) {

    int fds[2];
    void func() {
        close(fds[0]);
    }
    int out = dup(STDOUT_FILENO);
    pipe(fds);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    signal(SIGALRM, func);
    alarm(1);
    startGestures(NULL, 0, 0);
    dup2(out, STDOUT_FILENO);
}
