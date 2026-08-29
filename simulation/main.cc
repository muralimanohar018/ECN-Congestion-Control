#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/red-queue-disc.h"
#include "ns3/traffic-control-module.h"

#include "bdp-monitor.h"
#include "ecn-controller.h"
#include "ecn-monitor.h"
#include "traffic-generator.h"
#include "vbr-generator.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

using namespace ns3;
using namespace ecn;
using namespace std;

static void QueueEnqueueTrace(Ptr<const QueueDiscItem> item)
{
    EcnMonitor::OnEnqueue(item);
}

static void QueueMarkTrace(Ptr<const QueueDiscItem> item, const char* reason)
{
    EcnMonitor::OnMark(item, reason);
}

int main(int argc, char* argv[])
{
    uint64_t packetCount = 100000;
    double simulationTime = 30.0;
    double alpha = 1.0;
    uint32_t packetSize = 1000;
    string tcpLinkRate = "1Gbps";
    string vbrLinkRate = "100Mbps";
    string bottleneckRate = "50Mbps";
    string tcpLinkDelay = "5ms";
    string vbrLinkDelay = "5ms";
    string bottleneckDelay = "50ms";
    string receiverLinkRate = "100Mbps";
    string receiverLinkDelay = "5ms";
    uint32_t redMinTh = 100;
    uint32_t redMaxTh = 200;
    uint32_t queueSize = 300;
    uint32_t vbrMinRate = 5;
    uint32_t vbrMaxRate = 30;

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetCount", "Number of packets for each TCP flow", packetCount);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("alpha", "ECN controller alpha", alpha);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("bottleneckRate", "Shared bottleneck data rate", bottleneckRate);
    cmd.AddValue("redMinTh", "RED minimum threshold in packets", redMinTh);
    cmd.AddValue("redMaxTh", "RED maximum threshold in packets", redMaxTh);
    cmd.AddValue("queueSize", "Maximum queue size in packets", queueSize);
    cmd.AddValue("vbrMinRate", "Minimum VBR rate in Mbps", vbrMinRate);
    cmd.AddValue("vbrMaxRate", "Maximum VBR rate in Mbps", vbrMaxRate);
    cmd.Parse(argc, argv);

    uint64_t totalBytes = packetCount * static_cast<uint64_t>(packetSize);
    uint64_t bottleneckBps = DataRate(bottleneckRate).GetBitRate();

    EcnController::SetAlpha(alpha);
    EcnController::SetBottleneckBps(bottleneckBps);
    VbrGenerator::SetRateRange(vbrMinRate, vbrMaxRate);
    EcnMonitor::Reset();

    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(EcnController::GetTypeId()));

    NodeContainer tcpSenders;
    tcpSenders.Create(3);

    NodeContainer vbrSender;
    vbrSender.Create(1);

    NodeContainer routerNode;
    routerNode.Create(1);

    NodeContainer bottleneckNode;
    bottleneckNode.Create(1);

    NodeContainer receivers;
    receivers.Create(2);

    Ptr<Node> tcp1 = tcpSenders.Get(0);
    Ptr<Node> tcp2 = tcpSenders.Get(1);
    Ptr<Node> tcp3 = tcpSenders.Get(2);
    Ptr<Node> vbr = vbrSender.Get(0);
    Ptr<Node> router = routerNode.Get(0);
    Ptr<Node> bottleneck = bottleneckNode.Get(0);
    Ptr<Node> receiver1 = receivers.Get(0);
    Ptr<Node> receiver2 = receivers.Get(1);

    InternetStackHelper internet;
    internet.Install(tcpSenders);
    internet.Install(vbrSender);
    internet.Install(routerNode);
    internet.Install(bottleneckNode);
    internet.Install(receivers);

    PointToPointHelper tcpLink;
    tcpLink.SetDeviceAttribute("DataRate", StringValue(tcpLinkRate));
    tcpLink.SetChannelAttribute("Delay", StringValue(tcpLinkDelay));

    NetDeviceContainer tcp1Router = tcpLink.Install(tcp1, router);
    NetDeviceContainer tcp2Router = tcpLink.Install(tcp2, router);
    NetDeviceContainer tcp3Router = tcpLink.Install(tcp3, router);

    PointToPointHelper vbrLink;
    vbrLink.SetDeviceAttribute("DataRate", StringValue(vbrLinkRate));
    vbrLink.SetChannelAttribute("Delay", StringValue(vbrLinkDelay));

    NetDeviceContainer vbrRouter = vbrLink.Install(vbr, router);

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NetDeviceContainer routerBottleneck = bottleneckLink.Install(router, bottleneck);

    PointToPointHelper receiverLink;
    receiverLink.SetDeviceAttribute("DataRate", StringValue(receiverLinkRate));
    receiverLink.SetChannelAttribute("Delay", StringValue(receiverLinkDelay));

    NetDeviceContainer bottleneckReceiver1 = receiverLink.Install(bottleneck, receiver1);
    NetDeviceContainer bottleneckReceiver2 = receiverLink.Install(bottleneck, receiver2);

    TrafficControlHelper red;
    red.SetRootQueueDisc("ns3::RedQueueDisc", "LinkBandwidth", StringValue(bottleneckRate), "LinkDelay", StringValue(bottleneckDelay), "MinTh", DoubleValue(redMinTh), "MaxTh", DoubleValue(redMaxTh), "MaxSize", QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)), "UseEcn", BooleanValue(true));

    QueueDiscContainer queueDiscs = red.Install(routerBottleneck);

    Ipv4AddressHelper address;
    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer tcp1If = address.Assign(tcp1Router);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer tcp2If = address.Assign(tcp2Router);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer tcp3If = address.Assign(tcp3Router);

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer vbrIf = address.Assign(vbrRouter);

    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer routerBottleneckIf = address.Assign(routerBottleneck);

    address.SetBase("10.0.6.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver1If = address.Assign(bottleneckReceiver1);

    address.SetBase("10.0.7.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver2If = address.Assign(bottleneckReceiver2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    for (uint32_t i = 0; i < queueDiscs.GetN(); ++i)
    {
        Ptr<QueueDisc> queue = queueDiscs.Get(i);
        queue->TraceConnectWithoutContext("Enqueue", MakeCallback(&QueueEnqueueTrace));
        queue->TraceConnectWithoutContext("Mark", MakeCallback(&QueueMarkTrace));
    }

    cout << "[V2] Queue Enqueue Trace: CONNECTED\n";
    cout << "[V2] Queue Mark Trace: CONNECTED\n";

    uint16_t tcpPort1 = 5001;
    uint16_t tcpPort2 = 5002;
    uint16_t tcpPort3 = 5003;
    uint16_t vbrPort1 = 6001;
    uint16_t vbrPort2 = 6002;

    TrafficGenerator::InstallReceiver(receiver1, tcpPort1, simulationTime);
    TrafficGenerator::InstallReceiver(receiver1, tcpPort2, simulationTime);
    TrafficGenerator::InstallReceiver(receiver2, tcpPort3, simulationTime);

    TrafficGenerator::InstallUdpReceiver(receiver1, vbrPort1, 1.0, simulationTime);
    TrafficGenerator::InstallUdpReceiver(receiver2, vbrPort2, 1.0, simulationTime);

    Address tcp1Destination = InetSocketAddress(receiver1If.GetAddress(1), tcpPort1);
    Address tcp2Destination = InetSocketAddress(receiver1If.GetAddress(1), tcpPort2);
    Address tcp3Destination = InetSocketAddress(receiver2If.GetAddress(1), tcpPort3);

    TrafficGenerator::InstallSender(tcp1, tcp1Destination, tcpPort1, totalBytes, packetSize, 1.0, simulationTime);
    TrafficGenerator::InstallSender(tcp2, tcp2Destination, tcpPort2, totalBytes, packetSize, 1.0, simulationTime);
    TrafficGenerator::InstallSender(tcp3, tcp3Destination, tcpPort3, totalBytes, packetSize, 1.0, simulationTime);

    Address vbrDestination1 = InetSocketAddress(receiver1If.GetAddress(1), vbrPort1);
    Address vbrDestination2 = InetSocketAddress(receiver2If.GetAddress(1), vbrPort2);

    VbrGenerator::InstallBoth(vbr, vbrDestination1, vbrDestination2, packetSize, 1.0, simulationTime);

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    cout << "\n";
    cout << "==================================================\n";
    cout << "       INTELLIGENT ECN CONGESTION CONTROLLER V2\n";
    cout << "==================================================\n";
    cout << "Topology           : " << "3 TCP + 2 VBR -> Router -> " << "50 Mbps Bottleneck -> 2 Receivers\n";
    cout << "TCP Flows          : 3\n";
    cout << "VBR Flows          : 2\n";
    cout << "Controller         : ECN + BDP\n";
    cout << "TCP Base           : NewReno\n";
    cout << "ECN                : ENABLED\n";
    cout << "Queue Management   : RED + ECN\n";
    cout << "TCP -> Router      : " << tcpLinkRate << " / " << tcpLinkDelay << "\n";
    cout << "VBR -> Router      : " << vbrLinkRate << " / " << vbrLinkDelay << "\n";
    cout << "Shared Bottleneck  : " << bottleneckRate << " / " << bottleneckDelay << "\n";
    cout << "Router -> Receiver : " << receiverLinkRate << " / " << receiverLinkDelay << "\n";
    cout << "RED MinTh          : " << redMinTh << " packets\n";
    cout << "RED MaxTh          : " << redMaxTh << " packets\n";
    cout << "Queue Size         : " << queueSize << " packets\n";
    cout << "Alpha              : " << alpha << "\n";
    cout << "Packet Count       : " << packetCount << " per TCP flow\n";
    cout << "Packet Size        : " << packetSize << " bytes\n";
    cout << "VBR Rate Range     : " << vbrMinRate << " - " << vbrMaxRate << " Mbps\n";
    cout << "Simulation Time    : " << simulationTime << " seconds\n";
    cout << "==================================================\n";
    cout << "CWND Formula:\n";
    cout << "CWND_new = min(BDP, " << "CWND_old * (1 - Alpha * ECN_Ratio))\n";
    cout << "BDP Formula:\n";
    cout << "BDP = Bottleneck Bandwidth * RTT\n";
    cout << "==================================================\n";

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();
    flowmon->SerializeToXmlFile("ecn-v2-flowmon.xml", true, true);

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    ofstream csv("ecn-v2-results.csv");
    csv << "FlowId,Source,Destination,Protocol," << "TxPackets,RxPackets,LostPackets," << "PacketLossPercent,TxBytes,RxBytes," << "ThroughputMbps,MeanDelayMs,MeanJitterMs\n";

    ofstream txt("ecn-v2-results.txt");
    txt << "==================================================\n";
    txt << "       INTELLIGENT ECN CONGESTION CONTROLLER V2\n";
    txt << "==================================================\n";
    txt << "Bottleneck Rate    : " << bottleneckRate << "\n";
    txt << "RED MinTh          : " << redMinTh << " packets\n";
    txt << "RED MaxTh          : " << redMaxTh << " packets\n";
    txt << "Queue Size         : " << queueSize << " packets\n";
    txt << "Alpha              : " << alpha << "\n";
    txt << "VBR Rate Range     : " << vbrMinRate << " - " << vbrMaxRate << " Mbps\n";
    txt << "Simulation Time    : " << simulationTime << " seconds\n";
    txt << "\n";
    txt << "Total Packets      : " << EcnMonitor::GetTotalPackets() << "\n";
    txt << "ECN Marked Packets : " << EcnMonitor::GetMarkedPackets() << "\n";
    txt << "ECN Ratio          : " << fixed << setprecision(8) << EcnMonitor::GetCumulativeRatio() << "\n";
    txt << "ECN Percentage     : " << fixed << setprecision(4) << EcnMonitor::GetCumulativeRatio() * 100.0 << " %\n";
    txt << "ECN ACK Count      : " << EcnMonitor::GetEcnAckCount() << "\n";
    txt << "Last ECN ACK Time  : " << EcnMonitor::GetLastEcnAckTime() << " s\n";
    txt << "Controller Updates : " << EcnController::GetControllerUpdates() << "\n";
    txt << "Last Old CWND      : " << EcnController::GetLastOldCwnd() << " bytes\n";
    txt << "Last Formula CWND  : " << EcnController::GetLastFormulaCwnd() << " bytes\n";
    txt << "Last Final CWND    : " << EcnController::GetLastFinalCwnd() << " bytes\n";
    txt << "==================================================\n";

    for (const auto& flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(flow.first);
        FlowMonitor::FlowStats st = flow.second;
        double lossPercent = 0.0;
        double throughputMbps = 0.0;
        double meanDelayMs = 0.0;
        double meanJitterMs = 0.0;

        if (st.txPackets > 0)
        {
            lossPercent = 100.0 * static_cast<double>(st.lostPackets) / static_cast<double>(st.txPackets);
        }

        if (simulationTime > 0.0)
        {
            throughputMbps = static_cast<double>(st.rxBytes) * 8.0 / simulationTime / 1000000.0;
        }

        if (st.rxPackets > 0)
        {
            meanDelayMs = st.delaySum.GetSeconds() / static_cast<double>(st.rxPackets) * 1000.0;
        }

        if (st.rxPackets > 1)
        {
            meanJitterMs = st.jitterSum.GetSeconds() / static_cast<double>(st.rxPackets - 1) * 1000.0;
        }

        cout << "\n";
        cout << "---------------- FLOW " << flow.first << " ----------------\n";
        cout << "Source              : " << tuple.sourceAddress << "\n";
        cout << "Destination         : " << tuple.destinationAddress << "\n";
        cout << "Protocol            : " << static_cast<uint32_t>(tuple.protocol) << "\n";
        cout << "TX Packets          : " << st.txPackets << "\n";
        cout << "RX Packets          : " << st.rxPackets << "\n";
        cout << "Lost Packets        : " << st.lostPackets << "\n";
        cout << "Packet Loss         : " << fixed << setprecision(4) << lossPercent << " %\n";
        cout << "TX Bytes            : " << st.txBytes << "\n";
        cout << "RX Bytes            : " << st.rxBytes << "\n";
        cout << "Throughput          : " << fixed << setprecision(4) << throughputMbps << " Mbps\n";
        cout << "Mean Delay          : " << fixed << setprecision(4) << meanDelayMs << " ms\n";
        cout << "Mean Jitter         : " << fixed << setprecision(4) << meanJitterMs << " ms\n";
        cout << "------------------------------------------\n";

        csv << flow.first << ",";
        csv << tuple.sourceAddress << ",";
        csv << tuple.destinationAddress << ",";
        csv << static_cast<uint32_t>(tuple.protocol) << ",";
        csv << st.txPackets << ",";
        csv << st.rxPackets << ",";
        csv << st.lostPackets << ",";
        csv << fixed << setprecision(4) << lossPercent << ",";
        csv << st.txBytes << ",";
        csv << st.rxBytes << ",";
        csv << fixed << setprecision(4) << throughputMbps << ",";
        csv << fixed << setprecision(4) << meanDelayMs << ",";
        csv << fixed << setprecision(4) << meanJitterMs << "\n";

        txt << "\n";
        txt << "---------------- FLOW " << flow.first << " ----------------\n";
        txt << "Source              : " << tuple.sourceAddress << "\n";
        txt << "Destination         : " << tuple.destinationAddress << "\n";
        txt << "Protocol            : " << static_cast<uint32_t>(tuple.protocol) << "\n";
        txt << "TX Packets          : " << st.txPackets << "\n";
        txt << "RX Packets          : " << st.rxPackets << "\n";
        txt << "Lost Packets        : " << st.lostPackets << "\n";
        txt << "Packet Loss         : " << fixed << setprecision(4) << lossPercent << " %\n";
        txt << "TX Bytes            : " << st.txBytes << "\n";
        txt << "RX Bytes            : " << st.rxBytes << "\n";
        txt << "Throughput          : " << fixed << setprecision(4) << throughputMbps << " Mbps\n";
        txt << "Mean Delay          : " << fixed << setprecision(4) << meanDelayMs << " ms\n";
        txt << "Mean Jitter         : " << fixed << setprecision(4) << meanJitterMs << " ms\n";
        txt << "------------------------------------------\n";
    }

    cout << "\n";
    cout << "==================================================\n";
    cout << "                 FINAL RESULTS\n";
    cout << "==================================================\n";
    cout << "Total Packets      : " << EcnMonitor::GetTotalPackets() << "\n";
    cout << "ECN Marked Packets : " << EcnMonitor::GetMarkedPackets() << "\n";
    cout << "ECN Ratio          : " << fixed << setprecision(8) << EcnMonitor::GetCumulativeRatio() << "\n";
    cout << "ECN Percentage     : " << fixed << setprecision(4) << EcnMonitor::GetCumulativeRatio() * 100.0 << " %\n";
    cout << "ECN ACK Count      : " << EcnMonitor::GetEcnAckCount() << "\n";
    cout << "Last ECN ACK Time  : " << EcnMonitor::GetLastEcnAckTime() << " s\n";
    cout << "Controller Updates : " << EcnController::GetControllerUpdates() << "\n";
    cout << "Last Old CWND      : " << EcnController::GetLastOldCwnd() << " bytes\n";
    cout << "Last Formula CWND  : " << EcnController::GetLastFormulaCwnd() << " bytes\n";
    cout << "Last Final CWND    : " << EcnController::GetLastFinalCwnd() << " bytes\n";
    cout << "==================================================\n";
    cout << "Generated files:\n";
    cout << "  ecn-v2-results.csv\n";
    cout << "  ecn-v2-results.txt\n";
    cout << "  ecn-v2-flowmon.xml\n";
    cout << "==================================================\n";

    txt << "\n";
    txt << "==================================================\n";
    txt << "                 FINAL RESULTS\n";
    txt << "==================================================\n";
    txt << "Total Packets      : " << EcnMonitor::GetTotalPackets() << "\n";
    txt << "ECN Marked Packets : " << EcnMonitor::GetMarkedPackets() << "\n";
    txt << "ECN Ratio          : " << fixed << setprecision(8) << EcnMonitor::GetCumulativeRatio() << "\n";
    txt << "ECN Percentage     : " << fixed << setprecision(4) << EcnMonitor::GetCumulativeRatio() * 100.0 << " %\n";
    txt << "ECN ACK Count      : " << EcnMonitor::GetEcnAckCount() << "\n";
    txt << "Last ECN ACK Time  : " << EcnMonitor::GetLastEcnAckTime() << " s\n";
    txt << "Controller Updates : " << EcnController::GetControllerUpdates() << "\n";
    txt << "Last Old CWND      : " << EcnController::GetLastOldCwnd() << " bytes\n";
    txt << "Last Formula CWND  : " << EcnController::GetLastFormulaCwnd() << " bytes\n";
    txt << "Last Final CWND    : " << EcnController::GetLastFinalCwnd() << " bytes\n";
    txt << "==================================================\n";

    csv.close();
    txt.close();
    Simulator::Destroy();

    return 0;
}