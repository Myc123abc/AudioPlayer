#include "queue.hpp"
#include "util.hpp"
#include "Clock.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavcodec/avcodec.h>
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

    bool eof           = false;
    bool abort_request = false;

    AVFormatContext* format_context = nullptr;

    double max_frame_duration = 0.0;

    std::string title;

    int64_t start_time = AV_NOPTS_VALUE; // TODO: int64_t? really? It is use for adjust pos before playing

    int last_audio_stream = -1;
};

int decode_interrupt_callback(void* ptr)
{
    return ((AudioContext*)ptr)->abort_request;
}

// open a given stream. Return 0 if OK
int stream_component_open(AudioContext* ac, int stream_index)
{
    int ret = 0;
    AVFormatContext* fc = ac->format_context;
    const AVCodec* decoder = nullptr;

    // validate stream index
    if (stream_index < 0 || stream_index >= fc->nb_streams)
    {
        return -1;
    }

    // alloc codec context
    AVCodecContext* cc = avcodec_alloc_context3(nullptr);
    if (!cc)
    {
        return AVERROR(ENOMEM);
    }
    // fill codec context
    auto stream = fc->streams[stream_index];
    ret = avcodec_parameters_to_context(cc, stream->codecpar);
    if (ret < 0)
    {
        goto fail;
    }
    cc->pkt_timebase = stream->time_base;

    // get codec
    decoder = avcodec_find_decoder(cc->codec_id);
    if (!decoder)
    {
        ret = AVERROR(EINVAL);
        goto fail;
    }
    switch (cc->codec_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            ac->last_audio_stream = stream_index;
            break;

        default:
            exit_if(true, "Only support audio stream");
    }
    cc->codec_id = decoder->id;

fail:
    // release codec context
    avcodec_free_context(&cc);

    return 0;
}

// this thread gets the stream from the disk
int read_thread(AudioContext* ac)
{
    // alloc packet
    AVPacket* pkt = av_packet_alloc();
    // FIXME: should I directly exit? like ffplay if use event to handle error exit case
    exit_if(!pkt, "read_thread: packet_alloc failed: low memory");

    // alloc format context
    AVFormatContext* fc = avformat_alloc_context();
    exit_if(!fc, "read_thread: avformat_alloc_context failed: low memory");

    // set interrupt callback
    fc->interrupt_callback.callback = decode_interrupt_callback;
    fc->interrupt_callback.opaque   = ac;

    // set format options
    AVDictionary* format_opts = nullptr;
    bool scan_all_pmts_set = false; 
    if (!av_dict_get(format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE))
    {
        av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
        scan_all_pmts_set = true;
    }

    // TODO: use scan_all_pmts can used for id3 tag detect?
    // open audio stream
    exit_if(avformat_open_input(&fc, ac->filename.data(), ac->input_format, &format_opts) < 0,
        "read_thread: avformat_open_input failed: %s", ac->filename.data());

    // clean format options
    if (scan_all_pmts_set)
    {
        av_dict_set(&format_opts, "scan_all_pmts", nullptr, AV_DICT_MATCH_CASE);
    }

    // set format_context
    ac->format_context = fc;
    
    // check whether seek by bytes
    bool seek_by_bytes = ! (fc->iformat->flags & AVFMT_NO_BYTE_SEEK) &
                         !!(fc->iformat->flags & AVFMT_TS_DISCONT)   &
                         strcmp("ogg", fc->iformat->name);

    // get info
    exit_if(avformat_find_stream_info(fc, nullptr)  < 0, "read_thread: avformat_find_stream_info error");

    if (fc->pb)
    {
        fc->pb->eof_reached = 0; // FIXME: hack, ffplay maybe should not use avio_feof() to test for the end
    }

    ac->max_frame_duration = (fc->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0; 

    // here can use metadata, only title here,
    // you can also use other key to get data such as date, album, etc
    AVDictionaryEntry* entry;
    entry = av_dict_get(fc->metadata, "title", nullptr, 0);
    if (entry)
    {
        ac->title = entry->value;
    }

    // if seeking requested, we execute it
    if (ac->start_time != AV_NOPTS_VALUE)
    {
        int64_t timestamp;

        timestamp = ac->start_time;

        // add the stream start time
        if (fc->start_time != AV_NOPTS_VALUE)
        {
            timestamp += fc->start_time;
        }
        
        exit_if(avformat_seek_file(fc, -1, INT64_MIN, timestamp, INT64_MAX, 0) < 0, 
            "read_thread: avformat_seek_file error:\n"
            "  %s: could not seek to position %0.3f",
            ac->filename.data(), (double)timestamp / AV_TIME_BASE);
    }

    // find best audio stream
    int audio_stream = av_find_best_stream(fc, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    // open stream
    exit_if(stream_component_open(ac, audio_stream), "read_thread: open stream error");

    // TODO: loop decode audio

    avformat_close_input(&fc);
    av_packet_free(&pkt);
    
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
    ac->filename = 
    // "D:/music/四季ノ唄.mp3";
    "/home/myc/music/四季ノ唄.mp3";
    // "D:/music/野良猫/信千奈　～yo～.ogg";
    ac->start_time = 3 * AV_TIME_BASE;

    exit_if(ac->packet_queue.init() < 0, "packet queue init failed: low memory");
    exit_if(ac->frame_queue.init(&ac->packet_queue, true) < 0, "frame queue init failed: low memory");

    ac->audio_clock.init(&ac->packet_queue.serial);
    ac->external_clock.init(&ac->external_clock.serial);

    ac->read_thread = std::jthread(read_thread, ac);

    
    // TODO: event loop


    //
    // Destroy AudioContext
    //
    ac->frame_queue.destroy();
    ac->packet_queue.destroy();

    delete ac;
}
