// Teclado do sistema, para digitar o nome de uma coleção.
//
// O XShowKeyboardUI vem de include/xbox/xbox.h, que o xtl.h puxa. Bloqueia o laço
// principal enquanto a tela do sistema está aberta: ver a ressincronização de
// entrada em main.cpp, sem a qual o A que confirma o teclado vaza para o app.
#ifndef TECLADO_H
#define TECLADO_H

#include <string>

namespace teclado
{
    // Abre o teclado do sistema e BLOQUEIA ate o usuario confirmar ou cancelar.
    // Bloquear e aceitavel aqui: enquanto o teclado esta na tela, o nosso desenho
    // nao aparece de qualquer forma.
    // Devolve false se cancelou.
    bool Pedir(const char *titulo, const char *descricao,
               const char *textoInicial, std::string &saida);
}

#endif
