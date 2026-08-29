#ifndef BDP_CONTROLLER_H
#define BDP_CONTROLLER_H

#include "ns3/tcp-socket-state.h"

#include <cstdint>

namespace ecn
{

class BdpController
{
public:
    static uint32_t Apply(ns3::Ptr<const ns3::TcpSocketState> tcb, uint32_t formulaCwnd, uint32_t segmentSize);
};

}

#endif