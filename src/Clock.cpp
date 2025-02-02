#include "Clock.hpp"

extern "C"
{
#include <libavutil/mathematics.h>
#include <libavutil/time.h>
}

void Clock::init(int* queue_serial)
{
    _speed        = 1.0;
    _paused       = false;
    _queue_serial = queue_serial;
    set(NAN, -1);
}

void Clock::set(double pts, int serial)
{
    double time = av_gettime_relative() / 1000000.0;
    set(pts, serial, time);
}

void Clock::set(double pts, int serial, double time)
{
    _pts          = pts;
    _last_updated = time;
    _pts_drift    = _pts - time;
    this->serial  = serial;
}