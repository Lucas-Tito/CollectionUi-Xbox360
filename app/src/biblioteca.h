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
        // Arquivo que guarda a capa, caminho completo. Para jogo da FreeStyle e o
        // container .assets; para ROM de emulador e um .jpg solto. Quem le decide
        // pelo conteudo, nao pela extensao. Vazio = sem capa, nem tenta.
        std::string  capa;
        // Se a capa vai inteira para a tela ou so a frente dela NAO mora aqui: mora
        // no cache de texturas, junto da imagem. O mesmo jogo rende encarte ou capa
        // pequena conforme o que existir dentro do .assets, e so quem LEU o arquivo
        // sabe qual veio.

        // Só para ROM de emulador: o TitleId de quem a abre. É ele que semeia o id
        // sintético, então sem ele o id não se reproduz fora daqui. Zero nos jogos
        // que vêm do banco.
        unsigned int titleIdEmulador;

        Jogo() : id(0), titleId(0), tipoArquivo(0), contentType(0), discos(0),
                 titleIdEmulador(0) {}
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

    // Ordem alfabetica, ignorando o artigo inicial. O Ler ja a aplica; existe solta
    // porque as ROMs de emulador entram depois e a lista tem de voltar a ficar em
    // ordem antes de a tela usa-la.
    void Ordenar(std::vector<Jogo> &lista);

    // Intercala "extras" em "destino", mantendo a ordem. Ordena "extras" no caminho.
    void Juntar(std::vector<Jogo> &destino, std::vector<Jogo> &extras);
}

#endif
