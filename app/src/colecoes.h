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
    // Uma coleção é de JOGOS, é JUNÇÃO de outras, ou é CRUZAMENTO de outras.
    //
    //     TIPO_JOGOS       você escolhe os jogos, um a um
    //     TIPO_UNIAO       tudo o que estiver em QUALQUER UMA das origens
    //     TIPO_INTERSECAO  só o que estiver em TODAS as origens
    //
    // Quem pode ser origem de quem segue uma escada:
    //
    //     JOGOS  <  UNIAO  <  INTERSECAO
    //
    // Junção só aceita coleção de jogos. Cruzamento aceita jogos OU junção, nunca
    // outro cruzamento. Sendo ordem ESTRITA, ciclo é impossível por construção -- não
    // existe detecção de ciclo em lugar nenhum, e não precisa existir.
    //
    // A escada é também o que torna útil o caso que motivou tudo isto: "co-op não
    // zerado" é o cruzamento de "não zerado" com a JUNÇÃO de co-op 2, 3 e 4 jogadores.
    // Sem o degrau do meio, cruzar os três co-op direto daria os jogos que são de 2 E
    // de 3 E de 4 ao mesmo tempo, que é outra pergunta e quase sempre vazia.
    enum Tipo
    {
        TIPO_JOGOS = 0,
        TIPO_UNIAO,
        TIPO_INTERSECAO
    };

    struct Colecao
    {
        int                       id;      // estável; é por ele que uma composta referencia
        Tipo                      tipo;
        std::string               nome;
        std::vector<unsigned int> ids;     // TitleId, se for de jogos
        std::vector<int>          origens; // ids de coleção, se for composta
    };

    // Composta = tem origens em vez de jogos próprios. Vale para junção e cruzamento,
    // e é o teste que quase todo chamador quer -- o `!uniao` de antes virou isto.
    bool Composta(const Colecao *c);

    // A escada, num lugar só. Chamar isto em vez de comparar tipos na mão é o que
    // impede que a regra se espalhe por três arquivos e se contradiga num deles.
    bool PodeSerOrigem(const Colecao *candidata, Tipo tipoDoAlvo);

    // A palavra que vai no arquivo, e a volta. O arquivo é o contrato com o
    // xbox-vault, então as duas moram aqui e não no exportar.
    const char *Palavra(Tipo t);
    bool        PorPalavra(const std::string &s, Tipo *saida);

    // Corta em 28 bytes SEM partir uma sequência UTF-8 ao meio, e troca o '|' e as
    // quebras de linha por espaço -- o arquivo separa nome e ids por '|'.
    std::string Sanear(const std::string &nome);

    void Carregar();
    void Gravar();

    // Em ordem alfabética, ignorando o artigo inicial. É a ordem da tela.
    std::vector<Colecao *> Ordenadas();

    Colecao *Criar(const std::string &nome);
    Colecao *CriarComposta(const std::string &nome, Tipo tipo);

    Colecao *PorId(int id);

    void     Apagar(Colecao *c);

    // Vale para os três tipos: numa composta, pergunta a cada origem. É por ela que a
    // composta sabe o que mostrar -- não há lista resolvida guardada em lugar nenhum.
    bool Tem(const Colecao *c, unsigned int titleId);

    // Estas duas mexem em ids próprios, então NÃO valem para composta: numa delas o
    // jogo está por causa de uma origem, e é de lá que se tira. Chamar com uma
    // composta não faz nada -- e quem chama já não deveria ter oferecido a opção.
    void Alternar(Colecao *c, unsigned int titleId);
    void Remover(Colecao *c, unsigned int titleId);
}

#endif
