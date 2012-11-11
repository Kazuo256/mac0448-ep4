
#include "packet.h"

#include <sstream>

using std::string;
using std::ostream;
using std::stringstream;

namespace ep4 {

Packet::operator string () const {
  stringstream stream;
  stream << "[PACKET sender=" << id_sender << " receiver=" << id_receiver
         << " msg=\"" << msg << "\"]";
  return stream.str();
}

ostream& operator << (ostream& os, const Packet& packet) {
  os << static_cast<string>(packet);
  return os;
}

} // namespace ep4

