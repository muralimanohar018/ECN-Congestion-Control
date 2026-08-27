#include "ecn-monitor.h"

#include "ns3/core-module.h"

#include <iomanip>
#include <iostream>

using namespace ns3;

namespace ecn
{

namespace
{

// Cumulative statistics
uint64_t g_totalPackets = 0;
uint64_t g_ecnMarkedPackets = 0;

// Interval statistics
uint64_t g_intervalTotalPackets = 0;
uint64_t g_intervalEcnMarkedPackets = 0;

// ECN ACK statistics
uint64_t g_ecnAckCount = 0;
double g_lastEcnAckTime = 0.0;

}

void
EcnMonitor::Reset()
{
    g_totalPackets = 0;
    g_ecnMarkedPackets = 0;

    g_intervalTotalPackets = 0;
    g_intervalEcnMarkedPackets = 0;

    g_ecnAckCount = 0;
    g_lastEcnAckTime = 0.0;
}

void
EcnMonitor::ResetInterval()
{
    g_intervalTotalPackets = 0;
    g_intervalEcnMarkedPackets = 0;
}

void
EcnMonitor::OnEnqueue(Ptr<const QueueDiscItem> item)
{
    (void)item;

    // Cumulative
    ++g_totalPackets;

    // Current controller interval
    ++g_intervalTotalPackets;
}

void
EcnMonitor::OnMark(Ptr<const QueueDiscItem> item,
                   const char* reason)
{
    (void)item;

    // Cumulative
    ++g_ecnMarkedPackets;

    // Current controller interval
    ++g_intervalEcnMarkedPackets;

    std::cout
        << "[ECN MARK] "
        << "time=" << std::fixed << std::setprecision(6)
        << Simulator::Now().GetSeconds()
        << " s"
        << " | total=" << g_totalPackets
        << " | marked=" << g_ecnMarkedPackets
        << " | intervalTotal=" << g_intervalTotalPackets
        << " | intervalMarked=" << g_intervalEcnMarkedPackets
        << " | ratio=" << std::setprecision(8)
        << GetRatio()
        << " | reason=" << reason
        << "\n";
}

double
EcnMonitor::GetRatio()
{
    // IMPORTANT:
    // This is the INTERVAL ECN ratio.
    // It is used by YOUR controller.

    if (g_intervalTotalPackets == 0)
    {
        return 0.0;
    }

    return static_cast<double>(g_intervalEcnMarkedPackets) /
           static_cast<double>(g_intervalTotalPackets);
}

double
EcnMonitor::GetCumulativeRatio()
{
    // IMPORTANT:
    // This is the CUMULATIVE ECN ratio.
    // It is used only for final statistics.

    if (g_totalPackets == 0)
    {
        return 0.0;
    }

    return static_cast<double>(g_ecnMarkedPackets) /
           static_cast<double>(g_totalPackets);
}

uint64_t
EcnMonitor::GetTotalPackets()
{
    return g_totalPackets;
}

uint64_t
EcnMonitor::GetMarkedPackets()
{
    return g_ecnMarkedPackets;
}

uint64_t
EcnMonitor::GetIntervalTotalPackets()
{
    return g_intervalTotalPackets;
}

uint64_t
EcnMonitor::GetIntervalMarkedPackets()
{
    return g_intervalEcnMarkedPackets;
}

void
EcnMonitor::RecordEcnAck(double timeSeconds)
{
    ++g_ecnAckCount;
    g_lastEcnAckTime = timeSeconds;
}

uint64_t
EcnMonitor::GetEcnAckCount()
{
    return g_ecnAckCount;
}

double
EcnMonitor::GetLastEcnAckTime()
{
    return g_lastEcnAckTime;
}

}