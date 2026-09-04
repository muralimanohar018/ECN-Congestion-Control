#include "ecn-controller.h"

#include "../bdp-controller/bdp-controller.h"
#include "../bdp-monitor/bdp-monitor.h"
#include "../ecn-monitor/ecn-monitor.h"

#include "ns3/core-module.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace ns3;
using namespace std;

namespace ecn
{

double EcnController::g_alpha = 0.5;
uint64_t EcnController::g_bottleneckBps = 50000000;

uint32_t EcnController::g_lastOldCwnd = 0;
uint32_t EcnController::g_lastFormulaCwnd = 0;
uint32_t EcnController::g_lastFinalCwnd = 0;
uint32_t EcnController::g_controllerUpdates = 0;

uint64_t EcnController::g_intervalLastPackets = 0;
uint64_t EcnController::g_intervalLastMarkedPackets = 0;

double EcnController::g_intervalSeconds = 0.100;

bool EcnController::g_intervalInitialized = false;
bool EcnController::g_intervalScheduled = false;

EventId EcnController::g_intervalEvent;

vector<Ptr<TcpSocketState>> EcnController::g_activeFlows;

TypeId EcnController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EcnController").SetParent<TcpNewReno>().SetGroupName("Internet").AddConstructor<EcnController>();
    return tid;
}

EcnController::EcnController() : TcpNewReno(), m_tcb(nullptr), m_registered(false)
{
}

EcnController::EcnController(const EcnController& other) : TcpNewReno(other), m_tcb(nullptr), m_registered(false)
{
    (void)other;
}

EcnController::~EcnController()
{
    if (m_registered && m_tcb)
    {
        UnregisterFlow(m_tcb);
    }
}

string EcnController::GetName() const
{
    return "EcnController";
}

Ptr<TcpCongestionOps> EcnController::Fork()
{
    return CopyObject<EcnController>(this);
}

void EcnController::SetAlpha(double alpha)
{
    g_alpha = alpha;
}

double EcnController::GetAlpha()
{
    return g_alpha;
}

void EcnController::SetBottleneckBps(uint64_t bps)
{
    g_bottleneckBps = bps;
}

uint32_t EcnController::GetLastOldCwnd()
{
    return g_lastOldCwnd;
}

uint32_t EcnController::GetLastFormulaCwnd()
{
    return g_lastFormulaCwnd;
}

uint32_t EcnController::GetLastFinalCwnd()
{
    return g_lastFinalCwnd;
}

uint32_t EcnController::GetControllerUpdates()
{
    return g_controllerUpdates;
}

bool EcnController::HasCongControl() const
{
    return true;
}

void EcnController::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    (void)segmentsAcked;

    if (!tcb)
    {
        return;
    }

    if (!m_registered)
    {
        m_tcb = tcb;
        RegisterFlow(tcb);
        m_registered = true;
    }

    if (!rtt.IsZero())
    {
        BdpMonitor::Update(tcb, rtt.GetSeconds(), tcb->m_segmentSize, g_bottleneckBps);
    }

    ScheduleIntervalControl();
}

uint32_t EcnController::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    (void)tcb;
    (void)bytesInFlight;

    return UINT32_MAX;
}

void EcnController::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    (void)tcb;
    (void)segmentsAcked;
}

void EcnController::CongControl(Ptr<TcpSocketState> tcb, const TcpRateOps::TcpRateConnection& rc, const TcpRateOps::TcpRateSample& rs)
{
    (void)tcb;
    (void)rc;
    (void)rs;
}

void EcnController::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    (void)tcb;

    if (event == TcpSocketState::CA_EVENT_ECN_IS_CE)
    {
        cout << "[TCP CE] time=" << fixed << setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
    }

    if (event == TcpSocketState::CA_EVENT_COMPLETE_CWR)
    {
        cout << "[TCP CWR] time=" << fixed << setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
    }
}

void EcnController::RegisterFlow(Ptr<TcpSocketState> tcb)
{
    if (!tcb)
    {
        return;
    }

    for (const auto& flow : g_activeFlows)
    {
        if (flow == tcb)
        {
            return;
        }
    }

    g_activeFlows.push_back(tcb);

    if (!g_intervalInitialized)
    {
        g_intervalLastPackets = EcnMonitor::GetTotalPackets();
        g_intervalLastMarkedPackets = EcnMonitor::GetMarkedPackets();
        g_intervalInitialized = true;
    }

    ScheduleIntervalControl();
}

void EcnController::UnregisterFlow(Ptr<TcpSocketState> tcb)
{
    if (!tcb)
    {
        return;
    }

    g_activeFlows.erase(
        remove_if(
            g_activeFlows.begin(),
            g_activeFlows.end(),
            [tcb](const Ptr<TcpSocketState>& flow)
            {
                return flow == tcb;
            }),
        g_activeFlows.end());

    if (g_activeFlows.empty() && g_intervalScheduled)
    {
        Simulator::Cancel(g_intervalEvent);
        g_intervalScheduled = false;
    }
}

void EcnController::ScheduleIntervalControl()
{
    if (g_activeFlows.empty())
    {
        return;
    }

    if (g_intervalScheduled)
    {
        return;
    }

    g_intervalScheduled = true;

    g_intervalEvent = Simulator::Schedule(
        Seconds(g_intervalSeconds),
        &EcnController::IntervalControl);
}

void EcnController::IntervalControl()
{
    g_intervalScheduled = false;

    if (g_activeFlows.empty())
    {
        return;
    }

    uint64_t totalPackets = EcnMonitor::GetTotalPackets();
    uint64_t markedPackets = EcnMonitor::GetMarkedPackets();

    uint64_t intervalPackets = 0;
    uint64_t intervalMarkedPackets = 0;

    if (totalPackets >= g_intervalLastPackets)
    {
        intervalPackets = totalPackets - g_intervalLastPackets;
    }

    if (markedPackets >= g_intervalLastMarkedPackets)
    {
        intervalMarkedPackets = markedPackets - g_intervalLastMarkedPackets;
    }

    g_intervalLastPackets = totalPackets;
    g_intervalLastMarkedPackets = markedPackets;

    double intervalEcnRatio = 0.0;

    if (intervalPackets > 0)
    {
        intervalEcnRatio = static_cast<double>(intervalMarkedPackets) / static_cast<double>(intervalPackets);
    }

    double cumulativeEcnRatio = 0.0;

    if (totalPackets > 0)
    {
        cumulativeEcnRatio = static_cast<double>(markedPackets) / static_cast<double>(totalPackets);
    }

    uint32_t intervalUpdates = 0;

    for (auto it = g_activeFlows.begin(); it != g_activeFlows.end();)
    {
        Ptr<TcpSocketState> tcb = *it;

        if (!tcb)
        {
            it = g_activeFlows.erase(it);
            continue;
        }

        ++it;

        uint32_t segmentSize = tcb->m_segmentSize;

        if (segmentSize == 0)
        {
            continue;
        }

        uint64_t bdpBytes64 = BdpMonitor::GetBdpBytes(tcb);

        if (bdpBytes64 == 0)
        {
            continue;
        }

        uint32_t bdpBytes = static_cast<uint32_t>(min<uint64_t>(bdpBytes64, UINT32_MAX));

        uint32_t oldCwnd = tcb->m_cWnd.Get();

        if (oldCwnd < segmentSize)
        {
            oldCwnd = segmentSize;
        }

        if (oldCwnd > bdpBytes)
        {
            oldCwnd = bdpBytes;
        }

        uint32_t formulaCwnd = oldCwnd;
        uint32_t finalCwnd = oldCwnd;

        string action;

        if (intervalMarkedPackets == 0)
        {
            uint64_t increasedCwnd = static_cast<uint64_t>(oldCwnd) + segmentSize;

            if (increasedCwnd > bdpBytes)
            {
                increasedCwnd = bdpBytes;
            }

            formulaCwnd = static_cast<uint32_t>(increasedCwnd);
            action = "NO ECN - CWND INCREASE";
        }
        else
        {
            double reductionFactor = 1.0 - (g_alpha * intervalEcnRatio);

            if (reductionFactor < 0.0)
            {
                reductionFactor = 0.0;
            }

            formulaCwnd = static_cast<uint32_t>(static_cast<double>(oldCwnd) * reductionFactor);

            if (formulaCwnd < segmentSize)
            {
                formulaCwnd = segmentSize;
            }

            action = "ECN DETECTED - CWND REDUCTION";
        }

        finalCwnd = BdpController::Apply(tcb, formulaCwnd, segmentSize);

        if (finalCwnd < segmentSize)
        {
            finalCwnd = segmentSize;
        }

        if (finalCwnd > bdpBytes)
        {
            finalCwnd = bdpBytes;
        }

        tcb->m_cWnd = finalCwnd;

        g_lastOldCwnd = oldCwnd;
        g_lastFormulaCwnd = formulaCwnd;
        g_lastFinalCwnd = finalCwnd;

        ++g_controllerUpdates;
        ++intervalUpdates;

        cout << "\n";
        cout << "--------------------------------------------------\n";
        cout << "          FLOW CWND CONTROL UPDATE\n";
        cout << "--------------------------------------------------\n";
        cout << "Time              : " << fixed << setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
        cout << "Interval          : " << g_intervalSeconds << " s\n";
        cout << "Interval Packets  : " << intervalPackets << "\n";
        cout << "Interval ECN Mark : " << intervalMarkedPackets << "\n";
        cout << "Interval ECN E    : " << intervalEcnRatio << "\n";
        cout << "Cumulative Packets: " << totalPackets << "\n";
        cout << "Cumulative ECN    : " << markedPackets << "\n";
        cout << "Cumulative ECN E  : " << cumulativeEcnRatio << "\n";
        cout << "Alpha             : " << g_alpha << "\n";
        cout << "Bottleneck        : " << g_bottleneckBps << " bps\n";
        cout << "RTT               : " << BdpMonitor::GetRttSeconds(tcb) << " s\n";
        cout << "BDP               : " << BdpMonitor::GetBdpBytes(tcb) << " bytes\n";
        cout << "BDP               : " << BdpMonitor::GetBdpPackets(tcb) << " packets\n";
        cout << "MSS               : " << segmentSize << " bytes\n";
        cout << "Old CWND          : " << oldCwnd << " bytes\n";
        cout << "Formula CWND      : " << formulaCwnd << " bytes\n";
        cout << "Final CWND        : " << finalCwnd << " bytes\n";
        cout << "Action            : " << action << "\n";
        cout << "--------------------------------------------------\n";
    }

    if (intervalUpdates > 0)
    {
        if (intervalMarkedPackets > 0)
        {
            EcnMonitor::RecordEcnAck(Simulator::Now().GetSeconds());
        }

        cout << "\n";
        cout << "==================================================\n";
        cout << "        GLOBAL ECN INTERVAL SUMMARY\n";
        cout << "==================================================\n";
        cout << "Time              : " << fixed << setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
        cout << "Active TCP Flows  : " << g_activeFlows.size() << "\n";
        cout << "Interval Packets  : " << intervalPackets << "\n";
        cout << "Interval ECN Mark : " << intervalMarkedPackets << "\n";
        cout << "Interval ECN Ratio: " << intervalEcnRatio << "\n";
        cout << "Cumulative Packets: " << totalPackets << "\n";
        cout << "Cumulative ECN    : " << markedPackets << "\n";
        cout << "Cumulative Ratio  : " << cumulativeEcnRatio << "\n";
        cout << "Controller Updates: " << g_controllerUpdates << "\n";
        cout << "==================================================\n";
    }

    ScheduleIntervalControl();
}

NS_OBJECT_ENSURE_REGISTERED(EcnController);

}