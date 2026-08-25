#include "bdp-controller.h"
#include "../bdp-monitor/bdp-monitor.h"

namespace ecn
{

uint32_t BdpController::Apply(uint32_t formulaCwnd, uint32_t segmentSize)
{
    uint32_t finalCwnd = formulaCwnd;
    uint64_t bdp = BdpMonitor::GetBdpBytes();

    if (bdp > 0 && finalCwnd > bdp)
        finalCwnd = bdp;

    if (finalCwnd < segmentSize)
        finalCwnd = segmentSize;

    return finalCwnd;
}

}