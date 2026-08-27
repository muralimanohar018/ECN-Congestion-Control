#include "ecn-controller.h"

#include "../bdp-monitor/bdp-monitor.h"
#include "../bdp-controller/bdp-controller.h"
#include "../ecn-monitor/ecn-monitor.h"

#include "ns3/core-module.h"
#include "ns3/tcp-socket-state.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace ns3;

namespace ecn
{

double EcnController::g_alpha = 1.0;
uint64_t EcnController::g_bottleneckBps = 5000000;

uint32_t EcnController::g_lastOldCwnd = 0;
uint32_t EcnController::g_lastFormulaCwnd = 0;
uint32_t EcnController::g_lastFinalCwnd = 0;
uint32_t EcnController::g_controllerUpdates = 0;

TypeId
EcnController::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::EcnController")
            .SetParent<TcpNewReno>()
            .SetGroupName("Internet")
            .AddConstructor<EcnController>();

    return tid;
}

EcnController::EcnController()
    : TcpNewReno()
{
}

EcnController::EcnController(const EcnController& other)
    : TcpNewReno(other)
{
}

EcnController::~EcnController() = default;

std::string
EcnController::GetName() const
{
    return "EcnController";
}

Ptr<TcpCongestionOps>
EcnController::Fork()
{
    return CopyObject<EcnController>(this);
}

void
EcnController::SetAlpha(double alpha)
{
    g_alpha = alpha;
}

double
EcnController::GetAlpha()
{
    return g_alpha;
}

void
EcnController::SetBottleneckBps(uint64_t bps)
{
    g_bottleneckBps = bps;
}

uint32_t
EcnController::GetLastOldCwnd()
{
    return g_lastOldCwnd;
}

uint32_t
EcnController::GetLastFormulaCwnd()
{
    return g_lastFormulaCwnd;
}

uint32_t
EcnController::GetLastFinalCwnd()
{
    return g_lastFinalCwnd;
}

uint32_t
EcnController::GetControllerUpdates()
{
    return g_controllerUpdates;
}

bool
EcnController::HasCongControl() const
{
    return true;
}

void
EcnController::PktsAcked(Ptr<TcpSocketState> tcb,
                         uint32_t segmentsAcked,
                         const Time& rtt)
{
    /*
     * Do not allow NewReno to modify CWND.
     *
     * We only use the ACK/RTT information supplied by the TCP
     * machinery to update our BDP measurement.
     */

    (void)segmentsAcked;

    if (rtt.IsZero())
    {
        return;
    }

    BdpMonitor::Update(
        rtt.GetSeconds(),
        tcb->m_segmentSize,
        g_bottleneckBps);
}

uint32_t
EcnController::GetSsThresh(Ptr<const TcpSocketState> tcb,
                           uint32_t bytesInFlight)
{
    /*
     * Required by the ns-3 TCP congestion-control interface.
     *
     * It is deliberately NOT used by our ECN + BDP controller.
     */

    (void)tcb;
    (void)bytesInFlight;

    return UINT32_MAX;
}

void
EcnController::IncreaseWindow(Ptr<TcpSocketState> tcb,
                              uint32_t segmentsAcked)
{
    /*
     * NewReno must NOT control CWND in this project.
     *
     * CWND is controlled by EcnController::CongControl().
     */

    (void)tcb;
    (void)segmentsAcked;
}

void
EcnController::CongControl(
    Ptr<TcpSocketState> tcb,
    const TcpRateOps::TcpRateConnection& rc,
    const TcpRateOps::TcpRateSample& rs)
{
    (void)rc;
    (void)rs;

    uint32_t segmentSize = tcb->m_segmentSize;

    if (segmentSize == 0)
    {
        return;
    }

    uint32_t oldCwnd = tcb->m_cWnd.Get();

    if (oldCwnd < segmentSize)
    {
        oldCwnd = segmentSize;
    }

    uint32_t bdpBytes =
        static_cast<uint32_t>(
            std::min<uint64_t>(
                BdpMonitor::GetBdpBytes(),
                UINT32_MAX));

    /*
     * If BDP has not been measured yet, keep the current CWND.
     */
    if (bdpBytes == 0)
    {
        return;
    }

    /*
     * Never allow the controller's CWND to exceed BDP.
     */
    if (oldCwnd > bdpBytes)
    {
        oldCwnd = bdpBytes;
    }

    uint64_t intervalMarked =
        EcnMonitor::GetIntervalMarkedPackets();

    /*
     * =========================================================
     * ECN REDUCTION
     * =========================================================
     *
     * Only a NEW interval containing ECN marks triggers the
     * multiplicative reduction.
     */
    if (intervalMarked > 0)
    {
        double ecnRatio = EcnMonitor::GetRatio();

        double reductionFactor =
            1.0 - (g_alpha * ecnRatio);

        if (reductionFactor < 0.0)
        {
            reductionFactor = 0.0;
        }

        uint32_t formulaCwnd =
            static_cast<uint32_t>(
                oldCwnd * reductionFactor);

        if (formulaCwnd < segmentSize)
        {
            formulaCwnd = segmentSize;
        }

        uint32_t finalCwnd =
            std::min(formulaCwnd, bdpBytes);

        if (finalCwnd < segmentSize)
        {
            finalCwnd = segmentSize;
        }

        g_lastOldCwnd = oldCwnd;
        g_lastFormulaCwnd = formulaCwnd;
        g_lastFinalCwnd = finalCwnd;

        tcb->m_cWnd = finalCwnd;

        ++g_controllerUpdates;

        EcnMonitor::RecordEcnAck(
            Simulator::Now().GetSeconds());

        std::cout
            << "\n"
            << "==================================================\n"
            << "             ECN + BDP CONTROLLER\n"
            << "==================================================\n"
            << "Controller Update : " << g_controllerUpdates << "\n"
            << "Time              : "
            << std::fixed
            << std::setprecision(8)
            << Simulator::Now().GetSeconds()
            << " s\n"
            << "Interval Packets  : "
            << EcnMonitor::GetIntervalTotalPackets()
            << "\n"
            << "Interval Marked   : "
            << EcnMonitor::GetIntervalMarkedPackets()
            << "\n"
            << "ECN Ratio         : "
            << ecnRatio
            << "\n"
            << "Alpha             : "
            << g_alpha
            << "\n"
            << "RTT               : "
            << BdpMonitor::GetRttSeconds()
            << " s\n"
            << "BDP               : "
            << BdpMonitor::GetBdpBytes()
            << " bytes\n"
            << "BDP               : "
            << BdpMonitor::GetBdpPackets()
            << " packets\n"
            << "Old CWND          : "
            << oldCwnd
            << " bytes\n"
            << "Formula CWND      : "
            << formulaCwnd
            << " bytes\n"
            << "Final CWND        : "
            << finalCwnd
            << " bytes\n"
            << "Action            : ECN REDUCTION\n"
            << "==================================================\n";

        /*
         * Consume this ECN interval.
         * Future marks create a new measurement interval.
         */
        EcnMonitor::ResetInterval();

        return;
    }

    /*
     * =========================================================
     * NORMAL CWND GROWTH
     * =========================================================
     *
     * No ECN marks in the current interval.
     *
     * The controller itself grows CWND toward the measured BDP.
     *
     * One segment per controller growth step.
     */
    uint32_t newCwnd =
        oldCwnd + segmentSize;

    if (newCwnd < oldCwnd)
    {
        newCwnd = UINT32_MAX;
    }

    newCwnd =
        std::min(newCwnd, bdpBytes);

    if (newCwnd < segmentSize)
    {
        newCwnd = segmentSize;
    }

    tcb->m_cWnd = newCwnd;
}

void
EcnController::CwndEvent(
    Ptr<TcpSocketState> tcb,
    const TcpSocketState::TcpCAEvent_t event)
{
    /*
     * Do not delegate congestion-window control to NewReno.
     *
     * These events are only useful for observing the TCP/ECN
     * signalling path.
     */

    (void)tcb;

    if (event == TcpSocketState::CA_EVENT_ECN_IS_CE)
    {
        std::cout
            << "[TCP CE] time="
            << std::fixed
            << std::setprecision(8)
            << Simulator::Now().GetSeconds()
            << " s\n";
    }

    if (event == TcpSocketState::CA_EVENT_COMPLETE_CWR)
    {
        std::cout
            << "[TCP CWR] time="
            << std::fixed
            << std::setprecision(8)
            << Simulator::Now().GetSeconds()
            << " s\n";
    }
}

NS_OBJECT_ENSURE_REGISTERED(EcnController);

}