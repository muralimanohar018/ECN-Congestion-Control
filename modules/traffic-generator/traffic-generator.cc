#include "traffic-generator.h"

using namespace ns3;

namespace ecn
{

ApplicationContainer TrafficGenerator::InstallReceiver(
    Ptr<Node> receiver,
    uint16_t port,
    double stopTime)
{
    PacketSinkHelper sinkHelper(
        "ns3::TcpSocketFactory",
        InetSocketAddress(
            Ipv4Address::GetAny(),
            port));

    ApplicationContainer sinkApp =
        sinkHelper.Install(receiver);

    sinkApp.Start(Seconds(0.0));

    sinkApp.Stop(Seconds(stopTime));

    return sinkApp;
}

ApplicationContainer TrafficGenerator::InstallUdpReceiver(
    Ptr<Node> receiver,
    uint16_t port,
    double startTime,
    double stopTime)
{
    PacketSinkHelper sinkHelper(
        "ns3::UdpSocketFactory",
        InetSocketAddress(
            Ipv4Address::GetAny(),
            port));

    ApplicationContainer sinkApp =
        sinkHelper.Install(receiver);

    sinkApp.Start(Seconds(startTime));

    sinkApp.Stop(Seconds(stopTime));

    return sinkApp;
}

ApplicationContainer TrafficGenerator::InstallSender(
    Ptr<Node> sender,
    const Address& destination,
    uint16_t port,
    uint64_t totalBytes,
    uint32_t packetSize,
    double startTime,
    double stopTime)
{
    BulkSendHelper source(
        "ns3::TcpSocketFactory",
        destination);

    source.SetAttribute(
        "MaxBytes",
        UintegerValue(totalBytes));

    source.SetAttribute(
        "SendSize",
        UintegerValue(packetSize));

    ApplicationContainer sourceApp =
        source.Install(sender);

    sourceApp.Start(Seconds(startTime));

    sourceApp.Stop(Seconds(stopTime));

    return sourceApp;
}

}