#ifndef VBR_GENERATOR_H
#define VBR_GENERATOR_H

#include "ns3/applications-module.h"
#include "ns3/network-module.h"

#include <cstdint>

namespace ecn
{

class VbrGenerator
{
public:
    static void SetRateRange(uint32_t minRateMbps, uint32_t maxRateMbps);

    static ns3::ApplicationContainer Install(
        ns3::Ptr<ns3::Node> sender,
        const ns3::Address& receiver,
        uint32_t packetSize,
        double startTime,
        double stopTime);

    static ns3::ApplicationContainer InstallBoth(
        ns3::Ptr<ns3::Node> sender,
        const ns3::Address& receiver1,
        const ns3::Address& receiver2,
        uint32_t packetSize,
        double startTime,
        double stopTime);

private:
    static void ScheduleRateChanges(
        ns3::Ptr<ns3::OnOffApplication> application,
        double startTime,
        double stopTime);

    static uint32_t g_minRateMbps;

    static uint32_t g_maxRateMbps;
};

}

#endif