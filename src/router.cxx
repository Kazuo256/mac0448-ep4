
#include "router.h"
#include "network.h"

#include <utility>
#include <limits>
#include <stack>

using std::numeric_limits;
using std::list;
using std::vector;
using std::string;
using std::stringstream;
using std::cout;
using std::endl;
using std::pair;
using std::make_pair;
using std::map;
using std::tr1::unordered_map;
using std::tr1::unordered_set;
using std::tr1::function;
using std::tr1::bind;
using std::tr1::mem_fn;
using namespace std::tr1::placeholders;
using std::stack;


namespace ep4 {

typedef void (Router::*MsgHandler) (unsigned, stringstream&);

const static pair<string, MsgHandler> handler_list[] = {
  make_pair("HELLO", &Router::acknowledge_hello),
  make_pair("ACK_HELLO", &Router::acknowledge_neighbor),
  make_pair("REQ_LINKSTATE", &Router::respond_linkstate),
  make_pair("ANSWER_LINKSTATE", &Router::receive_linkstate),
  make_pair("ADD_GROUP", &Router::add_group),
  make_pair("JOIN", &Router::handle_join),
  make_pair("BROADCAST", &Router::handle_broadcast),
  make_pair("UNICAST", &Router::handle_unicast),
};

const static pair<string, MsgHandler> *const handler_end =
  handler_list+sizeof(handler_list)/sizeof(pair<string,MsgHandler>);

const static unordered_map<string, MsgHandler> handlers(handler_list,
                                                        handler_end);

const static string sep = " ";

#define INFINITO_UNSIGNED numeric_limits<unsigned>::max()
#define INFINITO_DOUBLE numeric_limits<double>::max()

// Métodos básicos.

void Router::receive_msg (unsigned id_sender, const string& msg) {
  stringstream tokens(msg);
  string header;
  tokens >> header;
  unordered_map<string, MsgHandler>::const_iterator it = handlers.find(header);
  if (it != handlers.end())
    (this->*(it->second))(id_sender, tokens);
}

void Router::make_group (unsigned group_id, bool shared) {
  stringstream msg;
  msg << "ADD_GROUP" << sep << group_id;
  unsigned root;
  if (shared)
    root = biggest_delta();
  else
    root = id();
  add_new_group(group_id, root, id());
  msg << sep << root << sep << id();
  broadcast(msg.str());
}

void Router::join_group (unsigned group_id) {
  stringstream msg;
  msg << "JOIN" << sep << group_id << sep << id();
  unicast(group_sources_[group_id], msg.str());
}

void Router::leave_group (unsigned group_id) {

}

unsigned Router::group_source (unsigned group_id) const {
  return group_sources_.find(group_id)->second;
}

void Router::report_group (unsigned group_id) const {
  unordered_map<unsigned, CrazyStruct>::const_iterator check =
    multicasts_.find(group_id);
  if (check == multicasts_.end()) {
    output() << "You sure about that?" << endl;
    return;
  }
  CrazyStruct const&group_info = check->second;
  cout << std::left << "group " << group_id << ":";
  for (CrazyGuys::const_iterator it = group_info.crazy_guys.begin();
       it != group_info.crazy_guys.end(); ++it) {
    cout << "netid " << it->first << " ";
  }
  cout << endl;
}

// Métodos de bootstrap

void Router::start_up () {
  linkstates_[id_] = LinkState();
  network_->local_broadcast(id_, "HELLO");
}

void Router::linkstate_begin () {
  LinkState &neighbors = linkstates_[id_];
  for (list<Neighbor>::iterator it = neighbors.begin();
       it != neighbors.end(); ++it) {
    stringstream request;
    request << "REQ_LINKSTATE" << sep << id_ << sep << it->id;
    network_->send(id_, it->id, request.str());
    pending_linkstates_.insert(it->id);
  }
}

void Router::make_sptree () {
  ls_route_ms_.resize(linkstates_.size(), INFINITO_UNSIGNED);
  ls_cost_ms_.resize(linkstates_.size(), INFINITO_DOUBLE);
  std::priority_queue<unsigned, vector<unsigned>, std::tr1::function<bool (unsigned, unsigned)> > 
      PQ(bind(&Router::comp_ms, this, _1, _2));
  ls_cost_ms_[id_] = 0.0;
  ls_route_ms_[id_] = id_;
  PQ.push(id_);
  while (!PQ.empty()) {
    unsigned n = PQ.top();
    PQ.pop();
    LinkState& link_n = linkstates_[n];
    for (std::list<Router::Neighbor>::iterator it = link_n.begin(); it != link_n.end(); ++it) {
      double cost = delay(n, it->id);
      if (ls_cost_ms_[it->id] == INFINITO_DOUBLE) {
        ls_cost_ms_[it->id] = ls_cost_ms_[n] + cost;
        ls_route_ms_[it->id] = n;
        PQ.push(it->id);
      } else if (ls_cost_ms_[it->id] > ls_cost_ms_[n] + cost) {
        ls_cost_ms_[it->id] = ls_cost_ms_[n] + cost;
        ls_route_ms_[it->id] = n;
      }
    }
  }
}

// Métodos que tratam mensagens

void Router::acknowledge_hello (unsigned id_sender, stringstream& args) {
  network_->send(id_, id_sender, "ACK_HELLO");
}

void Router::acknowledge_neighbor (unsigned id_sender, stringstream& args) {
  Neighbor  neighbor = { id_sender, network_->get_delay(id_, id_sender) };
  neighbors_[id_sender] = neighbor.delay;
  linkstates_[id_].push_back(neighbor);
  output() << "Acknowledges neighbor " << id_sender << "." << endl;
}

void Router::respond_linkstate (unsigned id_sender, stringstream& args) {
  unsigned id_origin, id_destiny;
  args >> id_origin >> id_destiny;
  if (id_destiny == id_) {
    stringstream answer;
    answer << "ANSWER_LINKSTATE" << sep << id_ << sep << id_origin;
    stack<unsigned> path;
    path.push(id_origin);
    while (!args.eof()) {
      unsigned id;
      args >> id;
      path.push(id);
    }
    unsigned next = path.top();
    path.pop();
    while (path.size() > 1) {
      answer << sep << path.top();
      path.pop();
    }
    if (!path.empty())
      answer << sep << "|";
    LinkState &neighbors = linkstates_[id_];
    for (list<Neighbor>::iterator it = neighbors.begin();
         it != neighbors.end(); ++it)
      answer << sep << it->id << ":" << it->delay;
    network_->send(id_, next, answer.str());
  } else {
    stringstream            request;
    unordered_set<unsigned> invalid;
    invalid.insert(id_origin);
    invalid.insert(id_sender);
    request << "REQ_LINKSTATE" << sep << id_origin << sep << id_destiny;
    output() << "Repassing packet from " << id_sender << endl;
    while (!args.eof()) {
      unsigned id;
      args >> id;
      invalid.insert(id);
      request << sep << id;
    }
    request << sep << id_;
    LinkState &neighbors = linkstates_[id_];
    for (list<Neighbor>::iterator it = neighbors.begin();
         it != neighbors.end(); ++it)
      if (it->id == id_destiny) {
        network_->send(id_, id_destiny, request.str());
        return;
      }
    for (list<Neighbor>::iterator it = neighbors.begin();
         it != neighbors.end(); ++it)
      if (invalid.count(it->id) == 0)
        network_->send(id_, it->id, request.str());
  }
}

void Router::receive_linkstate (unsigned id_sender, stringstream& args) {
  unsigned id_origin, id_destiny;
  args >> id_origin >> id_destiny;
  if (id_destiny == id_) {
    if (linkstates_.count(id_origin) > 0) {
      output() << "Ignoring linkstate answer from "
            << id_origin << endl;
      return;
    }
    pending_linkstates_.erase(id_origin);
    LinkState neighbors;
    // Lê os vizinhos
    while (!args.eof()) {
      string neighbor_data;
      args >> neighbor_data;
      size_t    div = neighbor_data.find(":");
      unsigned  id;
      double    delay;
      stringstream(neighbor_data.substr(0,div)) >> id;
      stringstream(neighbor_data.substr(div+1)) >> delay;
      output() << "Received linkstate from " << id_origin
            << ": neighbor " << id << " delay " << delay << endl;
      Neighbor neighbor = { id, delay };
      neighbors.push_back(neighbor);
    }
    // Manda mais requisições para os que não conhece ainda
    for (LinkState::iterator it = neighbors.begin();
         it != neighbors.end(); ++it)
      if (linkstates_.count(it->id) == 0 &&
          pending_linkstates_.count(it->id) == 0) {
        stringstream request;
        request << "REQ_LINKSTATE" << sep << id_ << sep << it->id;
        network_->local_broadcast(id_, request.str());
        pending_linkstates_.insert(it->id);
      }
    linkstates_[id_origin] = neighbors;
  } else {
    output() << "Repassing answer from " << id_origin
          << " to " << id_destiny << endl;
    stringstream  answer;
    string        token;
    unsigned      next;
    args >> token;
    answer << "ANSWER_LINKSTATE" << sep << id_origin << sep << id_destiny;
    if (token == "|")
      next = id_destiny;
    else {
      stringstream(token) >> next;
      while (token != "|") {
        args >> token;
        answer << sep << token;
      }
    }
    while (args.good()) {
      args >> token;
      answer << sep << token;
    }
    network_->send(id_, next, answer.str());
  }
}

void Router::handle_broadcast (unsigned id_sender, stringstream& args) {
  string msg;
  getline(args, msg);
  receive_msg(id_sender, msg);
  if (cut_broadcast()) return;
  for (unordered_map<unsigned,double>::iterator it = neighbors_.begin();
       it != neighbors_.end(); ++it)
    if (it->first != id_sender)
      network_->send(id(), it->first, args.str());
}

void Router::handle_unicast (unsigned id_sender, stringstream& args) {
  string token;
  args >> token;
  string msg;
  getline(args, msg);
  if (token == "|")
    receive_msg(id_sender, msg);
  else {
    unsigned  next;
    stringstream(token) >> next;
    network_->send(id(), next, "UNICAST"+sep+msg);
  }
}

void Router::add_group (unsigned id_sender, stringstream& args) {
  unsigned group_id, root, transmitter;
  args >> group_id;
  args >> root;
  args >> transmitter;
  if (!add_new_group(group_id, root, transmitter))
    cut_broadcast(true);
}

void Router::handle_join (unsigned id_sender, stringstream& args) {
  unsigned group_id, joiner_id;
  args >> group_id;
  args >> joiner_id;
  unordered_map<unsigned, CrazyStruct>::iterator it = multicasts_.find(group_id);
  if (it == multicasts_.end()) 
    output() << "WARNING: join request with bad group ID." << endl;
  else {
    CrazyGuys &aux = it->second.crazy_guys;
    map<unsigned, unsigned>::iterator crazy_it = aux.find(joiner_id);
    if (crazy_it == aux.end())
      aux.insert(make_pair(joiner_id, 1));
    else
      crazy_it->second++;
  }
}

//== Métodos para calcular rotas ==//

void Router::broadcast (const string& msg) {
  network_->send(id(), id(), msg);
  network_->local_broadcast(id(), "BROADCAST"+sep+msg);
}

void Router::unicast (unsigned id_target, const string& msg) {
  stringstream routing_msg;
  unsigned router = id_target;
  std::stack<unsigned> stack;
  stack.push(id_target);
  while (ls_route_ms_[router] != router) {
    stack.push(ls_route_ms_[router]);
    router = ls_route_ms_[router];
  }
  while (!stack.empty()) {
    routing_msg << sep << stack.top();
    stack.pop();
  }
  routing_msg << sep << "|" << sep << msg;
  stringstream args;
  args << routing_msg;
  handle_unicast(id(), args);
}

bool Router::comp_ms (unsigned id_1, unsigned id_2) const {
  return ls_cost_ms_[id_1] > ls_cost_ms_[id_2];
}

double Router::delay (unsigned origin, unsigned destiny) {
  LinkState& link_origin = linkstates_[origin];
  for (std::list<Router::Neighbor>::iterator it = link_origin.begin(); it != link_origin.end(); ++it)
    if (it->id == destiny && it->delay >= 0.0) {
      return it->delay;
    }
  return 0.0;
}

// Informações de debug

void Router::dump_linkstate_table () const {
  typedef unordered_map<unsigned, LinkState>::const_iterator iterator;
  output() << "Reporting link state table:" << endl;
  for (iterator it = linkstates_.begin(); it != linkstates_.end(); ++it) {
    cout << "(" << it->first << ")";
    for (LinkState::const_iterator nt = it->second.begin();
         nt != it->second.end(); ++nt)
      cout << sep << nt->id << ":" << nt->delay;
    cout << endl;
  }
}

bool Router::add_new_group (unsigned group_id, unsigned source_id,
                            unsigned transmitter_id) {
  unordered_map<unsigned, unsigned>::iterator has = group_sources_.find(group_id);
  if (has == group_sources_.end()) {
    group_sources_.insert(make_pair(group_id, source_id));
    output()  << "Acknowledges multicast group with ID " << group_id
              << "." << endl;
    if (source_id == id()) {
      CrazyStruct crazy;
      crazy.transmitter = transmitter_id;
      multicasts_.insert(make_pair(group_id, crazy));
    }
    return true;
  }
  return false;
}

// Métodos privados

unsigned Router::biggest_delta () {
  unordered_map<unsigned, LinkState>::iterator links_it;
  unsigned max_delta, router_id;
  list<Neighbor>::iterator neighbors_it;
  max_delta = router_id = 0;
  for (links_it = linkstates_.begin(); links_it != linkstates_.end(); ++links_it) {
    LinkState aux = links_it->second;
    unsigned delta = 0;
    for (neighbors_it = aux.begin(); neighbors_it != aux.end(); ++neighbors_it) {
      delta++;
    }
    if (delta > max_delta) {
      max_delta = delta;
      router_id = links_it->first;
    }
  }
  return router_id;
}

} // namespace ep4

