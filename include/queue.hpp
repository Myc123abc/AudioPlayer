#pragma once

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/fifo.h>
#include <libavcodec/packet.h>
}

#include <condition_variable>

struct Packet
{
    AVPacket* packet;
    int       serial; // serial number of packet
};

class PacketQueue
{
public:
    int init();
    void destroy();

private:
    AVFifo* _queue; 
    int     _size;       
    int     _capacity;     
    int64_t _duration;      // TODO: just like Frame::pos, it can be nagative?
    bool    _abort_request; // use for pause and seek
public:
    int     serial;         // TODO: serial number of stream?

    std::mutex              _mutex;
    std::condition_variable _cond;
};

struct Frame
{
    AVFrame* frame; 
    int      serial;   // serial number of the frame
    double   pts;      // presentation timestamp for the frame 
    double   duration; // estimated duration of the frame
    int64_t  pos;      // byte position of the frame in the input file  
                       // TODO: does pos can be negative?

    bool     uploaded_to_gpu; // whether uploaded to gpu use hardware acceleration
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

    void destroy() const noexcept
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
    int   _read_index;
    int   _write_index;
    int   _size;
    int   _capacity;
    bool  _keep_last;
    int   _playing_index;

    std::mutex              _mutex;
    std::condition_variable _cond;

    PacketQueue* _packet_queue;
};