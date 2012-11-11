
#ifndef EP4_TIMEDQUEUE_H_
#define EP4_TIMEDQUEUE_H_

#include "packet.h"

#include <vector>

namespace ep4 {

class Packet;

class TransmissionQueue {

  public:

    void insert (const Packet& packet, double delay);
    bool empty () const { return queue_.empty(); }
    const Packet next ();

  private:

    struct Transmission {
      Packet packet;
      double delay;
      bool operator < (const Transmission& rhs) const {
        return delay > rhs.delay; // para fazer um min-heap
      }
      void update (double dt) { delay -= dt; }
    };

    std::vector<Transmission> queue_;

};

} // namespace ep4

#endif

