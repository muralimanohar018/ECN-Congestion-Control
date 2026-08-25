#include "bdp-monitor.h"

namespace ecn
{

namespace
{
double g_lastRttSeconds = 0.0;
uint64_t g_bdpBytes = 0;
uint64_t g_bdpPackets = 0;
}

void BdpMonitor::Reset()
{
    g_lastRttSeconds = 0.0;
    g_bdpBytes = 0;
    g_bdpPackets = 0;
}

void BdpMonitor::Update(double rttSeconds, uint32_t segmentSize, uint64_t bottleneckBps)
{
    if (rttSeconds <= 0.0)
        return;

    g_lastRttSeconds = rttSeconds;

    double bdpBits = static_cast<double>(bottleneckBps) * g_lastRttSeconds;
    g_bdpBytes = static_cast<uint64_t>(bdpBits / 8.0);

    if (segmentSize > 0)
        g_bdpPackets = g_bdpBytes / segmentSize;
}

double BdpMonitor::GetRttSeconds()
{
    return g_lastRttSeconds;
}

uint64_t BdpMonitor::GetBdpBytes()
{
    return g_bdpBytes;
}

uint64_t BdpMonitor::GetBdpPackets()
{
    return g_bdpPackets;
}

}