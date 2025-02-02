#pragma once

class Clock
{
public:
    void init(int* queue_serial);

    void set(double pts, int serial);
    void set(double pts, int serial, double time);

private:
    double _pts;          // clock base
    double _pts_drift;    // clock base minus time at which we updated the clock
    double _last_updated;
    double _speed;
    bool   _paused;
    int*   _queue_serial; // pointer to the current packet queue serial,
                          // used for obsolete clock detection
public:
    int    serial;        // clock is based on a packet with this serial
};