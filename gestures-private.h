#ifndef GESTURES_PRIVATE_H
#define GESTURES_PRIVATE_H

#define LEN(X) (sizeof X / sizeof X[0])


#define ADD_POINT(dest, delta) do{dest.x+=delta.x; dest.y+=delta.y;}while(0)
#define DIVIDE_POINT(P, N) do{P.x/=N; P.y/=N;}while(0)

/// The consecutive points within this distance are considered the same and not double counted
#define THRESHOLD_SQ (256)
#ifndef DEBUG
#define MIN_LINE_LEN (256)
#else
#define MIN_LINE_LEN (1)
#endif
/// All seat of a gesture have to start/end within this sq distance of each other
#define PINCH_THRESHOLD_PERCENT .4
/// The cutoff for when a sequence of points forms a line
#define R_SQUARED_THRESHOLD .5

#endif
