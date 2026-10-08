// Inventario do console em arquivo, para o xbox-vault.
//
// O vault conhece o catalogo de jogos do mundo, mas nao sabe o que ESTE console tem:
// nao sabe quais ROMs existem nem com que nome de arquivo, nao conhece jogo que o
// catalogo dele nao cobre, e nao tem como saber o id sintetico que uma ROM recebe
// aqui. Este arquivo entrega as tres coisas -- a terceira so porque o nome do
// ARQUIVO da ROM vai junto: e ele, com o TitleId do emulador, que semeia o id.
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
    //
    // "bancoOk" e o retorno do biblioteca::Ler. Ele nao da para deduzir da lista: um
    // Ler que falha devolve lista VAZIA, igual a um console sem jogo. Exportar nesse
    // estado entregaria ao vault um inventario vazio com cara de verdade.
    bool Biblioteca(const std::vector<biblioteca::Jogo> &jogos, bool bancoOk,
                    std::string &erro);
}

#endif
