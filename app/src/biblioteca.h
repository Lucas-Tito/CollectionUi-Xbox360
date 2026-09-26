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
        // 1 = XEX solto, 2 = XBE (Xbox original), 3 = container STFS/GOD.
        // O 2 existe de verdade: sao os \ORIGINAL XBOX\...\default.xbe, e lancam
        // pela MESMA chamada do 1.
        int          tipoArquivo;
        // XCONTENTTYPE_*: 0x7000 jogo de 360, 0xD0000 arcade/XBLA, 0x5000 Xbox
        // original, 0x2 marketplace (indie). Só importa para escolher default.xex ou
        // default.xbe dentro de um container -- NAO serve de filtro: a lista branca do
        // FSD 2 recusa o 0x2, e o FreeStyle 3 desta casa lanca esses jogos.
        int          contentType;
        int          discos;
    };

    struct Candidato
    {
        std::string caminho;    // ...\Data\Databases\content.db
        std::string rotulo;     // "Hdd:\FreeStyle", o que aparece na tela
    };

    // Enumera as instalacoes do FreeStyle nos dispositivos montados. Nao adivinha o
    // nome da pasta e NAO abre banco nenhum: tudo o que devolve sai da varredura de
    // diretorio, que ja estava paga. Quem escolhe e o usuario, uma vez so -- ver
    // config::LerBanco.
    void ListarCandidatos(std::vector<Candidato> &saida);

    bool Existe(const std::string &caminho);

    // Le todos os jogos, em ordem alfabetica (ignorando artigo inicial, como no desenho).
    bool Ler(const char *caminhoBanco, std::vector<Jogo> &saida);

    // Pasta de arte de um jogo, a partir da pasta do banco: <raiz>\Data\GameData\<ID em hex>
    std::string PastaArte(const std::string &caminhoBanco, int id);
}

#endif
