#ifndef BDP_MONITOR_H
#define BDP_MONITOR_H

#include <cstdint>

namespace ecn
{

class BdpMonitor
{
  public:
    static void Reset();

    static void Update(double rttSeconds,uint32_t segmentSize,uint64_t bottleneckBps);

    static double GetRttSeconds();
    static uint64_t GetBdpBytes();
    static uint64_t GetBdpPackets();
};

} // namespace ecn

#endif
