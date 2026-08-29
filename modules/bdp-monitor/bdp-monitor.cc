#include "bdp-monitor.h"

#include <unordered_map>

using namespace ns3;
using namespace std;

namespace ecn
{

namespace
{

struct BdpData
{
    double rttSeconds = 0.0;
    uint64_t bdpBytes = 0;
    uint64_t bdpPackets = 0;
};

unordered_map<const TcpSocketState*, BdpData> g_bdpData;

}

void BdpMonitor::Reset(Ptr<const TcpSocketState> tcb)
{
    if (tcb)
    {
        g_bdpData[PeekPointer(tcb)] = BdpData();
    }
}

void BdpMonitor::Update(Ptr<const TcpSocketState> tcb, double rttSeconds, uint32_t segmentSize, uint64_t bottleneckBps)
{
    if (!tcb || rttSeconds <= 0.0 || segmentSize == 0 || bottleneckBps == 0)
    {
        return;
    }

    BdpData& data = g_bdpData[PeekPointer(tcb)];

    data.rttSeconds = rttSeconds;

    double bdpBits = static_cast<double>(bottleneckBps) * rttSeconds;

    data.bdpBytes = static_cast<uint64_t>(bdpBits / 8.0);

    data.bdpPackets = data.bdpBytes / segmentSize;
}

double BdpMonitor::GetRttSeconds(Ptr<const TcpSocketState> tcb)
{
    if (!tcb)
    {
        return 0.0;
    }

    auto it = g_bdpData.find(PeekPointer(tcb));

    if (it == g_bdpData.end())
    {
        return 0.0;
    }

    return it->second.rttSeconds;
}

uint64_t BdpMonitor::GetBdpBytes(Ptr<const TcpSocketState> tcb)
{
    if (!tcb)
    {
        return 0;
    }

    auto it = g_bdpData.find(PeekPointer(tcb));

    if (it == g_bdpData.end())
    {
        return 0;
    }

    return it->second.bdpBytes;
}

uint64_t BdpMonitor::GetBdpPackets(Ptr<const TcpSocketState> tcb)
{
    if (!tcb)
    {
        return 0;
    }

    auto it = g_bdpData.find(PeekPointer(tcb));

    if (it == g_bdpData.end())
    {
        return 0;
    }

    return it->second.bdpPackets;
}

}