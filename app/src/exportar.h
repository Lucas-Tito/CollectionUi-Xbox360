// Inventario do console em arquivo, para o xbox-vault.
//
// O vault conhece o catalogo de jogos do mundo, mas nao sabe o que ESTE console tem:
// nao sabe quais ROMs existem nem com que nome de arquivo, nao conhece jogo que o
// catalogo dele nao cobre, e nao tem como saber o id sintetico que uma ROM recebe
// aqui. Este arquivo entrega as tres coisas.
//
// NAO inclui as colecoes: o colecoes.txt esta do lado, o vault ja o le, e duplicar
// daria duas fontes da mesma verdade.
//
// Escrito por ITEM DE MENU, nunca no arranque: nem toda sessao usa o vault, e gravar
// 460 linhas a cada abertura seria peso a toa.
#ifndef EXPORTAR_H
#define EXPORTAR_H

#include "biblioteca.h"
#include <string>
#include <vector>

namespace exportar
{
    // Grava game:\biblioteca.txt. Devolve false e preenche "erro" com algo curto o
    // bastante para caber no balao do sistema.
    bool Biblioteca(const std::vector<biblioteca::Jogo> &jogos, std::string &erro);
}

#endif
