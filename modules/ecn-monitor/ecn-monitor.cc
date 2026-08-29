#include "ecn-monitor.h"

#include "ns3/core-module.h"

#include <iomanip>
#include <iostream>

using namespace ns3;
using namespace std;

namespace ecn
{

namespace
{

uint64_t g_totalPackets = 0;
uint64_t g_ecnMarkedPackets = 0;
uint64_t g_ecnAckCount = 0;
double g_lastEcnAckTime = 0.0;

}

void EcnMonitor::Reset()
{
    g_totalPackets = 0;
    g_ecnMarkedPackets = 0;
    g_ecnAckCount = 0;
    g_lastEcnAckTime = 0.0;
}

void EcnMonitor::OnEnqueue(Ptr<const QueueDiscItem> item)
{
    (void)item;

    ++g_totalPackets;
}

void EcnMonitor::OnMark(Ptr<const QueueDiscItem> item, const char* reason)
{
    (void)item;

    ++g_ecnMarkedPackets;

    cout << "[ECN MARK] "
         << "time=" << fixed << setprecision(6)
         << Simulator::Now().GetSeconds()
         << " s"
         << " | total=" << g_totalPackets
         << " | marked=" << g_ecnMarkedPackets
         << " | cumulativeRatio=" << setprecision(8)
         << GetCumulativeRatio()
         << " | reason=" << reason
         << "\n";
}

double EcnMonitor::GetCumulativeRatio()
{
    if (g_totalPackets == 0)
    {
        return 0.0;
    }

    return static_cast<double>(g_ecnMarkedPackets) /
           static_cast<double>(g_totalPackets);
}

double EcnMonitor::GetRatio()
{
    return GetCumulativeRatio();
}

uint64_t EcnMonitor::GetTotalPackets()
{
    return g_totalPackets;
}

uint64_t EcnMonitor::GetMarkedPackets()
{
    return g_ecnMarkedPackets;
}

void EcnMonitor::RecordEcnAck(double timeSeconds)
{
    ++g_ecnAckCount;

    g_lastEcnAckTime = timeSeconds;
}

uint64_t EcnMonitor::GetEcnAckCount()
{
    return g_ecnAckCount;
}

double EcnMonitor::GetLastEcnAckTime()
{
    return g_lastEcnAckTime;
}

}