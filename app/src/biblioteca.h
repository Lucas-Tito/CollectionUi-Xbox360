// Leitura da biblioteca do FreeStyle: o banco content.db e os caminhos da arte.
#ifndef BIBLIOTECA_H
#define BIBLIOTECA_H

#include <string>
#include <vector>

namespace biblioteca
{
    struct Jogo
    {
        int          id;            // ContentItemId; vira o nome da pasta de arte, em hex
        unsigned int titleId;
        std::string  nome;
        std::string  genero;
        std::string  desenvolvedora;
        std::string  publicadora;
        std::string  nota;          // do Marketplace, ex. "4.25"
        std::string  lancamento;
        std::string  caminho;       // relativo ao dispositivo
        int          tipoArquivo;   // 1 = XEX solto, 3 = container STFS/GOD
        int          discos;
    };

    // Procura a instalacao do FreeStyle nos dispositivos montados. Nao adivinha o nome da
    // pasta: enumera a raiz de cada dispositivo e testa <pasta>\Data\Databases\content.db.
    // A instalacao real encontrada no console chamava "Freestyle.780".
    bool AcharBanco(std::string &caminhoSaida);

    // Le todos os jogos, em ordem alfabetica (ignorando artigo inicial, como no desenho).
    bool Ler(const char *caminhoBanco, std::vector<Jogo> &saida);

    // Pasta de arte de um jogo, a partir da pasta do banco: <raiz>\Data\GameData\<ID em hex>
    std::string PastaArte(const std::string &caminhoBanco, int id);
}

#endif
