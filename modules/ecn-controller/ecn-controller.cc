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

double EcnController::g_alpha = 1.0;
uint64_t EcnController::g_bottleneckBps = 50000000;
uint32_t EcnController::g_lastOldCwnd = 0;
uint32_t EcnController::g_lastFormulaCwnd = 0;
uint32_t EcnController::g_lastFinalCwnd = 0;
uint32_t EcnController::g_controllerUpdates = 0;
uint64_t EcnController::g_lastProcessedEcnMarks = 0;

TypeId EcnController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::EcnController").SetParent<TcpNewReno>().SetGroupName("Internet").AddConstructor<EcnController>();
    return tid;
}

EcnController::EcnController() : TcpNewReno()
{
}

EcnController::EcnController(const EcnController& other) : TcpNewReno(other)
{
}

EcnController::~EcnController() = default;

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

    if (rtt.IsZero())
    {
        return;
    }

    BdpMonitor::Update(tcb, rtt.GetSeconds(), tcb->m_segmentSize, g_bottleneckBps);
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
    (void)rc;
    (void)rs;

    if (!tcb)
    {
        return;
    }

    uint32_t segmentSize = tcb->m_segmentSize;

    if (segmentSize == 0)
    {
        return;
    }

    uint64_t totalPackets = EcnMonitor::GetTotalPackets();
    uint64_t markedPackets = EcnMonitor::GetMarkedPackets();

    if (totalPackets == 0 || markedPackets == 0)
    {
        return;
    }

    uint64_t bdpBytes64 = BdpMonitor::GetBdpBytes(tcb);

    if (bdpBytes64 == 0)
    {
        return;
    }

    if (markedPackets <= g_lastProcessedEcnMarks)
    {
        return;
    }

    g_lastProcessedEcnMarks = markedPackets;

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

    double ecnRatio = static_cast<double>(markedPackets) / static_cast<double>(totalPackets);

    double reductionFactor = 1.0 - (g_alpha * ecnRatio);

    if (reductionFactor < 0.0)
    {
        reductionFactor = 0.0;
    }

    uint32_t formulaCwnd = static_cast<uint32_t>(static_cast<double>(oldCwnd) * reductionFactor);

    if (formulaCwnd < segmentSize)
    {
        formulaCwnd = segmentSize;
    }

    uint32_t finalCwnd = BdpController::Apply(tcb, formulaCwnd, segmentSize);

    if (finalCwnd < segmentSize)
    {
        finalCwnd = segmentSize;
    }

    tcb->m_cWnd = finalCwnd;

    g_lastOldCwnd = oldCwnd;
    g_lastFormulaCwnd = formulaCwnd;
    g_lastFinalCwnd = finalCwnd;

    ++g_controllerUpdates;

    EcnMonitor::RecordEcnAck(Simulator::Now().GetSeconds());

    cout << "\n";
    cout << "==================================================\n";
    cout << "           ECN + BDP CONTROLLER\n";
    cout << "==================================================\n";
    cout << "Controller Update : " << g_controllerUpdates << "\n";
    cout << "Time              : " << fixed << setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
    cout << "Total Packets     : " << totalPackets << "\n";
    cout << "Total ECN Marked  : " << markedPackets << "\n";
    cout << "Cumulative ECN E  : " << ecnRatio << "\n";
    cout << "Alpha             : " << g_alpha << "\n";
    cout << "Bottleneck        : " << g_bottleneckBps << " bps\n";
    cout << "RTT               : " << BdpMonitor::GetRttSeconds(tcb) << " s\n";
    cout << "BDP               : " << BdpMonitor::GetBdpBytes(tcb) << " bytes\n";
    cout << "BDP               : " << BdpMonitor::GetBdpPackets(tcb) << " packets\n";
    cout << "Old CWND          : " << oldCwnd << " bytes\n";
    cout << "Formula CWND      : " << formulaCwnd << " bytes\n";
    cout << "Final CWND        : " << finalCwnd << " bytes\n";
    cout << "Action            : CUMULATIVE ECN REDUCTION\n";
    cout << "==================================================\n";
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

NS_OBJECT_ENSURE_REGISTERED(EcnController);

}