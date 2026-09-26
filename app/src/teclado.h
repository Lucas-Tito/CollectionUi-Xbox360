// Teclado do sistema, para digitar o nome de uma coleção.
//
// O XShowKeyboardUI NAO esta declarado em header nenhum do XDK -- so as flags VKBD_*
// e os ids de mensagem estao. E export do xam.xex, e quem quer usar declara. Mesmo
// caso do ObCreateSymbolicLink em dispositivos.cpp.
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
