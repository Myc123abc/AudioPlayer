#pragma once

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/fifo.h>
}

#include <condition_variable>

struct PacketQueue
{
    AVFifo* queue;
    int     size;
    int     capacity;
    int64_t duration;       // TODO: just like Frame::pos, it can be nagative?
    bool    abort_request;  // use for pause and seek
    int     serial;         // TODO: serial number of stream?

    std::mutex              mutex;
    std::condition_variable cond;
};

struct Frame
{
    AVFrame* frame    = nullptr;
    int      serial   = 0;       // serial number of the frame
    double   pts      = 0.0;     // presentation timestamp for the frame 
    double   duration = 0.0;     // estimated duration of the frame
    int64_t  pos      = 0;       // byte position of the frame in the input file  
                                 // TODO: does pos can be negative?

    bool     uploaded_to_gpu = false; // whether uploaded to gpu use hardware acceleration
                                      // TODO: it be used? if not used,
                                      //       maybe I can try the hardware acceleration
};

template <int Frame_Queue_Capacity>
class FrameQueue
{
public:
    int init(PacketQueue* packet_queue, bool keep_last) noexcept
    {
        _read_index    = 0;
        _write_index   = 0;
        _size          = 0;
        _capacity      = Frame_Queue_Capacity;
        _keep_last     = keep_last;
        _playing_index = 0;
        _packet_queue  = packet_queue;

        for (int i = 0; i < Frame_Queue_Capacity; ++i)
        {
            if (!(_queue[i].frame = av_frame_alloc()))
            {
                return AVERROR(ENOMEM);
            }
        }

        return 0;
    }

    void release() const noexcept
    {
        for (int i = 0; i < Frame_Queue_Capacity; ++i)
        {
            auto frame = _queue[i].frame;
            // TODO: frame_queue_unref_item(frame);
            av_frame_free(&frame);
        }
    }

private:
    Frame _queue[Frame_Queue_Capacity];
    int   _read_index    = 0;
    int   _write_index   = 0;
    int   _size          = 0;
    int   _capacity      = 0;
    bool  _keep_last     = false;
    int   _playing_index = 0;

    std::mutex              _mutex;
    std::condition_variable _cond;

    PacketQueue* _packet_queue = nullptr;
};