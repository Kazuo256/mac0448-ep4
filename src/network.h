
#ifndef EP3_NETWORK_H_
#define EP3_NETWORK_H_

#include <string>
#include <vector>
#include <queue>
#include <ostream>
#include <sstream>

namespace ep4 {

class Packet;

class Network {
  public:
    Network ();
    size_t load_topology (const std::string& topology_file);
    double get_delay (unsigned id_sender, unsigned id_receiver) const;
    void local_broadcast (unsigned id_sender, const std::string& msg);
    void send (unsigned id_sender, unsigned id_receiver,
               const std::string& msg);
    bool pending_msgs () const { return !packets_.empty(); }
    Packet next_msg ();
  private:
    typedef std::vector< std::vector<double> > Topology;
    Topology            topology_;
    std::queue<Packet>  packets_;
};

} // namespace ep4

#endif

