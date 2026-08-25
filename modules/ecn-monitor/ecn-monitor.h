#ifndef ECN_MONITOR_H
#define ECN_MONITOR_H

#include "ns3/traffic-control-module.h"

#include <cstdint>

namespace ecn
{

class EcnMonitor
{
  public:
    static void Reset();

    static void OnEnqueue(ns3::Ptr<const ns3::QueueDiscItem> item);

    static void OnMark(ns3::Ptr<const ns3::QueueDiscItem> item,const char* reason);

    static double GetRatio();

    static uint64_t GetTotalPackets();
    static uint64_t GetMarkedPackets();

    static void RecordEcnAck(double timeSeconds);

    static uint64_t GetEcnAckCount();
    static double GetLastEcnAckTime();
};

} // namespace ecn

#endif
