#ifndef VBR_GENERATOR_H
#define VBR_GENERATOR_H

#include "ns3/address.h"
#include "ns3/application-container.h"
#include "ns3/node.h"

namespace ecn
{

class VbrGenerator
{
public:
    static ns3::ApplicationContainer Install(ns3::Ptr<ns3::Node> sender,const ns3::Address& receiver);
};

} // namespace ecn

#endif // VBR_GENERATOR_H