
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
  make_pair("DISTVECTOR", &Router::receive_distvector),
  make_pair("ROUTE_MS", &Router::route_ms),
  make_pair("ROUTE_HOP", &Router::route_hop),
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
  distvectors_[id_] = DistVector();
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

void Router::distvector_begin () {
  send_distvector();
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
  Dist      distvector = { neighbor.delay, 1 };
  neighbors_[id_sender] = neighbor.delay;
  linkstates_[id_].push_back(neighbor);
  distvectors_[id_][id_sender] = distvector;
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

void Router::receive_distvector (unsigned id_sender, stringstream& args) {
  output() << "Received distance vector from " << id_sender << endl;
  DistVector& updated = distvectors_[id_sender];
  while (!args.eof()) {
    string token;
    args >> token;
    unsigned  id;
    size_t    div1 = token.find(':', 0),
              div2 = token.find(':', div1+1),
              hops;
    double    delay;
    stringstream(token.substr(0, div1)) >> id;
    stringstream(token.substr(div1+1, div2-div1)) >> delay;
    stringstream(token.substr(div2+1)) >> hops;
    Dist dist = { delay, hops };
    updated[id] = dist;
  }
  bool changed = false;
  DistVector& dists = distvectors_[id_];
  for (DistVector::iterator it = updated.begin(); it != updated.end(); ++it)
    if (it->first != id_) {
      Dist new_dist = {
        neighbors_[id_sender] + it->second.delay,
        1 + it->second.hops
      };
      DistVector::iterator dist = dists.find(it->first);
      if (dist == dists.end())
        dists[it->first] = new_dist,
        changed = true;
      else {
        if (dist->second.delay > new_dist.delay)
          dist->second.delay = new_dist.delay,
          changed = true;
        if (dist->second.hops > new_dist.hops)
          dist->second.hops = new_dist.hops,
          changed = true;
      }
    }
  if (changed)
    send_distvector();
}

void Router::route_ms (unsigned id_sender, stringstream& args) {
  dv_handle_route(args, mem_fn(&Dist::get_delay), "MS");
}

void Router::route_hop (unsigned id_sender, stringstream& args) {
  dv_handle_route(args, mem_fn(&Dist::get_hops), "HOP");
}

//== Métodos para calcular rotas ==//

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

void Router::distvector_route_ms (unsigned id_target) {
  if (id_target == id_) return;
  dv_follow_route(id_target, 0.0, "", mem_fn(&Dist::get_delay), "MS");
}

void Router::distvector_route_hop (unsigned id_target) {
  if (id_target == id_) return;
  dv_follow_route(id_target, 0.0, "", mem_fn(&Dist::get_hops), "HOP");
}

void Router::dv_handle_route (stringstream& args, Metric metric,
                              const string& metric_name) {
  unsigned  id_target;
  double    cost;
  args >> id_target;
  args >> cost;
  if (id_target == id_) {
    lastcost_ = cost;
    while (!args.eof()) {
      unsigned id;
      args >> id;
      lastroute_.push_back(id);
    }
    return;
  }
  string path;
  getline(args, path);
  dv_follow_route(id_target, cost, path, metric, metric_name);
}

void Router::dv_follow_route (unsigned id_target, double cost,
                              const string& path, Metric metric,
                              const string& metric_name) {
  unsigned      next = dv_next_step(id_target, metric);
  stringstream  msg;
  Dist          dist = { neighbors_[next], 1 };
  msg << "ROUTE_" << metric_name
      << sep << id_target
      << sep << (cost+metric(dist))
      << path
      << sep << id_;
  network_->send(id_, next, msg.str());
}

unsigned Router::dv_next_step (unsigned id_target, Metric metric) {
  double    mincost = numeric_limits<double>::max();
  unsigned  next = id_;
  for (unordered_map<unsigned, double>::iterator it = neighbors_.begin();
       it != neighbors_.end(); ++it) {
    Dist dist = { it->second, 1 };
    double cost = metric(dist) + metric(distvectors_[it->first][id_target]);
    if (mincost > cost)
      mincost = cost,
      next = it->first;
  }
  return next;
}

double Router::distvector_extract_route (vector<unsigned>& route) {
  route.insert(route.end(), lastroute_.begin(), lastroute_.end());
  route.push_back(id_);
  double cost = lastcost_;
  lastroute_.clear();
  lastcost_ = 0.0;
  return cost;
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

// Métodos privados

void Router::send_distvector () {
  stringstream msg;
  msg << "DISTVECTOR";
  DistVector& neighbors = distvectors_[id_];
  for (DistVector::const_iterator it = neighbors.begin();
       it != neighbors.end(); ++it)
    msg << sep << it->first
        << ":" << it->second.delay
        << ":" << it->second.hops;
  network_->local_broadcast(id_, msg.str());
}

} // namespace ep4

