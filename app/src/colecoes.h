// As coleções do usuário: nome e a lista de jogos que estão nela.
//
// Guardadas em game:\colecoes.txt, ao lado do .xex, uma coleção por linha:
//     Para jogar em dois|12,40,117
//
// Os números são ContentItemId. Eles são estáveis enquanto o FreeStyle não reindexar
// do zero: o banco tem UNIQUE no caminho, então rescan de arquivos já conhecidos
// mantém a linha. Se um jogo for apagado e reinstalado, ganha id novo e sai da
// coleção -- limitação aceita para a v1.
#ifndef COLECOES_H
#define COLECOES_H

#include <string>
#include <vector>

namespace colecoes
{
    struct Colecao
    {
        std::string      nome;
        std::vector<int> ids;
    };

    void Carregar();
    void Gravar();

    // Em ordem alfabética, ignorando o artigo inicial. É a ordem da tela.
    std::vector<Colecao *> Ordenadas();

    Colecao *Criar(const std::string &nome);
    void     Apagar(Colecao *c);

    bool Tem(const Colecao *c, int id);
    void Alternar(Colecao *c, int id);
    void Remover(Colecao *c, int id);
}

#endif
