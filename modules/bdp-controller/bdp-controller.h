#ifndef BDP_CONTROLLER_H
#define BDP_CONTROLLER_H

#include <cstdint>

namespace ecn
{

class BdpController
{
  public:
    static uint32_t Apply(uint32_t formulaCwnd,uint32_t segmentSize);

};

} // namespace ecn

#endif
