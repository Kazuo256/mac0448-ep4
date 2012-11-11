
#include "router.h"
#include "routerlogic.h"
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
  make_pair("ROUTE", &Router::route),
};

const static pair<string, MsgHandler> *const handler_end =
  handler_list+sizeof(handler_list)/sizeof(pair<string,MsgHandler>);

const static unordered_map<string, MsgHandler> handlers(handler_list,
                                                        handler_end);

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
  else
    logic_->handle_msg(id(), id_sender, header, tokens);
}

// Métodos de bootstrap

const static string sep = " ";

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

void Router::route (unsigned id_sender, stringstream& args) {
  string token;
  args >> token;
  string msg;
  getline(args, msg);
  if (token == "|")
    receive_msg(id_sender, msg);
  else {
    unsigned  next;
    stringstream(token) >> next;
    network_->send(id(), next, msg);
  }
}

void Router::add_group (unsigned id_sender, stringstream& args) {

}

//== Métodos para calcular rotas ==//

void Router::broadcast (const string& msg) {
  network_->local_broadcast(id(), msg);
}

void Router::route_msg (unsigned id_target, const string& msg) {
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
  route(id(), args);
}

bool Router::comp_ms (unsigned id_1, unsigned id_2) const {
  return ls_cost_ms_[id_1] > ls_cost_ms_[id_2];
}

bool Router::comp_hop (unsigned id_1, unsigned id_2) const {
  return ls_cost_hop_[id_1] > ls_cost_hop_[id_2];
}

double Router::delay (unsigned origin, unsigned destiny) {
  LinkState& link_origin = linkstates_[origin];
  for (std::list<Router::Neighbor>::iterator it = link_origin.begin(); it != link_origin.end(); ++it)
    if (it->id == destiny && it->delay >= 0.0) {
      return it->delay;
    }
  return 0.0;
}

double Router::linkstate_route_ms (unsigned id_target, vector<unsigned>& route) {
  if (ls_route_ms_.empty() && ls_cost_ms_.empty()) {
    ls_route_ms_.resize(linkstates_.size(), INFINITO_UNSIGNED);
    ls_cost_ms_.resize(linkstates_.size(), INFINITO_DOUBLE);
  }
  if (ls_route_ms_[id_target] == INFINITO_UNSIGNED) {
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
  unsigned router = id_target;
  std::stack<unsigned> stack;
  stack.push(id_target);
  while (ls_route_ms_[router] != router) {
    stack.push(ls_route_ms_[router]);
    router = ls_route_ms_[router];
  }
  while (!stack.empty()) {
    route.push_back(stack.top());
    stack.pop();
  }
  return ls_cost_ms_[id_target];
}

double Router::linkstate_route_hop (unsigned id_target, vector<unsigned>& route) {
  if (ls_route_hop_.empty() && ls_cost_hop_.empty()) {
    ls_route_hop_.resize(linkstates_.size(), INFINITO_UNSIGNED);
    ls_cost_hop_.resize(linkstates_.size(), INFINITO_DOUBLE);
  }
  if (ls_route_hop_[id_target] == INFINITO_UNSIGNED) {
    std::priority_queue<unsigned, vector<unsigned>, std::tr1::function<bool (unsigned, unsigned)> > 
        PQ(bind(&Router::comp_hop, this, _1, _2));
    ls_cost_hop_[id_] = 0.0;
    ls_route_hop_[id_] = id_;
    PQ.push(id_);
    while (!PQ.empty()) {
      unsigned n = PQ.top();
      PQ.pop();
      LinkState& link_n = linkstates_[n];
      for (std::list<Router::Neighbor>::iterator it = link_n.begin(); it != link_n.end(); ++it) {
        if (ls_cost_hop_[it->id] == INFINITO_DOUBLE) {
          ls_cost_hop_[it->id] = ls_cost_hop_[n] + 1;
          ls_route_hop_[it->id] = n;
          PQ.push(it->id);
        } else if (ls_cost_hop_[it->id] > ls_cost_hop_[n] + 1) {
          ls_cost_hop_[it->id] = ls_cost_hop_[n] + 1;
          ls_route_hop_[it->id] = n;
        }
      }
    }
  }
  unsigned router = id_target;
  std::stack<unsigned> stack;
  stack.push(id_target);
  while (ls_route_hop_[router] != router) {
    stack.push(ls_route_hop_[router]);
    router = ls_route_hop_[router];
  }
  while (!stack.empty()) {
    route.push_back(stack.top());
    stack.pop();
  }
  return ls_cost_hop_[id_target];
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

bool Router::add_new_group (unsigned group_id, unsigned router_id) {
  unordered_map<unsigned, unsigned>::iterator has = groups_.find(group_id);
  if (has == groups_.end()) {
    groups_.insert(make_pair(group_id, router_id));
    return true;
  }
  return false;
}

// Métodos privados


} // namespace ep4

