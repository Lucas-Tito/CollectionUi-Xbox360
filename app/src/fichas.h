// As fichas da faixa de detalhes: o que o xbox-vault sabe de cada jogo, por TitleId.
// Vêm de game:\fichas.txt, gerado no PC por ferramentas/fichas.py (formato lá). Sem o
// arquivo, nada quebra: a faixa mostra só o que a FreeStyle guarda no próprio Jogo.
#ifndef FICHAS_H
#define FICHAS_H

#include <string>

namespace fichas
{
    struct Ficha
    {
        int         coop;           // jogadores no mesmo console, do Co-Optimus; 0 = nada
        std::string coopExtras;     // "co-op campaign · split screen"
        std::string coopTexto;      // o texto do Co-Optimus
        int         metacritic;     // 0 = sem nota
        float       notaJogadores;  // 0 a 5; 0 = sem nota (só quando não há Metacritic)
        float       horas[3];       // história, com extras, 100%; 0 = sem dado
        int         ano;
        std::string genero, dev, pub, descricao;
    };

    // Lê o arquivo (uma vez, no arranque). Devolve quantas fichas leu.
    int Carregar(const char *caminho);

    // A ficha do TitleId, ou NULL.
    const Ficha *Por(unsigned int titleId);
}

#endif
