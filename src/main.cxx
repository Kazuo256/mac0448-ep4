
#include "ep4.h"

#include <cstdlib>
#include <iostream>

using std::cout;

int main (int argc, char** argv) { 
  // Verifica argumentos
  if (argc < 3) {
    cout << "Uso:\n\t" << argv[0] << " <arquivo_de_topologia> <algoritmo>\n";
    return EXIT_FAILURE;
  }
  // Executa lógica do programa
  ep4::init_simulation(argv[1], argv[2]);
  ep4::find_routes();
  ep4::run_prompt(argv[0]);
  ep4::clean_up();
  // Execução bem sucedida
  return EXIT_SUCCESS;
}

