
#include "ep4.h"
#include "router.h"
#include "network.h"
#include "packet.h"
#include "routerlogic.h"
#include "sharedlogic.h"

#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <tr1/functional>

using std::vector;
using std::string;
using std::stringstream;
using std::getline;
using std::cout;
using std::cin;
using std::endl;
using std::for_each;
using std::tr1::mem_fn;

namespace ep4 {

// Representa um método bootstrap dos roteadores.
typedef void (Router::*Bootstrap) ();

// Lista de métodos bootstrap.
const static Bootstrap bootstrap_list[] = {
  &Router::start_up,
  &Router::linkstate_begin,
  &Router::distvector_begin
};

// Variável auxiliar que aponta para o final da lista acima.
const static Bootstrap *const bootstrap_end =
  bootstrap_list+sizeof(bootstrap_list)/sizeof(Bootstrap);

// Os roteadores.
static vector<Router>     routers;
// A rede.
static Network            network;
// Método usado para 
static RouterLogic*        routerlogic;
// Os métodos de bootstrap para os algoritmos de roteamento.
static vector<Bootstrap>  bootstraps(bootstrap_list,
                                     bootstrap_end);

// Executa um passo da simulação de troca de mensagens, usando o método de
// bootstrap indicado.
static void simulation_step (const Bootstrap& bootstrap_method);

// Trata uma linha de comando do prompt do programa. O retorno indica se o
// prompt deve continuar ou não. Se for falso, o prompt deve terminar.
static bool handle_command (stringstream& command);

void init_simulation (const std::string& topology_file, 
                      const std::string& multicast_type ) {
  if (multicast_type == "SOURCE" || multicast_type == "source") {
    routerlogic = NULL;  
  } else {
    routerlogic = NULL;
  }
  create_network(topology_file);
}

void create_network (const std::string& topology_file) {
  size_t router_num = network.load_topology(topology_file);
  cout << "## Number of routers in the network: " << router_num << endl;
  for (unsigned i = 0; i < router_num; ++i)
    routers.push_back(Router(&network, i, routerlogic));
}

void find_routes () {
  cout << "## Simulating routers' startup message exchange" << endl;
  for_each(bootstraps.begin(), bootstraps.end(), simulation_step);
}

void run_prompt (const string& progname) {
  cout << "## Inicando prompt..." << endl;
  while (cin.good()) {
    cout << progname << "$ ";
    cout.flush();
    string line;
    getline(cin, line);
    stringstream command(line);
    if (line.empty()) continue;
    if (!handle_command(command)) return;
  }
  cout << endl;
}

static void simulate_network () {
  while (network.pending_msgs()) {
    Packet packet = network.next_msg();
    routers[packet.id_receiver].receive_msg(packet.id_sender, packet.msg);
  }
}

static void simulation_step (const Bootstrap& bootstrap_method) {
  for_each(routers.begin(), routers.end(), mem_fn(bootstrap_method));
  // Uma vez executados o métodos de bootstrap, simula a troca de mensagens até
  // não haver mais mensagens enviadas entre os roteadores
  simulate_network();
}

static bool check_id (unsigned id) {
  if (id >= routers.size()) {
    cout << "## No router with ID " << id << "." << endl;
    return false;
  }
  return true;
}

static bool check_args (const stringstream& command) {
  if (command.eof()) {
    cout << "## Missing arguments." << endl;
    return false;
  }
  return true;
}


static void linkstate_route (unsigned id_origin, unsigned id_destiny,
                             const string& metric) {
  typedef double (Router::*RoutingMethod) (unsigned, vector<unsigned>&);
  // Detecta qual algoritmo de roteamento solicitado
  RoutingMethod method = NULL;
  if (metric == "a") method = &Router::linkstate_route_ms;
  else if (metric == "h") method = &Router::linkstate_route_hop;
  else {
    cout << "## Unknown metric '" << metric << "'." << endl;
    return;
  }
  cout << "## Finding route..." << endl;
  vector<unsigned> route;
  double total_delay = (routers[id_origin].*method) (id_destiny, route);
  // E exibimos a rota para o usuário
  for (vector<unsigned>::iterator it = route.begin(); it != route.end(); ++it)
    cout << *it << " ";
  cout << "(";
  if (metric == "h")
    cout << route.size() << " hops";
  else if (metric == "a")
    cout << total_delay << " milisegundos";
  cout << ")" << endl;
}

static void distvector_route (unsigned id_origin, unsigned id_destiny,
                              const string& metric) {
  typedef void (Router::*RoutingMethod) (unsigned);
  // Detecta qual algoritmo de roteamento solicitado
  RoutingMethod method = NULL;
  if (metric == "a") method = &Router::distvector_route_ms;
  else if (metric == "h") method = &Router::distvector_route_hop;
  else {
    cout << "## Unknown metric '" << metric << "'." << endl;
    return;
  }
  cout << "## Finding route..." << endl;
  (routers[id_origin].*method) (id_destiny);
  simulate_network();
  vector<unsigned> route;
  double total_delay = routers[id_destiny].distvector_extract_route(route);
  // E exibimos a rota para o usuário
  for (vector<unsigned>::iterator it = route.begin(); it != route.end(); ++it)
    cout << *it << " ";
  cout << "(";
  if (metric == "h")
    cout << route.size() << " hops";
  else if (metric == "a")
    cout << total_delay << " milisegundos";
  cout << ")" << endl;
}

static unsigned next = 0;

static unsigned next_id () {
  return next++;
}

static bool handle_command (stringstream& command) {
  // Um monte de código feio...
  // Começa verificando a primeira palavra do comando
  string cmd_name;
  command >> cmd_name;
  if (cmd_name == "quit") return false; // Interrompe o prompt
  if (cmd_name == "send") {
    unsigned source_id, group_id = next_id();
    if (!check_args(command)) return true;
    command >> source_id;
    if (!check_id(source_id)) return true;
    //routers[source_id].make_group(group_id);
    cout << "## Multicast group with ID " << group_id << " created." << endl;
  }
  else if (cmd_name == "join") {
    unsigned receiver_id, group_id;
    if (!check_args(command)) return true;
    command >> receiver_id;
    if (!check_id(receiver_id)) return true;
    if (!check_args(command)) return true;
    command >> group_id;
    if (group_id >= next) {
      cout << "## No multicast group with ID " << group_id << "." << endl;
    }
    //routers[receiver_id].join_group(group_id);
  }
  else if (cmd_name == "leave") {
    unsigned receiver_id, group_id;
    if (!check_args(command)) return true;
    command >> receiver_id;
    if (!check_id(receiver_id)) return true;
    if (!check_args(command)) return true;
    command >> group_id;
    if (group_id >= next) {
      cout << "## No multicast group with ID " << group_id << "." << endl;
    }
    //routers[receiver_id].leave_group(group_id);
  }
  else {
    cout << "## Unknown command '" << cmd_name << "'." << endl;
    return true;
  }
  return true;
}

} // namespace ep4

