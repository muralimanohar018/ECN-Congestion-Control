#ifndef ECN_MONITOR_H
#define ECN_MONITOR_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/traffic-control-module.h"
#include <cstdint>

namespace ecn
{

class EcnMonitor
{
public:
    static void Reset();

    // Interval statistics used by the controller
    static void ResetInterval();
    static void OnEnqueue(ns3::Ptr<const ns3::QueueDiscItem> item);
    static void OnMark(ns3::Ptr<const ns3::QueueDiscItem> item,
                       const char* reason);

    static double GetRatio();

    // Cumulative statistics used by final results
    static double GetCumulativeRatio();
    static uint64_t GetTotalPackets();
    static uint64_t GetMarkedPackets();

    static uint64_t GetIntervalTotalPackets();
    static uint64_t GetIntervalMarkedPackets();

    static void RecordEcnAck(double timeSeconds);
    static uint64_t GetEcnAckCount();
    static double GetLastEcnAckTime();
};

}

#endif