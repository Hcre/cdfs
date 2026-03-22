#include "Timestamp.h"
#include <cstdint>
#include <sys/time.h>

namespace cdfs{

Timestamp cdfs::Timestamp::now()
{
    struct timeval tv; //tv is a structure that contains the current time
    gettimeofday(&tv, nullptr); //get the current time
    int64_t second = tv.tv_sec; //get the seconds part of the current time
    return Timestamp(second * kMicroSecondsPerSecond + tv.tv_usec); //return the timestamp object
}
} // namespace cdfs

