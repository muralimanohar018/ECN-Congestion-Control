#include "vbr-generator.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"

using namespace ns3;

namespace ecn
{

ApplicationContainer VbrGenerator::Install(Ptr<Node> sender,const Address& receiver)
{
    OnOffHelper vbr("ns3::UdpSocketFactory",receiver);
    vbr.SetAttribute("DataRate",DataRateValue(DataRate("1Mbps")));
    vbr.SetAttribute("PacketSize",UintegerValue(1000));
    vbr.SetAttribute("OnTime",StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    vbr.SetAttribute("OffTime",StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer app=vbr.Install(sender);
    app.Start(Seconds(0.0));
    app.Stop(Seconds(30.0));

    return app;
}

} // namespace ecn