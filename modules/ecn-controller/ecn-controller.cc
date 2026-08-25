#include "ecn-controller.h"
#include "../bdp-monitor/bdp-monitor.h"
#include "../bdp-controller/bdp-controller.h"
#include "../ecn-monitor/ecn-monitor.h"
#include "ns3/core-module.h"
#include "ns3/tcp-socket-state.h"
#include <iomanip>
#include <iostream>

using namespace ns3;

namespace ns3
{
namespace ecn
{

double EcnController::g_alpha = 1.0;
uint64_t EcnController::g_bottleneckBps = 5000000;
uint32_t EcnController::g_lastOldCwnd = 0;
uint32_t EcnController::g_lastFormulaCwnd = 0;
uint32_t EcnController::g_lastFinalCwnd = 0;
uint32_t EcnController::g_controllerUpdates = 0;

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

std::string EcnController::GetName() const
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

void EcnController::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    TcpNewReno::PktsAcked(tcb, segmentsAcked, rtt);
    if (rtt.IsZero()) return;
    BdpMonitor::Update(rtt.GetSeconds(), tcb->m_segmentSize, g_bottleneckBps);
}

uint32_t EcnController::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    if (tcb->m_ecnState.Get() != TcpSocketState::ECN_ECE_RCVD)
        return TcpNewReno::GetSsThresh(tcb, bytesInFlight);

    EcnMonitor::RecordEcnAck(Simulator::Now().GetSeconds());

    uint32_t oldCwnd = tcb->m_cWnd.Get();
    g_lastOldCwnd = oldCwnd;

    double ecnRatio = EcnMonitor::GetRatio();
    double reductionFactor = 1.0 - (g_alpha * ecnRatio);

    if (reductionFactor < 0.0)
        reductionFactor = 0.0;

    uint32_t formulaCwnd = static_cast<uint32_t>(oldCwnd * reductionFactor);

    if (formulaCwnd < tcb->m_segmentSize)
        formulaCwnd = tcb->m_segmentSize;

    g_lastFormulaCwnd = formulaCwnd;

    uint32_t finalCwnd = BdpController::Apply(formulaCwnd, tcb->m_segmentSize);

    if (finalCwnd < tcb->m_segmentSize)
        finalCwnd = tcb->m_segmentSize;

    g_lastFinalCwnd = finalCwnd;
    ++g_controllerUpdates;

    std::cout << "\n"
              << "==================================================\n"
              << "             ECN + BDP CONTROLLER\n"
              << "==================================================\n"
              << "Controller Update : " << g_controllerUpdates << "\n"
              << "Time              : " << std::fixed << std::setprecision(8) << Simulator::Now().GetSeconds() << " s\n"
              << "ECN ACK Count     : " << EcnMonitor::GetEcnAckCount() << "\n"
              << "Total Packets     : " << EcnMonitor::GetTotalPackets() << "\n"
              << "ECN Marked        : " << EcnMonitor::GetMarkedPackets() << "\n"
              << "ECN Ratio         : " << ecnRatio << "\n"
              << "Alpha             : " << g_alpha << "\n"
              << "RTT               : " << BdpMonitor::GetRttSeconds() << " s\n"
              << "BDP               : " << BdpMonitor::GetBdpBytes() << " bytes\n"
              << "BDP               : " << BdpMonitor::GetBdpPackets() << " packets\n"
              << "Old CWND          : " << oldCwnd << " bytes\n"
              << "Bytes In Flight   : " << bytesInFlight << " bytes\n"
              << "Formula CWND      : " << formulaCwnd << " bytes\n"
              << "Final CWND        : " << finalCwnd << " bytes\n"
              << "==================================================\n";

    return finalCwnd;
}

void EcnController::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
}

void EcnController::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
    TcpNewReno::CwndEvent(tcb, event);

    if (event == TcpSocketState::CA_EVENT_ECN_IS_CE)
        std::cout << "[TCP CE] time=" << std::fixed << std::setprecision(8) << Simulator::Now().GetSeconds() << " s\n";

    if (event == TcpSocketState::CA_EVENT_COMPLETE_CWR)
        std::cout << "[TCP CWR] time=" << std::fixed << std::setprecision(8) << Simulator::Now().GetSeconds() << " s\n";
}

NS_OBJECT_ENSURE_REGISTERED(EcnController);

}
}