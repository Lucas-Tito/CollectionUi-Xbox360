// ROMs de emulador como itens da biblioteca.
//
// O content.db da FreeStyle nao guarda ROM: ele indexa so o que o 360 lanca -- XEX,
// XBE e container. O emulador em si E um item (o Snes360 e o titleId FFED0707), e as
// ROMs sao arquivo solto dentro da pasta dele. Entao quem varre somos nos.
//
// Abrir uma ROM abre o EMULADOR, no menu dele. Nenhum dos emuladores desta casa le
// launch data nem tem autoload -- isso foi lido no fonte dos tres, nao suposto. Ver
// a decisao 113.
#ifndef EMULADORES_H
#define EMULADORES_H

#include "biblioteca.h"
#include <string>
#include <vector>

namespace emuladores
{
    // Para cada emulador que estiver em "jogos", varre a pasta de ROMs dele e
    // acrescenta um item por ROM. Os itens saem com o caminho e o tipo do PROPRIO
    // emulador, entao lancar uma ROM usa o mesmo codigo de lancar qualquer jogo.
    //
    // "pastaDoApp" e onde fica a pasta capas\, de onde sai a arte das ROMs.
    void Ler(const std::vector<biblioteca::Jogo> &jogos,
             const std::string &pastaDoApp,
             std::vector<biblioteca::Jogo> &saida);

    // Identidade de uma ROM, ocupando o campo do TitleId -- e ela que vai para o
    // colecoes.txt. Todas as ROMs de um emulador compartilham o titleId dele, entao
    // sem isto marcar uma marcaria todas. Ver a decisao 109.
    unsigned int IdDaRom(unsigned int titleIdEmulador, const std::string &arquivo);
}

#endif
