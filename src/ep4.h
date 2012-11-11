
#ifndef EP4_H_
#define EP4_H_

#include <string>

namespace ep4 {

/// Cria a rede e sua topologia a partir do arquivo passado e determina o
/// tipo de grupo multicast usado.
void init_simulation (const std::string& topology_file, 
                      const std::string& multicast_type );

/// Os roteadores trocam mensagens até montarem suas tabelas de roteamento.
void find_routes ();

/// Roda o prompt para o usuário solicitar rotas.
void run_prompt (const std::string& progname);

} // namespace ep4

#endif

