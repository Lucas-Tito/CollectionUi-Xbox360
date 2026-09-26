// As coleções do usuário: nome e a lista de jogos que estão nela.
//
// Guardadas em game:\colecoes.txt, ao lado do .xex, uma coleção por linha:
//     Para jogar em dois|4156081C,4D530910,555308B6
//
// Os números são TitleId, em hexadecimal -- não ContentItemId.
//
// O ContentItemId é a chave primária do banco do FreeStyle, e o que a mantém é o
// CAMINHO (o schema tem UNIQUE (ContentItemPath)). Ele sobrevive a rescan e a reboot,
// mas morre se o jogo for reinstalado, se a pasta for movida ou renomeada, ou se o
// banco for refeito. O TitleId vem do cabeçalho do XEX e não depende de nada disso.
//
// O preço, aceito conscientemente: itens que compartilham TitleId se fundem. Neste
// acervo são 4 pares em 120 itens -- Forza e Splinter Cell (disco 1 e 2), CoD World
// at War e Kill Team (instalados em duplicata). Marcar um disco traz o outro junto, e
// remover tira os dois. Em acervo grande, dois jogos DIFERENTES com o mesmo TitleId
// se fundiriam de forma errada; não achamos nenhum caso aqui, mas a amostra é de 120.
//
// A CAPA continua sendo achada pelo ContentItemId (a pasta é GameData\<id em hex>):
// isto aqui é só o que fica gravado em disco.
#ifndef COLECOES_H
#define COLECOES_H

#include <string>
#include <vector>

namespace colecoes
{
    struct Colecao
    {
        std::string               nome;
        std::vector<unsigned int> ids;   // TitleId, ver o cabeçalho
    };

    // Corta em 28 bytes SEM partir uma sequência UTF-8 ao meio, e troca o '|' e as
    // quebras de linha por espaço -- o arquivo separa nome e ids por '|'.
    std::string Sanear(const std::string &nome);

    void Carregar();
    void Gravar();

    // Em ordem alfabética, ignorando o artigo inicial. É a ordem da tela.
    std::vector<Colecao *> Ordenadas();

    Colecao *Criar(const std::string &nome);
    void     Apagar(Colecao *c);

    bool Tem(const Colecao *c, unsigned int titleId);
    void Alternar(Colecao *c, unsigned int titleId);
    void Remover(Colecao *c, unsigned int titleId);
}

#endif
