
#ifndef EP4_PACKET_H_
#define EP4_PACKET_H_

#include <string>
#include <ostream>

namespace ep4 {

struct Packet {
  operator std::string () const;
  unsigned    id_sender,
              id_receiver;
  std::string msg;
};

std::ostream& operator << (std::ostream& os, const Packet& packet);

} // namespace ep4

#endif

