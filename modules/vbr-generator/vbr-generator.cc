#include "vbr-generator.h"

#include "ns3/core-module.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

using namespace ns3;
using namespace std;

namespace ecn
{

uint32_t VbrGenerator::g_minRateMbps = 5;

uint32_t VbrGenerator::g_maxRateMbps = 30;

void VbrGenerator::SetRateRange(
    uint32_t minRateMbps,
    uint32_t maxRateMbps)
{
    g_minRateMbps = minRateMbps;

    g_maxRateMbps = maxRateMbps;

    if (g_minRateMbps > g_maxRateMbps)
    {
        swap(g_minRateMbps, g_maxRateMbps);
    }
}

void VbrGenerator::ScheduleRateChanges(
    Ptr<OnOffApplication> application,
    double startTime,
    double stopTime)
{
    Ptr<UniformRandomVariable> random =
        CreateObject<UniformRandomVariable>();

    double currentTime = startTime;

    while (currentTime < stopTime)
    {
        double nextTime =
            min(currentTime + 1.0, stopTime);

        uint32_t rateMbps =
            random->GetInteger(
                g_minRateMbps,
                g_maxRateMbps);

        Simulator::Schedule(
            Seconds(currentTime),
            [application, rateMbps]()
            {
                string rate =
                    to_string(rateMbps) + "Mbps";

                application->SetAttribute(
                    "DataRate",
                    DataRateValue(
                        DataRate(rate)));

                cout << "[VBR] time="
                     << fixed
                     << setprecision(2)
                     << Simulator::Now().GetSeconds()
                     << " s | rate="
                     << rate
                     << "\n";
            });

        currentTime = nextTime;
    }
}

ApplicationContainer VbrGenerator::Install(
    Ptr<Node> sender,
    const Address& receiver,
    uint32_t packetSize,
    double startTime,
    double stopTime)
{
    OnOffHelper vbr(
        "ns3::UdpSocketFactory",
        receiver);

    vbr.SetAttribute(
        "DataRate",
        DataRateValue(
            DataRate("5Mbps")));

    vbr.SetAttribute(
        "PacketSize",
        UintegerValue(packetSize));

    vbr.SetAttribute(
        "OnTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=1]"));

    vbr.SetAttribute(
        "OffTime",
        StringValue(
            "ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps =
        vbr.Install(sender);

    Ptr<OnOffApplication> application =
        DynamicCast<OnOffApplication>(
            apps.Get(0));

    ScheduleRateChanges(
        application,
        startTime,
        stopTime);

    apps.Start(Seconds(startTime));

    apps.Stop(Seconds(stopTime));

    return apps;
}

ApplicationContainer VbrGenerator::InstallBoth(
    Ptr<Node> sender,
    const Address& receiver1,
    const Address& receiver2,
    uint32_t packetSize,
    double startTime,
    double stopTime)
{
    ApplicationContainer apps;

    apps.Add(
        Install(
            sender,
            receiver1,
            packetSize,
            startTime,
            stopTime));

    apps.Add(
        Install(
            sender,
            receiver2,
            packetSize,
            startTime,
            stopTime));

    return apps;
}

}