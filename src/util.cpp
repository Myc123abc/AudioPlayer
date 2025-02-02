#include "util.hpp"

extern "C"
{
#include <libavutil/log.h>
}

#include <stdlib.h>

void exit_if(bool b, const char* msg, ...)
{
    if (b)
    {
        va_list args;
        va_start(args, msg);
        av_log(nullptr, AV_LOG_FATAL, msg, args);
        va_end(args);
        exit(EXIT_FAILURE);
    }
}