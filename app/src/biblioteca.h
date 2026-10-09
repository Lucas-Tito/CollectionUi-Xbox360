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
        // sintético, junto do campo abaixo. Zero nos jogos que vêm do banco.
        unsigned int titleIdEmulador;
        // Só para ROM: o nome do arquivo dentro da pasta de ROMs ("Arcade\\smw.smc"),
        // que é a OUTRA metade da semente do id sintético. O "nome" acima não serve:
        // ele pode ser o rótulo do nomes.txt, que é escolhido por quem edita o arquivo
        // e não entra no cálculo. Sem este campo o id não se reproduz fora daqui.
        std::string  arquivoRom;

        // O titleId acima e SINTETICO, vindo do caminho em vez do cabecalho do xex.
        //
        // Xex sem EXECUTION_INFO chega do banco com TitleId zero, e zero nao e
        // identificador: e a AUSENCIA de um. Os seis homebrews deste console chegavam
        // todos como 00000000, o que os fundia num item so -- e nenhum entrava em
        // colecao, porque o colecoes::Carregar descarta o id zero. Ver IdDoApp.
        bool         idSintetico;

        Jogo() : id(0), titleId(0), idSintetico(false), tipoArquivo(0), contentType(0),
                 discos(0), titleIdEmulador(0) {}
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

    // FNV-1a de 32 bits sobre o CAMINHO de instalacao, em minusculas. Mesma ideia do
    // emuladores::IdDaRom e pelo mesmo motivo: o item nao tem TitleId e precisa de um
    // numero ESTAVEL entre execucoes, porque ele vai para o colecoes.txt.
    //
    // O caminho e a semente certa porque e a chave que o proprio FreeStyle usa -- o
    // schema tem UNIQUE (ContentItemPath) --, entao duas instalacoes nunca o
    // compartilham, e ele sobrevive a rescan e a reboot. Mover ou renomear a pasta
    // troca o id e tira o item das colecoes em que estava; e o mesmo preco que a ROM
    // ja paga ao ser renomeada, e pelo mesmo motivo.
    //
    // Colisao com um TitleId de verdade e possivel e tolerada: o efeito seria dois
    // itens sempre marcados juntos, que e o caso de jogo multi-disco -- ja previsto.
    unsigned int IdDoApp(const std::string &caminho);

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
