#ifndef ECN_CONTROLLER_H
#define ECN_CONTROLLER_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-base.h"

namespace ns3
{
namespace ecn
{

class EcnController : public TcpNewReno
{
public:
    static TypeId GetTypeId();

    EcnController();
    EcnController(const EcnController& other);
    ~EcnController() override;

    std::string GetName() const override;
    Ptr<TcpCongestionOps> Fork() override;

    static void SetAlpha(double alpha);
    static double GetAlpha();
    static void SetBottleneckBps(uint64_t bps);

    static uint32_t GetLastOldCwnd();
    static uint32_t GetLastFormulaCwnd();
    static uint32_t GetLastFinalCwnd();
    static uint32_t GetControllerUpdates();

    void PktsAcked(Ptr<TcpSocketState> tcb,uint32_t segmentsAcked,const Time& rtt) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb,uint32_t bytesInFlight) override;
    void IncreaseWindow(Ptr<TcpSocketState> tcb,uint32_t segmentsAcked) override;
    void CwndEvent(Ptr<TcpSocketState> tcb,const TcpSocketState::TcpCAEvent_t event) override;

private:
    static double g_alpha;
    static uint64_t g_bottleneckBps;
    static uint32_t g_lastOldCwnd;
    static uint32_t g_lastFormulaCwnd;
    static uint32_t g_lastFinalCwnd;
    static uint32_t g_controllerUpdates;
};

} // namespace ecn
} // namespace ns3

#endif // ECN_CONTROLLER_H