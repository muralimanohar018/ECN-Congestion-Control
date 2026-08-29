#ifndef BDP_MONITOR_H
#define BDP_MONITOR_H

#include "ns3/tcp-socket-state.h"

#include <cstdint>

namespace ecn
{

class BdpMonitor
{
public:
    static void Reset(ns3::Ptr<const ns3::TcpSocketState> tcb);
    static void Update(ns3::Ptr<const ns3::TcpSocketState> tcb, double rttSeconds, uint32_t segmentSize, uint64_t bottleneckBps);
    static double GetRttSeconds(ns3::Ptr<const ns3::TcpSocketState> tcb);
    static uint64_t GetBdpBytes(ns3::Ptr<const ns3::TcpSocketState> tcb);
    static uint64_t GetBdpPackets(ns3::Ptr<const ns3::TcpSocketState> tcb);
};

}

#endif