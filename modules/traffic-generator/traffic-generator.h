#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"

#include <cstdint>

namespace ecn
{

class TrafficGenerator
{
  public:
    static ns3::ApplicationContainer InstallReceiver(
        ns3::Ptr<ns3::Node> receiver,
        uint16_t port,
        double stopTime);

    static ns3::ApplicationContainer InstallSender(
        ns3::Ptr<ns3::Node> sender,
        const ns3::Address& destination,
        uint16_t port,
        uint64_t totalBytes,
        uint32_t packetSize,
        double startTime,
        double stopTime);
    static ns3::ApplicationContainer InstallUdpReceiver(ns3::Ptr<ns3::Node> receiver, uint16_t port, double startTime, double stopTime);
};

} // namespace ecn

#endif
