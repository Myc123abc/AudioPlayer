#include "queue.hpp"
#include "util.hpp"
#include "Clock.hpp"

extern "C"
{
#include <libavformat/avformat.h>
}

#include <thread>

namespace
{

constexpr int Frame_Queue_Capacity = 9;

struct AudioContext
{
    int                  audio_stream = -1;
    std::string          filename;
    const AVInputFormat* input_format = nullptr;

    PacketQueue                      packet_queue;
    FrameQueue<Frame_Queue_Capacity> frame_queue;

    std::condition_variable continue_read_thread;

    Clock audio_clock;
    Clock external_clock;
    int   clock_serial = -1;

    int  audio_volume = 100;
    bool muted        = false;
    
    std::jthread read_thread;
};

// this thread gets the stream from the disk
int read_thread(AudioContext* ac)
{
    return 0;
}

}

int main()
{
    av_log_set_level(AV_LOG_DEBUG);

    auto ac = new AudioContext();

    //
    // Initialize AudioContext
    //
    ac->filename = "D:/music/四季ノ唄.mp3";

    exit_if(ac->packet_queue.init() < 0, "packet queue init failed: low memory");
    exit_if(ac->frame_queue.init(&ac->packet_queue, true) < 0, "frame queue init failed: low memory");

    ac->audio_clock.init(&ac->packet_queue.serial);
    ac->external_clock.init(&ac->external_clock.serial);

    ac->read_thread = std::jthread(read_thread, ac);



    //
    // Destroy AudioContext
    //
    ac->frame_queue.destroy();
    ac->packet_queue.destroy();

    delete ac;
}