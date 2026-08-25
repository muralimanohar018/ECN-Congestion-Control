#include "bdp-monitor.h"
#include "bdp-controller.h"
#include "ecn-monitor.h"
#include "ecn-controller.h"
#include "traffic-generator.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

using namespace ns3;
using namespace ecn;
using namespace ns3;
using namespace ecn;

namespace
{
constexpr uint64_t BOTTLENECK_BPS = 5000000;
constexpr uint32_t DEFAULT_PACKET_SIZE = 1000;
constexpr double DEFAULT_SIMULATION_TIME = 30.0;
constexpr double DEFAULT_MIN_TH = 1.0;
constexpr double DEFAULT_MAX_TH = 3.0;
constexpr uint32_t DEFAULT_QUEUE_PACKETS = 100;
}

int
main(int argc, char* argv[])
{
    uint64_t packetCount = 100000;
    uint32_t packetSize = DEFAULT_PACKET_SIZE;
    double simulationTime = DEFAULT_SIMULATION_TIME;
    double alpha = 1.0;

    double minTh = DEFAULT_MIN_TH;
    double maxTh = DEFAULT_MAX_TH;
    uint32_t queuePackets = DEFAULT_QUEUE_PACKETS;

    CommandLine cmd(__FILE__);

    cmd.AddValue(
        "packetCount",
        "Number of application packets",
        packetCount);

    cmd.AddValue(
        "packetSize",
        "Application packet size",
        packetSize);

    cmd.AddValue(
        "simulationTime",
        "Simulation duration",
        simulationTime);

    cmd.AddValue(
        "alpha",
        "ECN controller alpha",
        alpha);

    cmd.AddValue(
        "minTh",
        "RED minimum threshold in packets",
        minTh);

    cmd.AddValue(
        "maxTh",
        "RED maximum threshold in packets",
        maxTh);

    cmd.AddValue(
        "queuePackets",
        "RED queue size in packets",
        queuePackets);

    cmd.Parse(argc, argv);

    EcnMonitor::Reset();
    BdpMonitor::Reset();

    EcnController::SetAlpha(alpha);
    EcnController::SetBottleneckBps(BOTTLENECK_BPS);

    const uint64_t totalBytes =
        packetCount * static_cast<uint64_t>(packetSize);

    Config::SetDefault(
        "ns3::TcpL4Protocol::SocketType",
        TypeIdValue(EcnController::GetTypeId()));

    Config::SetDefault(
        "ns3::TcpSocketBase::UseEcn",
        EnumValue(TcpSocketState::On));

    NodeContainer nodes;
    nodes.Create(3);

    Ptr<Node> sender = nodes.Get(0);
    Ptr<Node> router = nodes.Get(1);
    Ptr<Node> receiver = nodes.Get(2);

    PointToPointHelper senderRouter;
    senderRouter.SetDeviceAttribute(
        "DataRate",
        StringValue("1Gbps"));
    senderRouter.SetChannelAttribute(
        "Delay",
        StringValue("5ms"));

    NetDeviceContainer senderRouterDevices =
        senderRouter.Install(sender, router);

    PointToPointHelper routerReceiver;
    routerReceiver.SetDeviceAttribute(
        "DataRate",
        StringValue("5Mbps"));
    routerReceiver.SetChannelAttribute(
        "Delay",
        StringValue("50ms"));

    NetDeviceContainer routerReceiverDevices =
        routerReceiver.Install(router, receiver);

    InternetStackHelper internet;
    internet.Install(nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase(
        "10.0.1.0",
        "255.255.255.0");

    Ipv4InterfaceContainer senderRouterInterfaces =
        address.Assign(senderRouterDevices);

    address.SetBase(
        "10.0.2.0",
        "255.255.255.0");

    Ipv4InterfaceContainer routerReceiverInterfaces =
        address.Assign(routerReceiverDevices);

    (void)senderRouterInterfaces;

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    /*
     * Install RED + ECN on the router -> receiver device.
     * The uninstall is kept from the verified implementation to
     * avoid the "root queue disc already exists" fatal error.
     */
    TrafficControlHelper trafficControl;

    trafficControl.Uninstall(
        routerReceiverDevices.Get(0));

    trafficControl.SetRootQueueDisc(
        "ns3::RedQueueDisc",
        "MinTh",
        DoubleValue(minTh),
        "MaxTh",
        DoubleValue(maxTh),
        "MaxSize",
        QueueSizeValue(
            QueueSize(
                std::to_string(queuePackets) + "p")),
        "UseEcn",
        BooleanValue(true));

    QueueDiscContainer queueDiscs =
        trafficControl.Install(
            routerReceiverDevices.Get(0));

    Ptr<QueueDisc> redQueue =
        queueDiscs.Get(0);

    const bool enqueueConnected =
        redQueue->TraceConnectWithoutContext(
            "Enqueue",
            MakeCallback(&EcnMonitor::OnEnqueue));

    const bool markConnected =
        redQueue->TraceConnectWithoutContext(
            "Mark",
            MakeCallback(&EcnMonitor::OnMark));

    std::cout
        << "[FINAL] Queue Enqueue Trace: "
        << (enqueueConnected ? "CONNECTED" : "FAILED")
        << "\n";

    std::cout
        << "[FINAL] Queue Mark Trace: "
        << (markConnected ? "CONNECTED" : "FAILED")
        << "\n";

    const uint16_t port = 5000;

    TrafficGenerator::InstallReceiver(
        receiver,
        port,
        simulationTime);

    Address sinkAddress(
        InetSocketAddress(
            routerReceiverInterfaces.GetAddress(1),
            port));

    TrafficGenerator::InstallSender(
        sender,
        sinkAddress,
        port,
        totalBytes,
        packetSize,
        1.0,
        simulationTime);

    AnimationInterface anim("ecn-final.xml");

    anim.SetConstantPosition(
        sender, 10.0, 30.0);

    anim.SetConstantPosition(
        router, 50.0, 30.0);

    anim.SetConstantPosition(
        receiver, 90.0, 30.0);

    anim.UpdateNodeDescription(
        sender,
        "TCP Sender");

    anim.UpdateNodeDescription(
        router,
        "ECN Router");

    anim.UpdateNodeDescription(
        receiver,
        "TCP Receiver");

    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor =
        flowHelper.InstallAll();

    std::cout
        << "\n"
        << "==================================================\n"
        << "       INTELLIGENT ECN CONGESTION CONTROLLER\n"
        << "==================================================\n"
        << "Topology           : Sender -> Router -> Receiver\n"
        << "Number of Nodes    : 3\n"
        << "Number of Flows    : 1 TCP data flow\n"
        << "TCP Base           : NewReno\n"
        << "Controller         : ECN + BDP\n"
        << "ECN                : ENABLED\n"
        << "Queue Management   : RED + ECN\n"
        << "Sender -> Router   : 1 Gbps / 5 ms\n"
        << "Router -> Receiver : 5 Mbps / 50 ms\n"
        << "RED MinTh          : " << minTh << " packets\n"
        << "RED MaxTh          : " << maxTh << " packets\n"
        << "Queue Size         : " << queuePackets << " packets\n"
        << "Alpha              : " << alpha << "\n"
        << "Packet Count       : " << packetCount << "\n"
        << "Packet Size        : " << packetSize << " bytes\n"
        << "Total Data Target  : " << totalBytes << " bytes\n"
        << "Simulation Time    : " << simulationTime << " seconds\n"
        << "==================================================\n"
        << "CWND Formula:\n"
        << "CWND_new = CWND_old * (1 - Alpha * ECN_Ratio)\n"
        << "BDP Formula:\n"
        << "BDP = Bottleneck Bandwidth * RTT\n"
        << "Final CWND = min(Formula CWND, BDP)\n"
        << "==================================================\n";

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(
            flowHelper.GetClassifier());

    std::map<FlowId, FlowMonitor::FlowStats> stats =
        flowMonitor->GetFlowStats();

    const double finalEcnRatio =
        EcnMonitor::GetRatio();

    const double finalEcnPercentage =
        finalEcnRatio * 100.0;

    std::cout
        << "\n"
        << "==================================================\n"
        << "                 FINAL RESULTS\n"
        << "==================================================\n"
        << "Total Packets      : "
        << EcnMonitor::GetTotalPackets() << "\n"
        << "ECN Marked Packets : "
        << EcnMonitor::GetMarkedPackets() << "\n"
        << "ECN Ratio          : "
        << std::fixed << std::setprecision(8)
        << finalEcnRatio << "\n"
        << "ECN Percentage     : "
        << std::setprecision(4)
        << finalEcnPercentage << " %\n"
        << "ECN ACK Count      : "
        << EcnMonitor::GetEcnAckCount() << "\n"
        << "Last ECN ACK Time  : "
        << EcnMonitor::GetLastEcnAckTime() << " s\n"
        << "Last RTT           : "
        << BdpMonitor::GetRttSeconds() << " s\n"
        << "BDP Bytes          : "
        << BdpMonitor::GetBdpBytes() << "\n"
        << "BDP Packets        : "
        << BdpMonitor::GetBdpPackets() << "\n"
        << "Controller Updates : "
        << EcnController::GetControllerUpdates() << "\n"
        << "Last Old CWND      : "
        << EcnController::GetLastOldCwnd() << " bytes\n"
        << "Last Formula CWND  : "
        << EcnController::GetLastFormulaCwnd() << " bytes\n"
        << "Last Final CWND    : "
        << EcnController::GetLastFinalCwnd() << " bytes\n"
        << "==================================================\n";

    std::ofstream csv("ecn-final-results.csv");

    csv
        << "FlowId,"
        << "Source,"
        << "Destination,"
        << "TxPackets,"
        << "RxPackets,"
        << "LostPackets,"
        << "PacketLossPercent,"
        << "TxBytes,"
        << "RxBytes,"
        << "ThroughputMbps,"
        << "MeanDelayMs,"
        << "MeanJitterMs,"
        << "TotalPackets,"
        << "ECNMarkedPackets,"
        << "ECNRatio,"
        << "ECNPercentage,"
        << "ECNAckCount,"
        << "LastECNAckTime,"
        << "RTTSeconds,"
        << "BDPBytes,"
        << "BDPPackets,"
        << "Alpha,"
        << "OldCWND,"
        << "FormulaCWND,"
        << "FinalCWND\n";

    for (const auto& entry : stats)
    {
        const FlowId flowId = entry.first;
        const FlowMonitor::FlowStats& flowStats = entry.second;

        Ipv4FlowClassifier::FiveTuple tuple =
            classifier->FindFlow(flowId);

        double duration = 0.0;

        if (flowStats.rxPackets > 0)
        {
            duration =
                flowStats.timeLastRxPacket.GetSeconds()
                -
                flowStats.timeFirstTxPacket.GetSeconds();
        }

        double throughput = 0.0;

        if (duration > 0.0)
        {
            throughput =
                (
                    static_cast<double>(
                        flowStats.rxBytes) * 8.0
                )
                /
                duration
                /
                1000000.0;
        }

        double packetLoss = 0.0;

        if (flowStats.txPackets > 0)
        {
            packetLoss =
                static_cast<double>(
                    flowStats.lostPackets)
                /
                static_cast<double>(
                    flowStats.txPackets)
                *
                100.0;
        }

        double meanDelay = 0.0;

        if (flowStats.rxPackets > 0)
        {
            meanDelay =
                flowStats.delaySum.GetSeconds()
                /
                flowStats.rxPackets
                *
                1000.0;
        }

        double meanJitter = 0.0;

        if (flowStats.rxPackets > 1)
        {
            meanJitter =
                flowStats.jitterSum.GetSeconds()
                /
                (flowStats.rxPackets - 1)
                *
                1000.0;
        }

        std::cout
            << "\n---------------- FLOW "
            << flowId
            << " ----------------\n"
            << "Source              : "
            << tuple.sourceAddress << "\n"
            << "Destination         : "
            << tuple.destinationAddress << "\n"
            << "TX Packets          : "
            << flowStats.txPackets << "\n"
            << "RX Packets          : "
            << flowStats.rxPackets << "\n"
            << "Lost Packets        : "
            << flowStats.lostPackets << "\n"
            << "Packet Loss         : "
            << packetLoss << " %\n"
            << "TX Bytes            : "
            << flowStats.txBytes << "\n"
            << "RX Bytes            : "
            << flowStats.rxBytes << "\n"
            << "Throughput          : "
            << throughput << " Mbps\n"
            << "Mean Delay          : "
            << meanDelay << " ms\n"
            << "Mean Jitter         : "
            << meanJitter << " ms\n"
            << "------------------------------------------\n";

        csv
            << flowId << ","
            << tuple.sourceAddress << ","
            << tuple.destinationAddress << ","
            << flowStats.txPackets << ","
            << flowStats.rxPackets << ","
            << flowStats.lostPackets << ","
            << packetLoss << ","
            << flowStats.txBytes << ","
            << flowStats.rxBytes << ","
            << throughput << ","
            << meanDelay << ","
            << meanJitter << ","
            << EcnMonitor::GetTotalPackets() << ","
            << EcnMonitor::GetMarkedPackets() << ","
            << finalEcnRatio << ","
            << finalEcnPercentage << ","
            << EcnMonitor::GetEcnAckCount() << ","
            << EcnMonitor::GetLastEcnAckTime() << ","
            << BdpMonitor::GetRttSeconds() << ","
            << BdpMonitor::GetBdpBytes() << ","
            << BdpMonitor::GetBdpPackets() << ","
            << alpha << ","
            << EcnController::GetLastOldCwnd() << ","
            << EcnController::GetLastFormulaCwnd() << ","
            << EcnController::GetLastFinalCwnd()
            << "\n";
    }

    csv.close();

    flowMonitor->SerializeToXmlFile(
        "ecn-final-flowmon.xml",
        true,
        true);

    Simulator::Destroy();

    std::cout
        << "\n"
        << "==================================================\n"
        << "             FINAL SIMULATION COMPLETE\n"
        << "==================================================\n"
        << "Generated files:\n"
        << "  ecn-final.xml\n"
        << "  ecn-final-results.csv\n"
        << "  ecn-final-flowmon.xml\n"
        << "==================================================\n";

    return 0;
}


/*
 * ns-3 scratch integration:
 * The module implementations are included here so the modular source can
 * remain split in the project repository while still building as one
 * scratch executable without editing ns-3's top-level CMake files.
 */
