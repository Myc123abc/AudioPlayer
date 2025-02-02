#include "queue.hpp"

int PacketQueue::init()
{
    _queue = av_fifo_alloc2(1, sizeof(Packet), AV_FIFO_FLAG_AUTO_GROW);
    if (!_queue)
    {
        return AVERROR(ENOMEM);
    }    

    _size           = 0;
    _capacity       = 0;
    _duration       = 0;     
    _abort_request  = true; 
    
    serial          = 0;     

    return 0;
}

void PacketQueue::destroy()
{
    // TODO: packet_queue_flush
    av_fifo_freep2(&_queue);
}