#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/red-queue-disc.h"
#include "ns3/traffic-control-module.h"

#include "vbr-generator.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace ns3;
using namespace ecn;
using namespace std;

static ofstream g_cwndFile;
static ofstream g_rttFile;
static uint64_t g_cwndUpdates = 0;
static uint64_t g_rttUpdates = 0;
static double g_lastCwndTime[3] = {-1.0, -1.0, -1.0};

static void CwndTrace(uint32_t flowId, uint32_t oldCwnd, uint32_t newCwnd)
{
    double now = Simulator::Now().GetSeconds();
    double interval = 0.0;
    double rate = 0.0;

    if (g_lastCwndTime[flowId - 1] >= 0.0)
    {
        interval = now - g_lastCwndTime[flowId - 1];

        if (interval > 0.0)
        {
            rate = (static_cast<double>(newCwnd) - static_cast<double>(oldCwnd)) / interval;
        }
    }

    g_lastCwndTime[flowId - 1] = now;
    ++g_cwndUpdates;

    cout << "[CWND] Flow=" << flowId
         << " Time=" << fixed << setprecision(8) << now
         << " s | Old=" << oldCwnd
         << " bytes | New=" << newCwnd
         << " bytes | Change=" << static_cast<int64_t>(newCwnd) - static_cast<int64_t>(oldCwnd)
         << " bytes | Interval=" << interval
         << " s | Rate=" << rate
         << " bytes/s\n";

    if (g_cwndFile.is_open())
    {
        g_cwndFile << flowId << "," << fixed << setprecision(8) << now << "," << oldCwnd << "," << newCwnd << "," << static_cast<int64_t>(newCwnd) - static_cast<int64_t>(oldCwnd) << "," << interval << "," << rate << "\n";
    }
}

static void RttTrace(uint32_t flowId, Time oldRtt, Time newRtt)
{
    double now = Simulator::Now().GetSeconds();
    ++g_rttUpdates;

    cout << "[RTT] Flow=" << flowId
         << " Time=" << fixed << setprecision(8) << now
         << " s | Old=" << oldRtt.GetSeconds()
         << " s | New=" << newRtt.GetSeconds()
         << " s\n";

    if (g_rttFile.is_open())
    {
        g_rttFile << flowId << "," << fixed << setprecision(8) << now << "," << oldRtt.GetSeconds() << "," << newRtt.GetSeconds() << "\n";
    }
}

static void ConnectTcpTraces(Ptr<Node> node, uint32_t flowId)
{
    Ptr<TcpL4Protocol> tcp = node->GetObject<TcpL4Protocol>();

    if (!tcp)
    {
        return;
    }

    ObjectVectorValue sockets;
    tcp->GetAttribute("SocketList", sockets);

    for (uint32_t i = 0; i < sockets.GetN(); ++i)
    {
        Ptr<Object> object = sockets.Get(i);

        Ptr<TcpSocketBase> socket = DynamicCast<TcpSocketBase>(object);

        if (!socket)
        {
            continue;
        }

        socket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndTrace, flowId));
        socket->TraceConnectWithoutContext("RTT", MakeBoundCallback(&RttTrace, flowId));
    }
}

static void ConnectAllTcpTraces(Ptr<Node> node1, Ptr<Node> node2, Ptr<Node> node3)
{
    ConnectTcpTraces(node1, 1);
    ConnectTcpTraces(node2, 2);
    ConnectTcpTraces(node3, 3);
}

int main(int argc, char* argv[])
{
    string tcpType = "NewReno";

    uint64_t packetCount = 100000;
    uint32_t packetSize = 1000;
    double simulationTime = 30.0;

    string tcpLinkRate = "1Gbps";
    string tcpLinkDelay = "5ms";

    string vbrLinkRate = "100Mbps";
    string vbrLinkDelay = "5ms";

    string bottleneckRate = "50Mbps";
    string bottleneckDelay = "50ms";

    string receiverLinkRate = "100Mbps";
    string receiverLinkDelay = "5ms";

    uint32_t redMinTh = 50;
    uint32_t redMaxTh = 100;
    uint32_t queueSize = 300;

    uint32_t vbrMinRate = 10;
    uint32_t vbrMaxRate = 30;

    bool useEcn = true;
    bool sack = true;
    bool timestamp = true;
    bool windowScaling = true;
    bool limitedTransmit = true;

    uint32_t runNumber = 1;

    CommandLine cmd(__FILE__);

    cmd.AddValue("tcpType", "TCP congestion-control model: NewReno, CUBIC, BBR, or DCTCP", tcpType);
    cmd.AddValue("packetCount", "Number of packets per TCP flow", packetCount);
    cmd.AddValue("packetSize", "Application packet size in bytes", packetSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);

    cmd.AddValue("tcpLinkRate", "TCP sender access-link rate", tcpLinkRate);
    cmd.AddValue("tcpLinkDelay", "TCP sender access-link delay", tcpLinkDelay);

    cmd.AddValue("vbrLinkRate", "VBR sender access-link rate", vbrLinkRate);
    cmd.AddValue("vbrLinkDelay", "VBR sender access-link delay", vbrLinkDelay);

    cmd.AddValue("bottleneckRate", "Shared bottleneck rate", bottleneckRate);
    cmd.AddValue("bottleneckDelay", "Shared bottleneck delay", bottleneckDelay);

    cmd.AddValue("receiverLinkRate", "Receiver-link rate", receiverLinkRate);
    cmd.AddValue("receiverLinkDelay", "Receiver-link delay", receiverLinkDelay);

    cmd.AddValue("redMinTh", "RED minimum threshold in packets", redMinTh);
    cmd.AddValue("redMaxTh", "RED maximum threshold in packets", redMaxTh);
    cmd.AddValue("queueSize", "RED queue size in packets", queueSize);

    cmd.AddValue("vbrMinRate", "Minimum VBR rate in Mbps", vbrMinRate);
    cmd.AddValue("vbrMaxRate", "Maximum VBR rate in Mbps", vbrMaxRate);

    cmd.AddValue("useEcn", "Enable TCP ECN", useEcn);
    cmd.AddValue("sack", "Enable TCP SACK", sack);
    cmd.AddValue("timestamp", "Enable TCP timestamps", timestamp);
    cmd.AddValue("windowScaling", "Enable TCP window scaling", windowScaling);
    cmd.AddValue("limitedTransmit", "Enable TCP limited transmit", limitedTransmit);

    cmd.AddValue("runNumber", "Experiment run number", runNumber);

    cmd.Parse(argc, argv);

    RngSeedManager::SetRun(runNumber);

    if (tcpType == "NewReno")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
    }
    else if (tcpType == "BBR")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    }
    else if (tcpType == "CUBIC")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    }
    else if (tcpType == "DCTCP")
    {
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpDctcp::GetTypeId()));
    }
    else
    {
        cerr << "ERROR: tcpType must be NewReno, CUBIC, BBR, or DCTCP\n";
        return 1;
    }

    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue(useEcn ? "On" : "Off"));
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(sack));
    Config::SetDefault("ns3::TcpSocketBase::Timestamp", BooleanValue(timestamp));
    Config::SetDefault("ns3::TcpSocketBase::WindowScaling", BooleanValue(windowScaling));
    Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(limitedTransmit));

    NodeContainer tcpSenders;
    tcpSenders.Create(3);

    NodeContainer vbrSender;
    vbrSender.Create(1);

    NodeContainer router;
    router.Create(1);

    NodeContainer bottleneck;
    bottleneck.Create(1);

    NodeContainer receivers;
    receivers.Create(2);

    InternetStackHelper internet;
    internet.Install(tcpSenders);
    internet.Install(vbrSender);
    internet.Install(router);
    internet.Install(bottleneck);
    internet.Install(receivers);

    PointToPointHelper tcpLink;
    tcpLink.SetDeviceAttribute("DataRate", StringValue(tcpLinkRate));
    tcpLink.SetChannelAttribute("Delay", StringValue(tcpLinkDelay));

    NetDeviceContainer tcp1Router = tcpLink.Install(tcpSenders.Get(0), router.Get(0));
    NetDeviceContainer tcp2Router = tcpLink.Install(tcpSenders.Get(1), router.Get(0));
    NetDeviceContainer tcp3Router = tcpLink.Install(tcpSenders.Get(2), router.Get(0));

    PointToPointHelper vbrLink;
    vbrLink.SetDeviceAttribute("DataRate", StringValue(vbrLinkRate));
    vbrLink.SetChannelAttribute("Delay", StringValue(vbrLinkDelay));

    NetDeviceContainer vbrRouter = vbrLink.Install(vbrSender.Get(0), router.Get(0));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue(bottleneckRate));
    bottleneckLink.SetChannelAttribute("Delay", StringValue(bottleneckDelay));

    NetDeviceContainer routerBottleneck = bottleneckLink.Install(router.Get(0), bottleneck.Get(0));

    PointToPointHelper receiverLink;
    receiverLink.SetDeviceAttribute("DataRate", StringValue(receiverLinkRate));
    receiverLink.SetChannelAttribute("Delay", StringValue(receiverLinkDelay));

    NetDeviceContainer bottleneckReceiver1 = receiverLink.Install(bottleneck.Get(0), receivers.Get(0));
    NetDeviceContainer bottleneckReceiver2 = receiverLink.Install(bottleneck.Get(0), receivers.Get(1));

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
    Ipv4InterfaceContainer bottleneckIf = address.Assign(routerBottleneck);

    address.SetBase("10.0.6.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver1If = address.Assign(bottleneckReceiver1);

    address.SetBase("10.0.7.0", "255.255.255.0");
    Ipv4InterfaceContainer receiver2If = address.Assign(bottleneckReceiver2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t tcpPort1 = 5001;
    uint16_t tcpPort2 = 5002;
    uint16_t tcpPort3 = 5003;

    uint16_t vbrPort1 = 6001;
    uint16_t vbrPort2 = 6002;

    PacketSinkHelper tcpSink1("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort1));
    PacketSinkHelper tcpSink2("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort2));
    PacketSinkHelper tcpSink3("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort3));

    ApplicationContainer sinkApps;
    sinkApps.Add(tcpSink1.Install(receivers.Get(0)));
    sinkApps.Add(tcpSink2.Install(receivers.Get(0)));
    sinkApps.Add(tcpSink3.Install(receivers.Get(1)));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    PacketSinkHelper udpSink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), vbrPort1));
    PacketSinkHelper udpSink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), vbrPort2));

    ApplicationContainer udpSinkApps;
    udpSinkApps.Add(udpSink1.Install(receivers.Get(0)));
    udpSinkApps.Add(udpSink2.Install(receivers.Get(1)));
    udpSinkApps.Start(Seconds(0.0));
    udpSinkApps.Stop(Seconds(simulationTime));

    Address tcpDestination1 = InetSocketAddress(receiver1If.GetAddress(1), tcpPort1);
    Address tcpDestination2 = InetSocketAddress(receiver1If.GetAddress(1), tcpPort2);
    Address tcpDestination3 = InetSocketAddress(receiver2If.GetAddress(1), tcpPort3);

    BulkSendHelper tcpSource1("ns3::TcpSocketFactory", tcpDestination1);
    tcpSource1.SetAttribute("MaxBytes", UintegerValue(packetCount * static_cast<uint64_t>(packetSize)));
    tcpSource1.SetAttribute("SendSize", UintegerValue(packetSize));

    BulkSendHelper tcpSource2("ns3::TcpSocketFactory", tcpDestination2);
    tcpSource2.SetAttribute("MaxBytes", UintegerValue(packetCount * static_cast<uint64_t>(packetSize)));
    tcpSource2.SetAttribute("SendSize", UintegerValue(packetSize));

    BulkSendHelper tcpSource3("ns3::TcpSocketFactory", tcpDestination3);
    tcpSource3.SetAttribute("MaxBytes", UintegerValue(packetCount * static_cast<uint64_t>(packetSize)));
    tcpSource3.SetAttribute("SendSize", UintegerValue(packetSize));

    ApplicationContainer tcpApps;
    tcpApps.Add(tcpSource1.Install(tcpSenders.Get(0)));
    tcpApps.Add(tcpSource2.Install(tcpSenders.Get(1)));
    tcpApps.Add(tcpSource3.Install(tcpSenders.Get(2)));
    tcpApps.Start(Seconds(1.0));
    tcpApps.Stop(Seconds(simulationTime));

    Address vbrDestination1 = InetSocketAddress(receiver1If.GetAddress(1), vbrPort1);
    Address vbrDestination2 = InetSocketAddress(receiver2If.GetAddress(1), vbrPort2);

    VbrGenerator::SetRateRange(vbrMinRate, vbrMaxRate);
    ApplicationContainer vbrApps = VbrGenerator::InstallBoth(vbrSender.Get(0), vbrDestination1, vbrDestination2, packetSize, 1.0, simulationTime);

    string prefix = "baseline-" + tcpType + "-run" + to_string(runNumber);

    g_cwndFile.open(prefix + "-cwnd.csv");
    g_rttFile.open(prefix + "-rtt.csv");

    g_cwndFile << "Flow,Time,OldCWND,NewCWND,CWNDChange,UpdateInterval,CWNDChangeRate\n";
    g_rttFile << "Flow,Time,OldRTT,NewRTT\n";

    cout << "\n==================================================\n";
    cout << "          STANDALONE TCP BASELINE\n";
    cout << "==================================================\n";
    cout << "TCP Model          : " << tcpType << "\n";
    cout << "V3 Controller      : DISABLED\n";
    cout << "BDP Controller     : DISABLED\n";
    cout << "Custom ECN Logic   : DISABLED\n";
    cout << "TCP Flows          : 3\n";
    cout << "VBR Flows          : 2\n";
    cout << "Packet Count       : " << packetCount << " per TCP flow\n";
    cout << "Packet Size        : " << packetSize << " bytes\n";
    cout << "Bottleneck         : " << bottleneckRate << " / " << bottleneckDelay << "\n";
    cout << "RED MinTh          : " << redMinTh << " packets\n";
    cout << "RED MaxTh          : " << redMaxTh << " packets\n";
    cout << "Queue Size         : " << queueSize << " packets\n";
    cout << "VBR Rate           : " << vbrMinRate << " - " << vbrMaxRate << " Mbps\n";
    cout << "TCP ECN            : " << (useEcn ? "ON" : "OFF") << "\n";
    cout << "==================================================\n";

    Simulator::Schedule(Seconds(1.01), &ConnectAllTcpTraces, tcpSenders.Get(0), tcpSenders.Get(1), tcpSenders.Get(2));

    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    flowmon->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    ofstream resultCsv(prefix + "-results.csv");
    ofstream resultTxt(prefix + "-results.txt");

    resultCsv << "FlowId,Source,Destination,Protocol,TxPackets,RxPackets,LostPackets,PacketLossPercent,TxBytes,RxBytes,ThroughputMbps,MeanDelayMs,MeanJitterMs\n";

    cout << "\n==================================================\n";
    cout << "                 FINAL RESULTS\n";
    cout << "==================================================\n";

    for (const auto& entry : stats)
    {
        Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(entry.first);
        FlowMonitor::FlowStats st = entry.second;

        double loss = st.txPackets > 0 ? 100.0 * static_cast<double>(st.lostPackets) / static_cast<double>(st.txPackets) : 0.0;
        double throughput = simulationTime > 0.0 ? static_cast<double>(st.rxBytes) * 8.0 / simulationTime / 1000000.0 : 0.0;
        double delay = st.rxPackets > 0 ? st.delaySum.GetSeconds() / static_cast<double>(st.rxPackets) * 1000.0 : 0.0;
        double jitter = st.rxPackets > 1 ? st.jitterSum.GetSeconds() / static_cast<double>(st.rxPackets - 1) * 1000.0 : 0.0;

        cout << "\n---------------- FLOW " << entry.first << " ----------------\n";
        cout << "Source              : " << tuple.sourceAddress << "\n";
        cout << "Destination         : " << tuple.destinationAddress << "\n";
        cout << "Protocol            : " << static_cast<uint32_t>(tuple.protocol) << "\n";
        cout << "TX Packets          : " << st.txPackets << "\n";
        cout << "RX Packets          : " << st.rxPackets << "\n";
        cout << "Lost Packets        : " << st.lostPackets << "\n";
        cout << "Packet Loss         : " << fixed << setprecision(4) << loss << " %\n";
        cout << "TX Bytes            : " << st.txBytes << "\n";
        cout << "RX Bytes            : " << st.rxBytes << "\n";
        cout << "Throughput          : " << fixed << setprecision(4) << throughput << " Mbps\n";
        cout << "Mean Delay          : " << fixed << setprecision(4) << delay << " ms\n";
        cout << "Mean Jitter         : " << fixed << setprecision(4) << jitter << " ms\n";
        cout << "------------------------------------------\n";

        resultCsv << entry.first << "," << tuple.sourceAddress << "," << tuple.destinationAddress << "," << static_cast<uint32_t>(tuple.protocol) << "," << st.txPackets << "," << st.rxPackets << "," << st.lostPackets << "," << fixed << setprecision(4) << loss << "," << st.txBytes << "," << st.rxBytes << "," << throughput << "," << delay << "," << jitter << "\n";
    }

    flowmon->SerializeToXmlFile(prefix + "-flowmon.xml", true, true);

    resultTxt << "TCP Model: " << tcpType << "\n";
    resultTxt << "Packet Count: " << packetCount << "\n";
    resultTxt << "Packet Size: " << packetSize << " bytes\n";
    resultTxt << "Bottleneck: " << bottleneckRate << "\n";
    resultTxt << "RED MinTh: " << redMinTh << " packets\n";
    resultTxt << "RED MaxTh: " << redMaxTh << " packets\n";
    resultTxt << "Queue Size: " << queueSize << " packets\n";
    resultTxt << "VBR Rate: " << vbrMinRate << " - " << vbrMaxRate << " Mbps\n";

    cout << "\n==================================================\n";
    cout << "             BASELINE COMPLETED\n";
    cout << "==================================================\n";
    cout << "CWND Updates       : " << g_cwndUpdates << "\n";
    cout << "RTT Updates        : " << g_rttUpdates << "\n";
    cout << "CWND CSV           : " << prefix << "-cwnd.csv\n";
    cout << "RTT CSV            : " << prefix << "-rtt.csv\n";
    cout << "Results CSV        : " << prefix << "-results.csv\n";
    cout << "Results TXT        : " << prefix << "-results.txt\n";
    cout << "FlowMonitor XML    : " << prefix << "-flowmon.xml\n";
    cout << "==================================================\n";

    resultCsv.close();
    resultTxt.close();
    g_cwndFile.close();
    g_rttFile.close();

    Simulator::Destroy();

    return 0;
}