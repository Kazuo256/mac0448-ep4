
#ifndef EP4_TIMEDQUEUE_H_
#define EP4_TIMEDQUEUE_H_

#include <vector>
#include <algorithm>

namespace ep4 {

class Packet;

class TransmissionQueue {

  public:

    void insert (const Packet& packet, double delay);
    bool empty () const;
    const Packet next ();

  private:

    std::vector<Packet> queue_;

};

} // namespace ep4

#endif

