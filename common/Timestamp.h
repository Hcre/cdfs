#ifndef CDFS_TIMESTAMP_H
#define CDFS_TIMESTAMP_H
#include <cstdint>

namespace cdfs{

class Timestamp {
public:
    Timestamp() : microsecondsSinceEpoch_(0) {}
    explicit Timestamp(int64_t microsecondsSinceEpoch)
        : microsecondsSinceEpoch_(microsecondsSinceEpoch) {}

    static Timestamp now();
    int64_t microsecondsSinceEpoch() const { return microsecondsSinceEpoch_; }

    const static int kMicroSecondsPerSecond = 1000 * 1000;
private:
    int64_t microsecondsSinceEpoch_;
};

} // namespace cdfs




#endif