// Inventario do console em arquivo, para o xbox-vault.
//
// O vault conhece o catalogo de jogos do mundo, mas nao sabe o que ESTE console tem:
// nao sabe quais ROMs existem nem com que nome de arquivo, nao conhece jogo que o
// catalogo dele nao cobre, e nao tem como saber o id sintetico que uma ROM recebe
// aqui. Este arquivo entrega as tres coisas -- a terceira so porque o nome do
// ARQUIVO da ROM vai junto: e ele, com o TitleId do emulador, que semeia o id.
//
// Leva as COLECOES junto, de proposito, e o arquivo se chama vault.txt e nao
// biblioteca.txt por causa disso: e um so de importar, nao dois. A versao anterior
// deixava as colecoes de fora para nao haver duas fontes da mesma verdade, e o
// raciocinio estava errado -- o colecoes.txt e o arquivo VIVO, que o app le e
// reescreve; este aqui e um retrato morto, gerado sob demanda e nunca lido de volta.
// Retrato que duplica o original nao disputa a verdade com ele. O que havia de risco
// real era o oposto: dois arquivos para o vault importar, que podiam chegar de
// momentos diferentes e descrever consoles diferentes.
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
    // Grava game:\vault.txt: biblioteca, ROMs e colecoes no mesmo arquivo. Devolve
    // false e preenche "erro" com algo curto o bastante para caber no balao do
    // sistema.
    //
    // As colecoes nao vem por parametro: saem de colecoes::Ordenadas(), que le o
    // estado que o proprio modulo ja mantem carregado.
    //
    // "bancoOk" e o retorno do biblioteca::Ler. Ele nao da para deduzir da lista: um
    // Ler que falha devolve lista VAZIA, igual a um console sem jogo. Exportar nesse
    // estado entregaria ao vault um inventario vazio com cara de verdade.
    bool ParaOVault(const std::vector<biblioteca::Jogo> &jogos, bool bancoOk,
                    std::string &erro);
}

#endif
