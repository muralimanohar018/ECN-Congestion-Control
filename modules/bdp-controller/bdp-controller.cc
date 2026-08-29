#include "bdp-controller.h"

#include "../bdp-monitor/bdp-monitor.h"

#include <algorithm>
#include <cstdint>

using namespace ns3;

namespace ecn
{

uint32_t BdpController::Apply(Ptr<const TcpSocketState> tcb, uint32_t formulaCwnd, uint32_t segmentSize)
{
    if (segmentSize == 0)
    {
        return 0;
    }

    uint64_t bdpBytes = BdpMonitor::GetBdpBytes(tcb);

    uint32_t finalCwnd = formulaCwnd;

    if (bdpBytes > 0)
    {
        finalCwnd = static_cast<uint32_t>(std::min<uint64_t>(formulaCwnd, bdpBytes));
    }

    if (finalCwnd < segmentSize)
    {
        finalCwnd = segmentSize;
    }

    return finalCwnd;
}

}