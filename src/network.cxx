
#include "network.h"
#include "packet.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>

using std::max;
using std::vector;
using std::string;
using std::getline;
using std::ostream;
using std::ifstream;
using std::stringstream;

using std::cout;
using std::endl;

namespace ep4 {

Network::Network () {}

size_t Network::load_topology (const string& topology_file) {
  size_t   node_num = 0, count = 0;
  ifstream file(topology_file.c_str(), ifstream::in);

  topology_.clear();
  do {
    string line;
    getline(file, line);
    stringstream stream(line, stringstream::in);
    topology_.push_back(vector<double>());
    while(!stream.eof()) {
      double cost;
      stream >> cost;
      topology_.back().push_back(cost);
    }
    node_num = max(node_num, topology_.back().size());
    count++;
  } while (count < node_num);
  return node_num;
}

double Network::get_delay (unsigned id_sender, unsigned id_receiver) const {
  return topology_[id_sender][id_receiver];
}

void Network::local_broadcast (unsigned id_sender, const string& msg) {
  vector<double> &neighbors = topology_[id_sender];
  for (vector<double>::iterator it = neighbors.begin();
       it != neighbors.end(); ++it)
    send(id_sender, it-neighbors.begin(), msg);
}

void Network::send (unsigned id_sender, unsigned id_receiver,
                    const string& msg) {
  double delay = topology_[id_sender][id_receiver];
  if (id_sender != id_receiver && delay < 0.0)
    return;
  Packet packet = { id_sender, id_receiver, msg };
  cout << packet << endl;
  packets_.insert(packet, delay); 
}

Packet Network::next_msg () {
  return packets_.next();
}

} // namespce ep4

