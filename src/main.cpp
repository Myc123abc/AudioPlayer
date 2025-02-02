#include "queue.hpp"
#include "util.hpp"

namespace
{

constexpr int Frame_Queue_Capacity = 9;

}

int main()
{
    av_log_set_level(AV_LOG_DEBUG);

    PacketQueue packet_queue;
    FrameQueue<Frame_Queue_Capacity> frame_queue;

    exit_if(frame_queue.init(&packet_queue, true) < 0, "frame queue init failed: low memory");



    frame_queue.release();
}