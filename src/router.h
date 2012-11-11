
#ifndef EP3_ROUTER_H_
#define EP3_ROUTER_H_

#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <vector>
#include <queue>
#include <tr1/unordered_map>
#include <tr1/unordered_set>
#include <tr1/functional>

namespace ep4 {

class Network;
class RouterLogic;

class Router {
  public:
    Router (Network* network, unsigned id, RouterLogic* logic) :
      network_(network), logic_(logic), id_(id) {}
    //== Métodos básicos ==//
    unsigned id () const { return id_; }
    void receive_msg (unsigned id_sender, const std::string& msg);
    //== Métodos de bootstrap ==//
    void start_up ();
    void linkstate_begin ();
    void make_sptree ();
    //== Métodos que tratam mensagens ==//
    void acknowledge_hello (unsigned id_sender, std::stringstream& args);
    void acknowledge_neighbor (unsigned id_sender, std::stringstream& args);
    void respond_linkstate (unsigned id_sender, std::stringstream& args);
    void receive_linkstate (unsigned id_sender, std::stringstream& args);
    void handle_unicast (unsigned id_sender, std::stringstream& args);
    void handle_broadcast (unsigned id_sender, std::stringstream& args);
    void add_group (unsigned id_sender, std::stringstream& args);
    //== Métodos para calcular rotas ==//
    // Usados para estado de enlace:
    void broadcast (const std::string& msg);
    void unicast (unsigned id_target, const std::string& msg);
    double linkstate_route_ms (unsigned id_target, std::vector<unsigned>& route);
    double delay (unsigned origin, unsigned destiny);
    bool comp_ms (unsigned id_1, unsigned id_2) const;
    //== Informações de debug ==//
    void dump_linkstate_table () const;
    // Usados para o multicast
    bool add_new_group (unsigned group_id, unsigned router_id);
  private:
    Network*                                      network_;
    RouterLogic*                                  logic_;
    unsigned                                      id_;
    std::tr1::unordered_map<unsigned, double>     neighbors_;
    bool                                          cut_broadcast_;
    //== Informações de estado de enlace ==//
    struct Neighbor {
      unsigned  id;
      double    delay;
    };
    typedef std::list<Neighbor> LinkState;
    std::tr1::unordered_map<unsigned, LinkState>  linkstates_;
    std::tr1::unordered_set<unsigned>             pending_linkstates_;
    std::vector<unsigned>                         ls_route_ms_;
    std::vector<double>                           ls_cost_ms_;
    //== Outros ==//
    // Método para formatar a saída do roteador.
    std::ostream& output () const {
      return std::cout << "[ROUTER " << id_ << "] ";
    }
    bool cut_broadcast (bool cut = false) {
      bool before = cut_broadcast_;
      cut_broadcast_ = cut;
      return before;
    }
    std::tr1::unordered_map<unsigned, unsigned> groups_;
};

} // namespace ep4

#endif

