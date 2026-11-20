// CollectionUI — coleções de jogos do Xbox 360.
//
// Três telas, como em docs/interface.md: coleções, os jogos de uma coleção, e a de
// adicionar jogos. A regra dos botões é a mesma em todas: X acrescenta, ☰ abre opções
// do que está em foco, A confirma, B volta ou cancela.
//
// Desenha com a ATG, o conjunto de amostra do próprio XDK: dela vêm o renderizador de
// fonte (com corte por LARGURA, não por contagem de caractere) e os glifos dos botões.

#include <xtl.h>
#include <xgraphics.h>
#include <algorithm>   // sort/unique/binary_search do g_cobertos
#include "diario.h"
#include "biblioteca.h"
#include "emuladores.h"
#include "exportar.h"
#include "colecoes.h"
#include "dispositivos.h"
#include "config.h"
#include "carregador.h"
#include "teclado.h"
#include "fsda.h"
#include "lancador.h"
#include "som.h"
#include "roteiro.h"
#include "fichas.h"
#include "AtgDevice.h"
#include "AtgFont.h"
#include "AtgDebugDraw.h"
#include "AtgSimpleShaders.h"

// XNotifyQueueUI nao esta em header nenhum do XDK, mas esta exportada em xav.lib --
// e o balao de notificacao do sistema, o mesmo que a Guide usa. O hiddriver a usa
// assim, e e bem melhor que uma caixa desenhada por nos no meio da tela.
extern "C" VOID __stdcall XNotifyQueueUI(DWORD tipo, DWORD usuario, ULONGLONG prioridade,
                                         PWCHAR texto, PVOID contexto);
#define XNOTIFY_GENERIC         3    // icone de carta
#define XNOTIFY_PRIORIDADE_ALTA 2

// A ATG espera este ponteiro global; normalmente quem o define é o AtgApp.cpp, que não
// usamos. Atenção ao tipo: dentro do namespace, D3DDevice é o ATG::D3DDevice do
// AtgDevice.h, que herda do global. Definir com o tipo global compila e falha no link.
namespace ATG { D3DDevice *g_pd3dDevice = NULL; }

namespace
{
    // ---- geometria, em pixels de 1280x720 --------------------------------------
    const int COLUNAS    = 5;
    const int LINHAS     = 2;          // visíveis por vez; o resto rola
    // Capa menor e mais folga: com 170x238 e 56 de espaço as capas ficavam coladas,
    // sem ar entre uma e outra. A altura segue a proporção da FRENTE do encarte, que é
    // 421x600 depois do recorte em U -- 1,425, não 1,4.
    const int CAPA_L     = 146;
    const int ESPACO_X   = 72;
    const int ESPACO_Y   = 56;
    const int MARGEM_X   = 100;        // margem do texto (cabeçalho e rodapé)
    const int CAPA_A     = 208;        // 146 * 1,425, a proporção da frente
    const int TOPO       = 92;

    // A grade é centralizada por cálculo, como a de coleções: 5*150 + 4*70 = 1070,
    // sobra 210 dividida nos dois lados.
    const int GRID_MARGEM = (1280 - (COLUNAS * CAPA_L + (COLUNAS - 1) * ESPACO_X)) / 2;
    const int POR_PAGINA = COLUNAS * LINHAS;

    // Quatro por linha em vez de cinco: o quadrado passa de 180 para 244, e sobrava
    // espaço de qualquer jeito -- com 5 colunas a grade terminava em x=1120 e as duas
    // linhas em y=454, numa tela que vai até 720.
    const int COL_LADO = 244, COL_GAP = 32;
    const int COL_POR_LINHA = 4, COL_LINHAS = 2;
    // Centralizada: 4*244 + 3*32 = 1072, sobra 208 dividida nos dois lados.
    const int COL_MARGEM = (1280 - (COL_POR_LINHA * COL_LADO + (COL_POR_LINHA - 1) * COL_GAP)) / 2;
    const int COL_POR_PAGINA = COL_POR_LINHA * COL_LINHAS;

    // O asset tipo 128 é o ENCARTE inteiro (contracapa + lombada + frente). A frente
    // são os 46,8% da direita — fração calibrada contra a biblioteca real. Em vez de
    // recortar a imagem, amostramos só essa parte ao desenhar.
    const float FRENTE_U0 = 1.0f - 0.468f;

    // As cores de antes do Metro que ficaram: as do nome no quadrado de coleção (o de
    // hoje, pedido de 17/11) e o verde do emblema. As do Metro são as M_*, mais abaixo.
    const D3DCOLOR COR_TEXTO   = D3DCOLOR_XRGB(231, 237, 233);
    const D3DCOLOR COR_APAGADO = D3DCOLOR_XRGB(142, 156, 148);
    const D3DCOLOR COR_FRACO   = D3DCOLOR_XRGB(92, 104, 98);
    const D3DCOLOR COR_ANEL    = D3DCOLOR_XRGB(155, 203, 60);   // o verde do ring of light

    // Analógico: fora desta zona o eixo conta como direção. O repique imita tecla
    // segurada, para segurar a alavanca percorrer a lista sem virar corrida.
    const short ZONA_MORTA     = 14000;
    const DWORD ESPERA_INICIAL = 380;
    const DWORD ESPERA_REPETE  = 110;

    // ---- cache de capas --------------------------------------------------------
    // A chave é o ContentItemId, NÃO a posição na lista. Com coleções, a posição 3 é
    // um jogo diferente em cada uma: chavear por posição mostraria a capa errada ao
    // trocar de coleção. Pelo id, as três telas compartilham o mesmo cache e trocar
    // de tela não perde nem recarrega nada.
    const int CACHE_MAX = 160;   // 36 dos cards (12 na janela x 3) + 120 do adicionar

    struct Entrada
    {
        int         jogoId;
        D3DTexture *textura;
        DWORD       uso;      // relógio lógico, para descartar o usado há mais tempo
        // Viaja junto da TEXTURA, não do Jogo: o mesmo jogo rende encarte (só a frente
        // vai para a tela) ou capa pequena (vai inteira) conforme o que existir dentro
        // do .assets. Guardado no Jogo, as duas decisões podiam discordar.
        bool        inteira;
    };

    Entrada g_cache[CACHE_MAX];
    DWORD   g_relogio = 0;
    std::vector<int> g_emVoo;
    std::vector<int> g_falhou;
    const int EM_VOO_MAX = 6;

    ATG::Font g_fonte;

    // ---- estado ----------------------------------------------------------------
    // TELA_ORIGENS reusa o desenho da grade de coleções, marcando quais entram na
    // união. Mesma mecânica da tela de adicionar jogos: A marca, ☰ conclui, B cancela.
    // TELA_SOLTOS e a grade de capas dos jogos que nao estao em colecao nenhuma --
    // uma TELA, mas alcancada por uma ABA, a primeira da barra. TELA_ARQUIVAR e a
    // escolha de colecoes para um jogo, com o vocabulario da tela de origens.
    enum Tela { TELA_COLECOES, TELA_JOGOS, TELA_ADICIONAR, TELA_ORIGENS,
                TELA_SOLTOS, TELA_ARQUIVAR };

    std::string                   g_caminhoBanco;
    std::vector<biblioteca::Jogo> g_jogos;
    // Se o banco FOI LIDO. Lista vazia nao distingue console sem jogo de leitura que
    // falhou, e o export precisa dessa diferenca -- ver exportar.h.
    bool g_bancoOk = true;

    Tela g_tela = TELA_COLECOES;
    colecoes::Colecao *g_atual = NULL;
    std::vector<unsigned int> g_selecao;   // rascunho ao adicionar; TitleIds
    int g_iCol = 0, g_iJogo = 0, g_primeiraLinha = 0, g_primeiraLinhaCol = 0;

    // ---- a aba de categoria ----------------------------------------------------
    //
    // A aba NAO muda o que a colecao tem: ela encolhe o UNIVERSO sobre o qual se
    // pergunta. Por isso o colecoes.cpp nao sabe que abas existem -- o filtro entra
    // so onde se VARRE g_jogos, que sao QUATRO lugares: ListaAtual, TresDaColecao, a
    // contagem do card e o filtro do ListaDeColecoes.
    //
    // O rodape da tela de origens varre g_jogos e NAO filtra, de proposito: ele conta
    // o que a composta vai ter, que nao depende de aba nenhuma.
    //
    // O APP SEMPRE ABRE EM "TODOS". Esta variavel mora na memoria e em lugar nenhum
    // alem dela: nao vai para o colecoes.txt, nao vai para a config, nao sobrevive a
    // sair do app. Voltar e encontrar metade do acervo sumido, sem lembrar de ter
    // apertado RB na sessao passada, parece defeito -- e o jeito de nunca pagar esse
    // preco e nao ter onde guardar.
    int g_aba = biblioteca::ABA_AUTOMATICA;   // 0 = Todos

    // A primeira da barra, a ESQUERDA do Todos, e nao e categoria da FreeStyle: sao
    // os jogos que nao estao em colecao nenhuma. Negativa de proposito -- as da
    // FreeStyle sao 0..6 e esta nao concorre com numero nenhum deles.
    const int ABA_SOLTOS = -1;

    bool g_temSoltos = false;        // ha o que arquivar? decide se a aba existe

    // TitleIds citados por alguma colecao DE JOGOS, ordenado, refeito uma vez por
    // quadro. Sem ele, perguntar "este jogo esta em alguma colecao?" custaria uma
    // varredura dos ids de cada colecao por item -- colecoes x jogos x ids a cada
    // quadro. Com ele e uma busca binaria.
    std::vector<unsigned int> g_cobertos;

    // O jogo sendo arquivado e o rascunho das colecoes marcadas. Rascunho pelo mesmo
    // motivo do escolhedor de jogos: sem ele o B nao teria o que desfazer.
    const biblioteca::Jogo *g_arquivando = NULL;
    std::vector<int>        g_escolhidas;

    // De qual menu se trata. Despachar pelo g_tela dava errado: ☰ na tela de jogos
    // abria o menu do jogo, mas EscolherNoMenu lia g_tela de novo -- e se a ação
    // anterior do mesmo quadro tivesse mudado de tela, executava o item do outro menu.
    enum MenuDe { MENU_COLECAO, MENU_JOGO, MENU_APAGAR, MENU_TIPO, MENU_EXPORTAR };

    bool   g_menuAberto = false;
    MenuDe g_menuDe = MENU_COLECAO;
    int    g_menuFoco = 0, g_menuQtd = 0;
    const char *g_menuItens[4];

    // O cabecalho do modal tem DUAS linhas: um rotulo pequeno dizendo de que se trata
    // e, embaixo, o nome. Sem o rotulo, "Co-op de sofa" sozinho no alto de uma caixa
    // nao diz se aquilo vai renomear, apagar ou exportar.
    const char *g_menuRotulo = "";

    // Qual linha e a destrutiva, ou -1. Ela NAO tem cor propria parada: fica igual as
    // outras ate receber o foco. Cor parada em item de menu vira enfeite; o que o
    // vermelho precisa marcar e o instante em que o A faria aquilo.
    int    g_menuPerigo = -1;

    // O menu de coleção tem layout VARIÁVEL -- composta ganha uma linha a mais.
    // Deduzir a ação do índice já quase deu errado uma vez; aqui cada linha diz o que
    // faz. Os outros menus têm layout fixo e continuam decidindo pelo índice.
    enum AcaoCol { ACAO_RENOMEAR, ACAO_ORIGENS, ACAO_APAGAR };
    AcaoCol g_menuAcao[4];
    char   g_menuTitulo[128] = "";   // CÓPIA: o c_str() de uma coleção apagada morre
    char   g_menuItemBuf[64] = "";   // item de menu com texto montado na hora

    // O teclado do sistema é assíncrono (ver teclado.h -- bloquear TRAVA o console),
    // então o que fazer com o texto tem de sobreviver a vários quadros.
    enum Pedido { PEDIDO_NENHUM, PEDIDO_NOVA, PEDIDO_RENOMEAR };
    Pedido g_pedido = PEDIDO_NENHUM;
    colecoes::Colecao *g_renomeando = NULL;

    // Entre o teclado e a escolha do tipo, o nome fica aqui.
    char g_nomeNovo[64] = "";

    // Rascunho da composta em edição, e a composta sendo editada. Mesma ideia do
    // g_selecao dos jogos: só vira arquivo quando se conclui.
    std::vector<int>    g_origens;
    colecoes::Colecao  *g_editandoComposta = NULL;


    bool  g_anelMarcado = true;      // ver config::Ligado("anel")


    // ---- texto -----------------------------------------------------------------
    // O SQLite devolve UTF-8. Converter com CP_ACP quebra o que não for ASCII: o
    // "BLAZBLUE　CONTINUUM SHIFT" tem um espaço ideográfico japonês (três bytes)
    // que virava lixo. E acima de 0x100 ficam os GLIFOS DE BOTÃO da fonte, então um
    // caractere japonês que passasse desenharia um botão no meio do nome.
    // O texto que chega aqui vem de DUAS codificações diferentes.
    //
    // O SQLite e o colecoes.txt devolvem UTF-8 de verdade. Mas um literal estreito do
    // próprio fonte NÃO é UTF-8: com o BOM no arquivo, o cl.exe converte "..." para a
    // codificação de execução, e "coleção" vira os bytes e7 e3 6f -- conferido no .obj.
    // Com CP_UTF8 o e7 é começo de sequência inválida e a conversão PARA ALI: era por
    // isso que o menu mostrava "Remover da cole".
    //
    // MB_ERR_INVALID_CHARS não existe nos headers do Xbox, então a validação é nossa.
    bool EhUtf8(const char *s)
    {
        const unsigned char *p = (const unsigned char *)s;
        while (*p)
        {
            int extras;
            if (*p < 0x80)                       extras = 0;
            else if ((*p & 0xE0) == 0xC0)        extras = 1;
            else if ((*p & 0xF0) == 0xE0)        extras = 2;
            else if ((*p & 0xF8) == 0xF0)        extras = 3;
            else return false;                   // continuação solta ou byte inválido

            p++;
            while (extras-- > 0)
                if ((*p++ & 0xC0) != 0x80) return false;
        }
        return true;
    }

    // Latin-1 puro: cada byte vira o ponto de código de mesmo valor. Cobre exatamente
    // os acentos dos nossos literais (e7 = ç, e3 = ã) sem depender de nenhuma página
    // de código estar presente no console.
    void LarguraLatin1(const char *origem, WCHAR *destino, int capacidade)
    {
        int i = 0;
        for (; origem[i] != '\0' && i < capacidade - 1; i++)
            destino[i] = (WCHAR)(unsigned char)origem[i];
        destino[i] = L'\0';
    }

    void Larga(const std::string &origem, WCHAR *destino, int capacidade)
    {
        if (!EhUtf8(origem.c_str()))
            LarguraLatin1(origem.c_str(), destino, capacidade);
        else if (MultiByteToWideChar(CP_UTF8, 0, origem.c_str(), -1,
                                     destino, capacidade) <= 0)
        {
            destino[0] = L'\0';
            return;
        }
        for (int i = 0; destino[i] != L'\0'; i++)
        {
            if (destino[i] == 0x3000)        destino[i] = L' ';
            else if (destino[i] >= 0x100)    destino[i] = L'?';
        }
    }

    const char *SemArtigo(const char *s)
    {
        static const char *ARTIGOS[] = { "the ", "a ", "an ", "o ", "os ", "as ", "um ", "uma " };
        for (int i = 0; i < 8; i++)
        {
            size_t n = strlen(ARTIGOS[i]);
            if (_strnicmp(s, ARTIGOS[i], (int)n) == 0)
                return s + n;
        }
        return s;
    }

    char Inicial(const std::string &nome)
    {
        char c = (char)toupper((unsigned char)SemArtigo(nome.c_str())[0]);
        return (c >= 'A' && c <= 'Z') ? c : '#';
    }

    // ---- listas ----------------------------------------------------------------
    // Ponteiros para g_jogos, que não muda depois de carregada.
    // O rotulo na tela e NOSSO, em portugues. O TabName do settings.db diz "Xbox
    // Classic" e "Emulator", e o resto do app fala portugues -- o que vem da
    // FreeStyle e o NUMERO, que e o que precisa concordar com ela.
    const WCHAR *NomeDaAba(int aba)
    {
        switch (aba)
        {
        case biblioteca::ABA_XBLA:     return L"XBLA";
        case biblioteca::ABA_X360:     return L"Xbox 360";
        case biblioteca::ABA_CLASSICO: return L"Xbox OG";
        case biblioteca::ABA_HOMEBREW: return L"Homebrew";
        case biblioteca::ABA_EMULADOR: return L"Emulators";
        case biblioteca::ABA_OUTROS:   return L"Other";
        case ABA_SOLTOS:               return L"Unsorted";
        }
        return L"All";
    }

    // ONDE A ABA VALE, num lugar so. Escolhendo origens ela NAO vale: ali se escolhe
    // coleção, e a composta que sair dali vale para as outras abas também.
    //
    // Isto precisa morar aqui e não em cada chamador porque o TelaColecoes desenha as
    // DUAS telas. Sem esta pergunta, o card de uma origem saía com a colagem vazia e
    // `( 0 )` -- que é o sinal de "coleção vazia" em todo o resto do app -- enquanto o
    // rodapé da mesma tela dizia "1 escolhidas · 37 jogos". Quem decidisse ali
    // decidiria pelo número errado, e composta sem origem é apagada sem volta.
    int AbaEmVigor()
    {
        // Arquivar um jogo e escolher COLECAO, igual a escolher origem -- e a aba
        // corrente ali e justamente "Unsorted", que nao e categoria de jogo nenhum.
        // Sem esta linha, toda colecao sairia com a colagem vazia e `( 0 )`.
        if (g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR) return biblioteca::ABA_AUTOMATICA;
        // Na tela dos soltos a pergunta e sobre a biblioteca inteira, nao sobre uma
        // categoria: o universo ali e "tudo o que nao esta em colecao".
        if (g_aba == ABA_SOLTOS) return biblioteca::ABA_AUTOMATICA;
        return g_aba;
    }

    bool NaAba(const biblioteca::Jogo &j)
    {
        const int aba = AbaEmVigor();
        return aba == biblioteca::ABA_AUTOMATICA || j.aba == aba;
    }

    // "Em colecao" e estar numa colecao DE JOGOS. Composta nao conta: o conteudo
    // dela vem das origens, entao um jogo que so aparece numa juncao ja esta na
    // colecao de jogos que e origem dela -- contar a juncao tambem nao mudaria
    // resposta nenhuma, e contar SO ela daria um "solto" que nao esta solto.
    void AtualizarCobertos()
    {
        g_cobertos.clear();
        std::vector<colecoes::Colecao *> todas = colecoes::Ordenadas();
        for (size_t i = 0; i < todas.size(); i++)
            if (!colecoes::Composta(todas[i]))
                for (size_t j = 0; j < todas[i]->ids.size(); j++)
                    g_cobertos.push_back(todas[i]->ids[j]);

        std::sort(g_cobertos.begin(), g_cobertos.end());
        g_cobertos.erase(std::unique(g_cobertos.begin(), g_cobertos.end()), g_cobertos.end());
    }

    bool EmAlgumaColecao(unsigned int titleId)
    {
        return std::binary_search(g_cobertos.begin(), g_cobertos.end(), titleId);
    }

    // As abas com conteudo, em ordem, com o Todos na frente. Recalculada UMA VEZ por
    // quadro, no ClampFoco: ela custa uma varredura da biblioteca por colecao, e
    // chamar a cada uso sairia tres vezes mais cara pelo mesmo resultado.
    std::vector<int> g_abas;

    // A REGRA INTEIRA, num lugar so: uma aba existe quando ALGUMA colecao tem jogo
    // nela.
    //
    // Nao e "o console tem jogo de 360": e "alguma colecao tem". A diferenca aparece
    // num acervo com 215 jogos de 360 e nenhum deles marcado -- a aba existiria e
    // levaria a uma tela vazia. Do jeito daqui, aba que existe tem conteudo, e por
    // construcao NAO DA para parar numa tela vazia.
    //
    // E no limite: com UMA categoria so, "All" e ela sao a mesma lista, e a barra
    // viraria dois botoes que fazem a mesma coisa. Ai nao ha barra nenhuma, e um
    // console so de Xbox 360 fica com a tela de sempre, sem saber que isto existe.
    void AtualizarAbas()
    {
        bool tem[biblioteca::ABA_QUANTAS];
        for (int a = 0; a < biblioteca::ABA_QUANTAS; a++) tem[a] = false;

        // UMA passada por colecao, contando as abas de quem casou -- e nao uma
        // passada por aba. O resultado e o mesmo e custa seis vezes menos.
        std::vector<colecoes::Colecao *> todas = colecoes::Ordenadas();
        // O teste do `tem` vem ANTES do Tem(): assim que uma categoria aparece, os
        // outros itens dela custam um bool em vez de uma varredura dos ids da coleção
        // (e, em composta, de uma recursão com PorId linear dentro). Isto roda todo
        // quadro, e sem o curto-circuito eram colecoes x jogos chamadas caras.
        for (size_t i = 0; i < todas.size(); i++)
            for (size_t j = 0; j < g_jogos.size(); j++)
                if (!tem[g_jogos[j].aba] && colecoes::Tem(todas[i], g_jogos[j].titleId))
                    tem[g_jogos[j].aba] = true;

        g_abas.clear();
        for (int a = biblioteca::ABA_XBLA; a < biblioteca::ABA_QUANTAS; a++)
            if (tem[a]) g_abas.push_back(a);

        if (g_abas.size() < 2) g_abas.clear();
        g_abas.insert(g_abas.begin(), biblioteca::ABA_AUTOMATICA);

        // O Soltos existe enquanto houver o que arquivar, e some sozinho quando o
        // ultimo for arquivado. Mesma forma da regra das categorias: aba que existe
        // tem conteudo, entao nenhuma tecla leva a uma tela vazia.
        //
        // Entra DEPOIS do insert do Todos, porque fica a esquerda dele -- e vale
        // mesmo sem categoria nenhuma: "Unsorted" e "All" ja sao escolha de verdade.
        AtualizarCobertos();
        g_temSoltos = false;
        for (size_t j = 0; j < g_jogos.size() && !g_temSoltos; j++)
            if (!EmAlgumaColecao(g_jogos[j].titleId)) g_temSoltos = true;

        if (g_temSoltos) g_abas.insert(g_abas.begin(), ABA_SOLTOS);
    }

    int IndiceDaAba()
    {
        for (size_t i = 0; i < g_abas.size(); i++)
            if (g_abas[i] == g_aba) return (int)i;
        return 0;
    }

    std::vector<const biblioteca::Jogo *> ListaAtual()
    {
        std::vector<const biblioteca::Jogo *> saida;

        if (g_tela == TELA_SOLTOS)
        {
            // A aba nao filtra aqui: o universo JA e um recorte, e cruzar os dois
            // esconderia metade do que se veio arrumar.
            for (size_t i = 0; i < g_jogos.size(); i++)
                if (!EmAlgumaColecao(g_jogos[i].titleId))
                    saida.push_back(&g_jogos[i]);
        }
        else if (g_tela == TELA_ADICIONAR)
        {
            // Ao ADICIONAR a aba nao filtra: marcar so o que a aba corrente mostra e
            // marcar as cegas -- o jogo entraria na coleção e sumiria da tela ao
            // voltar para Todos. O escolhedor mostra a biblioteca inteira.
            for (size_t i = 0; i < g_jogos.size(); i++)
                saida.push_back(&g_jogos[i]);
        }
        else if (g_atual != NULL)
        {
            for (size_t i = 0; i < g_jogos.size(); i++)
                // Casa por TitleId: é o que fica gravado. Itens que compartilham
                // TitleId (multi-disco, instalação duplicada) entram juntos.
                if (NaAba(g_jogos[i]) && colecoes::Tem(g_atual, g_jogos[i].titleId))
                    saida.push_back(&g_jogos[i]);
        }
        return saida;      // g_jogos já vem ordenada de biblioteca::Ler
    }

    // Quantos ITENS da biblioteca casam com os TitleIds marcados. Não é o tamanho do
    // rascunho: um TitleId de multi-disco casa com dois itens, e a grade mostra os
    // dois. Mesmo motivo da contagem na tela de coleções.
    int ItensMarcados()
    {
        int quantos = 0;
        for (size_t j = 0; j < g_jogos.size(); j++)
            for (size_t i = 0; i < g_selecao.size(); i++)
                if (g_selecao[i] == g_jogos[j].titleId) { quantos++; break; }
        return quantos;
    }

    // Quantos itens sairiam da coleção ao remover este TitleId.
    int ItensComTitleId(unsigned int titleId)
    {
        int quantos = 0;
        for (size_t j = 0; j < g_jogos.size(); j++)
            if (g_jogos[j].titleId == titleId) quantos++;
        return quantos;
    }

    bool SelecionadoNoRascunho(unsigned int titleId)
    {
        for (size_t i = 0; i < g_selecao.size(); i++)
            if (g_selecao[i] == titleId)
                return true;
        return false;
    }

    // ---- cache -----------------------------------------------------------------
    // "inteira" e opcional: so quem DESENHA precisa dele. Quem so pergunta "ja esta
    // carregada?" passa NULL.
    D3DTexture *NoCache(int jogoId, bool marcarUso, bool *inteira = NULL)
    {
        for (int i = 0; i < CACHE_MAX; i++)
        {
            if (g_cache[i].textura != NULL && g_cache[i].jogoId == jogoId)
            {
                if (marcarUso)
                    g_cache[i].uso = ++g_relogio;
                if (inteira != NULL)
                    *inteira = g_cache[i].inteira;
                return g_cache[i].textura;
            }
        }
        return NULL;
    }

    bool EstaNaLista(const std::vector<int> &v, int x)
    {
        for (size_t i = 0; i < v.size(); i++)
            if (v[i] == x)
                return true;
        return false;
    }

    void Guardar(int jogoId, D3DTexture *textura, bool inteira)
    {
        int alvo = -1;
        DWORD maisAntigo = 0xFFFFFFFF;

        for (int i = 0; i < CACHE_MAX; i++)
        {
            if (g_cache[i].textura == NULL) { alvo = i; break; }
            if (g_cache[i].uso < maisAntigo) { maisAntigo = g_cache[i].uso; alvo = i; }
        }

        if (alvo < 0)
        {
            textura->Release();
            return;
        }

        if (g_cache[alvo].textura != NULL)
        {
            // Desassociar do device não basta: o Present do quadro anterior só
            // ENFILEIRA o desenho, e a GPU ainda pode estar lendo esta textura.
            // Descarte só acontece com o cache cheio, então dá para pagar a barreira.
            ATG::g_pd3dDevice->SetTexture(0, NULL);
            ATG::g_pd3dDevice->BlockUntilIdle();
            g_cache[alvo].textura->Release();
            g_cache[alvo].textura = NULL;
        }

        g_cache[alvo].jogoId  = jogoId;
        g_cache[alvo].textura = textura;
        g_cache[alvo].uso     = ++g_relogio;
        g_cache[alvo].inteira = inteira;
    }

    // As tres primeiras da colecao NA ORDEM DA TELA. g_jogos ja vem alfabetica de
    // biblioteca::Ler, entao basta varrer na ordem e parar na terceira. Previsivel: o
    // card mostra o comeco do que se ve ao entrar.
    void TresDaColecao(const colecoes::Colecao *c, const biblioteca::Jogo *saida[3], int *quantas)
    {
        *quantas = 0;
        if (c == NULL) return;

        // Na aba, a colagem mostra o que a colecao mostra. Sem isto o card de uma
        // coleção filtrada estamparia capas que a grade de dentro nao tem.
        for (size_t i = 0; i < g_jogos.size() && *quantas < 3; i++)
            if (NaAba(g_jogos[i]) && colecoes::Tem(c, g_jogos[i].titleId))
                saida[(*quantas)++] = &g_jogos[i];
    }

    // O que a grade de coleções mostra. Escolhendo origens, entra só quem a escada
    // permite -- junção aceita jogos, cruzamento aceita jogos e junção -- e nunca a
    // própria composta em edição. É o que impede ciclo sem precisar detectar.
    std::vector<colecoes::Colecao *> ListaDeColecoes()
    {
        std::vector<colecoes::Colecao *> todas = colecoes::Ordenadas();

        // Arquivando: so colecao DE JOGOS recebe. Composta nao guarda jogo proprio --
        // oferecer uma seria oferecer um destino que nao recebe nada.
        if (g_tela == TELA_ARQUIVAR)
        {
            std::vector<colecoes::Colecao *> recebem;
            for (size_t i = 0; i < todas.size(); i++)
                if (!colecoes::Composta(todas[i])) recebem.push_back(todas[i]);
            return recebem;
        }

        if (g_tela != TELA_ORIGENS)
        {
            // A OUTRA metade da regra: numa aba, coleção que não tem nada dela sai da
            // tela. Junto com o AtualizarAbas, as duas garantem que nenhuma tecla
            // leva a uma tela vazia.
            // Pelo AbaEmVigor e não pelo g_aba: na aba Soltos a grade de coleções
            // nem está à vista, e comparar a categoria de cada jogo com -1 era uma
            // varredura inteira da biblioteca para devolver lista vazia.
            const int aba = AbaEmVigor();
            if (aba == biblioteca::ABA_AUTOMATICA)
                return todas;

            std::vector<colecoes::Colecao *> naAba;
            for (size_t i = 0; i < todas.size(); i++)
            {
                for (size_t j = 0; j < g_jogos.size(); j++)
                    if (g_jogos[j].aba == aba &&
                        colecoes::Tem(todas[i], g_jogos[j].titleId))
                    {
                        naAba.push_back(todas[i]);
                        break;
                    }
            }
            return naAba;
        }

        // Daqui para baixo e a tela de ORIGENS, onde a aba NAO vale. Ali se escolhe
        // coleção, e sumir com uma porque a aba corrente a esvazia esconderia uma
        // origem legítima -- a composta resultante vale para as outras abas também.

        // Na tela de origens SEM composta em edicao o seguro e a lista VAZIA, nao a
        // completa: a grade cheia ali deixaria marcar origem para ninguem. Estado
        // inalcancavel hoje; o ponto e que, se virar alcancavel, erre para o lado que
        // nao estraga nada.
        if (g_editandoComposta == NULL)
            return std::vector<colecoes::Colecao *>();

        std::vector<colecoes::Colecao *> saida;
        for (size_t i = 0; i < todas.size(); i++)
            if (todas[i] != g_editandoComposta &&
                colecoes::PodeSerOrigem(todas[i], g_editandoComposta->tipo))
                saida.push_back(todas[i]);
        return saida;
    }

    // Poe o foco numa coleção pelo PONTEIRO, voltando para Todos se a aba corrente
    // não a mostrar. Quem acabou de criar ou editar uma coleção tem de VÊ-LA: sumir
    // com ela porque a aba a esvazia é a pior forma de ensinar a regra -- e coleção
    // recém-criada é vazia, logo não está em aba nenhuma.
    //
    // Por ponteiro e não por nome: dois nomes iguais focariam a coleção errada.
    void FocarColecao(colecoes::Colecao *c)
    {
        g_iCol = 0;
        if (c == NULL) return;

        const int abaAntes = g_aba;
        for (int tentativa = 0; tentativa < 2; tentativa++)
        {
            std::vector<colecoes::Colecao *> L = ListaDeColecoes();
            for (size_t i = 0; i < L.size(); i++)
                if (L[i] == c) { g_iCol = (int)i; return; }
            g_aba = biblioteca::ABA_AUTOMATICA;
        }
        // Não achou nem em Todos: a coleção não está na lista por outro motivo, e
        // trocar a aba não resolveu nada. Desfaz, para não mexer no que funcionava.
        g_aba = abaAntes;
    }
    bool ColecaoEscolhida(int id)
    {
        for (size_t i = 0; i < g_escolhidas.size(); i++)
            if (g_escolhidas[i] == id) return true;
        return false;
    }

    bool OrigemMarcada(int id)
    {
        for (size_t i = 0; i < g_origens.size(); i++)
            if (g_origens[i] == id) return true;
        return false;
    }

    void PedirOQueFalta()
    {
        // A tela de coleções tambem carrega agora: tres capas por card. Sao as MESMAS
        // texturas que a colecao usa por dentro (a chave do cache e o ContentItemId),
        // entao isto deixa a tela de jogos mais rapida em vez de custar o dobro.
        // TELA_ARQUIVAR entra aqui: é grade de coleções como as outras duas. Sem
        // isto ela pediria as capas pela ListaAtual, que ali é vazia -- e todo card
        // sairia sem colagem, que é o sinal de coleção vazia.
        if (g_tela == TELA_COLECOES || g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR)
        {
            std::vector<colecoes::Colecao *> L = ListaDeColecoes();
            // Uma linha de folga para CADA lado, como a tela de jogos faz: sem a de
            // cima, rolar para tras mostra card sem capa por um instante.
            int base = (g_primeiraLinhaCol - 1) * COL_POR_LINHA;
            if (base < 0) base = 0;
            int fim  = (g_primeiraLinhaCol + COL_LINHAS + 1) * COL_POR_LINHA;
            if (fim > (int)L.size()) fim = (int)L.size();

            for (int i = base; i < fim; i++)
            {
                const biblioteca::Jogo *tres[3];
                int n = 0;
                TresDaColecao(L[i], tres, &n);

                for (int k = 0; k < n; k++)
                {
                    if ((int)g_emVoo.size() >= EM_VOO_MAX)
                        return;

                    int id = tres[k]->id;
                    if (NoCache(id, false) != NULL || EstaNaLista(g_emVoo, id) ||
                        EstaNaLista(g_falhou, id))
                        continue;

                    if (tres[k]->capa.empty())
                        continue;

                    carregador::Pedir(id, tres[k]->capa);
                    g_emVoo.push_back(id);
                }
            }
            return;
        }

        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        int base = (g_primeiraLinha - 1) * COLUNAS;
        if (base < 0) base = 0;
        int fim = (g_primeiraLinha + LINHAS + 1) * COLUNAS;
        if (fim > (int)L.size()) fim = (int)L.size();

        for (int volta = 0; volta < 2; volta++)
        {
            for (int i = base; i < fim; i++)
            {
                if ((int)g_emVoo.size() >= EM_VOO_MAX)
                    return;

                bool visivel = (i >= g_primeiraLinha * COLUNAS) &&
                               (i <  (g_primeiraLinha + LINHAS) * COLUNAS);
                if ((volta == 0) != visivel)
                    continue;

                int id = L[i]->id;
                if (NoCache(id, false) != NULL || EstaNaLista(g_emVoo, id) ||
                    EstaNaLista(g_falhou, id))
                    continue;

                if (L[i]->capa.empty())
                    continue;

                carregador::Pedir(id, L[i]->capa);
                g_emVoo.push_back(id);
            }
        }
    }

    void RecolherCarregadas()
    {
        int jogoId;
        std::vector<unsigned char> bytes;
        bool inteira = true;

        // UMA por quadro. Mesmo a 3 ms, cinco de uma vez dariam um solavanco.
        if (!carregador::Retirar(&jogoId, bytes, &inteira))
            return;

        for (size_t k = 0; k < g_emVoo.size(); k++)
        {
            if (g_emVoo[k] == jogoId) { g_emVoo.erase(g_emVoo.begin() + k); break; }
        }

        if (NoCache(jogoId, false) != NULL)
            return;                          // duplicado

        if (bytes.empty())
        {
            g_falhou.push_back(jogoId);      // não insistir a 60 Hz
            return;
        }

        // LIN_DXT, não DXT. No Xbox 360 a textura normal é LADRILHADA, e o DDS que vem
        // no .assets é LINEAR, como todo DDS de PC. Pedindo D3DFMT_DXT5 o D3DX tem de
        // converter o layout de cada capa; pedindo D3DFMT_LIN_DXT5 ele usa os bytes como
        // estão. É o que o FreeStyle faz (TextureCache.cpp:91), no mesmo console e com os
        // mesmos arquivos -- e era a única diferença entre a chamada dele e a nossa.
        // So faz sentido num DDS. Desde que a capa de ROM entrou, aqui tambem chega
        // JPG e PNG cru -- e ler o cabecalho DDS de um JPEG devolve lixo. Hoje o lixo
        // cai fora da faixa 1..4096 e e descartado, mas isso e sorte da codificacao, e
        // a decisao 115 convida o usuario a largar qualquer imagem na pasta.
        const bool ehDds = (bytes.size() > 88 && memcmp(&bytes[0], "DDS ", 4) == 0);

        D3DFORMAT formato = D3DFMT_UNKNOWN;
        if (ehDds)
        {
            if (memcmp(&bytes[84], "DXT5", 4) == 0)      formato = D3DFMT_LIN_DXT5;
            else if (memcmp(&bytes[84], "DXT1", 4) == 0) formato = D3DFMT_LIN_DXT1;
        }

        // Largura e altura explícitas, lidas do cabeçalho DDS -- que é little-endian,
        // ao contrário do container FSDA em volta. Com D3DX_DEFAULT_NONPOW2 o D3DX
        // decidia sozinho; dizendo o tamanho exato não há redimensionamento nenhum.
        UINT largura = 0, altura = 0;
        if (ehDds)
        {
            altura  = (UINT)bytes[12] | ((UINT)bytes[13] << 8) |
                      ((UINT)bytes[14] << 16) | ((UINT)bytes[15] << 24);
            largura = (UINT)bytes[16] | ((UINT)bytes[17] << 8) |
                      ((UINT)bytes[18] << 16) | ((UINT)bytes[19] << 24);
        }
        if (largura == 0 || largura > 4096 || altura == 0 || altura > 4096)
        {
            largura = D3DX_DEFAULT_NONPOW2;
            altura  = D3DX_DEFAULT_NONPOW2;
        }

        D3DTexture *textura = NULL;
        diario::Detalhe("criando textura %d (%u bytes, %ux%u)", jogoId,
                        (unsigned)bytes.size(), (unsigned)largura, (unsigned)altura);
        HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
            ATG::g_pd3dDevice, &bytes[0], (UINT)bytes.size(),
            largura, altura,
            1,                               // um mipmap só
            D3DUSAGE_CPU_CACHED_MEMORY, formato, D3DPOOL_DEFAULT,
            D3DX_FILTER_BOX, D3DX_FILTER_BOX,
            0, NULL, NULL, &textura);

        if (SUCCEEDED(hr) && textura != NULL)
        {
            MEMORYSTATUS mem; mem.dwLength = sizeof(mem);
            GlobalMemoryStatus(&mem);
            diario::Detalhe("textura %d (%u bytes, livre %u KB)", jogoId,
                            (unsigned)bytes.size(), (unsigned)(mem.dwAvailPhys / 1024));
            Guardar(jogoId, textura, inteira);
        }
        else
        {
            diario::Escrever("textura %d FALHOU: hr=0x%08X", jogoId, hr);
            g_falhou.push_back(jogoId);
        }
    }

    // ---- primitivas de desenho -------------------------------------------------
    //
    // Tudo aqui desenha com os QUATRO cantos explicitos, em triangulos. Nada de
    // D3DPT_RECTLIST com tres vertices: a doc do XDK e literal -- "Each set of three
    // vertices (upper-left corner, upper-right corner, and lower-left corner) defines a
    // SCREEN-ALIGNED quadrilateral" --, o hardware forca alinhamento aos eixos e deduz
    // o quarto canto por igualdade de coordenada. Capa girada sai em pe e com a largura
    // errada. O gradiente tinha o problema irmao: a COR do vertice deduzido tambem nao
    // e coisa que a gente controle.
    struct VertPC { XMFLOAT3 pos; D3DCOLOR cor; };
    struct VertPT { XMFLOAT3 pos; XMFLOAT2 uv;  };

    void Mistura(bool ligada)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, ligada ? TRUE : FALSE);
        if (ligada)
        {
            d->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
            d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            d->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);
        }
    }

    void SemZ()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->SetRenderState(D3DRS_ZENABLE, FALSE);
        d->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        d->SetRenderState(D3DRS_VIEWPORTENABLE, FALSE);

        // CULLMODE explicito: com RECTLIST nao havia culling nenhum ("No back-face
        // culling is performed"), mas TRIANGLELIST sofre -- "Back-face culling is
        // affected by the current winding-order render state". Funcionaria por heranca,
        // porque todo mundo em volta deixa CCW; mas o gradiente do fundo e o PRIMEIRO
        // desenho do quadro e herda o estado do quadro anterior. Um dia alguem grava CW
        // e a tela inteira apaga em silencio.
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    }

    void VoltaViewport()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->SetRenderState(D3DRS_VIEWPORTENABLE, TRUE);
        d->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);   // como o resto espera achar
    }

    D3DCOLOR Media(D3DCOLOR a, D3DCOLOR b)
    {
        return D3DCOLOR_ARGB((((a >> 24) & 0xFF) + ((b >> 24) & 0xFF)) / 2,
                             (((a >> 16) & 0xFF) + ((b >> 16) & 0xFF)) / 2,
                             (((a >>  8) & 0xFF) + ((b >>  8) & 0xFF)) / 2,
                             (( a        & 0xFF) + ( b        & 0xFF)) / 2);
    }

    // Gradiente por cor de vertice, com os QUATRO cantos explicitos.
    //
    // Era RECTLIST com tres vertices, deixando o hardware deduzir o quarto -- mas a COR
    // do vertice deduzido nao e coisa que a gente controle, e numa diagonal e justamente
    // o canto que importa. Dois triangulos custam o mesmo e sao deterministicos.
    void Gradiente(float x1, float y1, float x2, float y2,
                   D3DCOLOR inicio, D3DCOLOR fim, bool diagonal)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;

        D3DCOLOR cTL, cTR, cBL, cBR;
        if (diagonal)
        {
            cTL = inicio; cBR = fim;
            cTR = cBL = Media(inicio, fim);
        }
        else
        {
            cTL = cTR = inicio;
            cBL = cBR = fim;
        }

        VertPC v[6];
        v[0].pos = XMFLOAT3(x1, y1, 0); v[0].cor = cTL;
        v[1].pos = XMFLOAT3(x2, y1, 0); v[1].cor = cTR;
        v[2].pos = XMFLOAT3(x1, y2, 0); v[2].cor = cBL;
        v[3].pos = XMFLOAT3(x2, y1, 0); v[3].cor = cTR;
        v[4].pos = XMFLOAT3(x2, y2, 0); v[4].cor = cBR;
        v[5].pos = XMFLOAT3(x1, y2, 0); v[5].cor = cBL;

        ATG::SimpleShaders::SetDeclPosColor();
        ATG::SimpleShaders::BeginShader_PreTransformed_VertexColor();
        SemZ();
        Mistura(true);
        d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, v, sizeof(VertPC));
        Mistura(false);
        ATG::SimpleShaders::EndShader();
        VoltaViewport();
    }

    // Quad texturizado com os QUATRO cantos e UVs explicitos, em dois triangulos.
    //
    // Era RECTLIST com tres vertices, deixando o hardware deduzir o quarto. RECTLIST
    // existe para retangulo ALINHADO AOS EIXOS; com a capa girada a deducao nao
    // corresponde ao paralelogramo que a gente quer, e o quad sai deformado. Dois
    // triangulos custam o mesmo e nao dependem de suposicao nenhuma -- mesma licao do
    // gradiente, que tinha o problema irmao na COR do vertice deduzido.
    //
    // Ordem dos cantos: 0 superior esquerdo, 1 superior direito, 2 inferior direito,
    // 3 inferior esquerdo.
    void QuadTex(const XMFLOAT2 pos[4], const XMFLOAT2 uv[4],
                 D3DTexture *tex, D3DCOLOR cor, bool usarCor)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        const int ordem[6] = { 0, 1, 3,  1, 2, 3 };

        VertPT v[6];
        for (int i = 0; i < 6; i++)
        {
            const int k = ordem[i];
            v[i].pos = XMFLOAT3(pos[k].x, pos[k].y, 0);
            v[i].uv  = uv[k];
        }

        ATG::SimpleShaders::SetDeclPosTex();
        if (usarCor) ATG::SimpleShaders::BeginShader_PreTransformed_TexturedConstantColor(tex, cor);
        else         ATG::SimpleShaders::BeginShader_PreTransformed_Textured(tex);
        SemZ();
        d->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        d->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, v, sizeof(VertPT));
        ATG::SimpleShaders::EndShader();
        VoltaViewport();
    }

    // ---- canto arredondado -----------------------------------------------------
    //
    // Gerada no arranque, nao e arquivo: um quarto de circulo em alfa, 16x16. Desenhada
    // nos quatro cantos NA COR DO FUNDO, por cima da colagem, para "comer" o canto da
    // imagem -- o D3D nao tem recorte por caminho e o device nem tem stencil.
    //
    // LIN_A8R8G8B8 e nao A8R8G8B8: textura normal do 360 e LADRILHADA, e escrever
    // pixel a pixel numa ladrilhada daria lixo. Mesma licao das capas.
    const int CANTO_N = 16;
    D3DTexture *g_canto = NULL;        // mascara do lado de FORA da curva
    D3DTexture *g_arco[2] = { NULL, NULL };   // [0] fino (1 px), [1] grosso (3 px)

    D3DTexture *g_cantoInt = NULL;     // mascara do lado de DENTRO da curva

    // 'arco' negativo gera a mascara externa; positivo gera uma faixa sobre a curva;
    // ZERO gera a mascara interna, que PINTA o canto em vez de come-lo.
    D3DTexture *GerarCanto(float arco)
    {
        D3DTexture *t = NULL;
        if (FAILED(ATG::g_pd3dDevice->CreateTexture(CANTO_N, CANTO_N, 1, 0,
                                                    D3DFMT_LIN_A8R8G8B8,
                                                    D3DPOOL_DEFAULT, &t, NULL)))
            return NULL;

        D3DLOCKED_RECT tr;
        if (FAILED(t->LockRect(0, &tr, NULL, 0)))
        {
            t->Release();
            return NULL;
        }

        const float R = (float)CANTO_N;
        for (int y = 0; y < CANTO_N; y++)
        {
            DWORD *linha = (DWORD *)((BYTE *)tr.pBits + y * tr.Pitch);
            for (int x = 0; x < CANTO_N; x++)
            {
                float dx = R - ((float)x + 0.5f);
                float dy = R - ((float)y + 0.5f);
                float dist = sqrtf(dx * dx + dy * dy);
                float a;

                if (arco <= 0.0f)
                {
                    float d = dist - R + 0.5f;                  // >0 fora da curva
                    a = d < 0.0f ? 0.0f : (d > 1.0f ? 1.0f : d);
                    // A interna e o complemento da externa, e e com ela que o modal
                    // se arredonda: o painel e desenhado POR DENTRO da curva, por cima
                    // do veu. A externa nao serviria -- ela pinta na cor do fundo, e
                    // sobre o veu essa cor nao e uniforme (ha capa por baixo), entao o
                    // canto sairia como um entalhe escuro sobre as capas claras.
                    if (arco == 0.0f) a = 1.0f - a;
                }
                else
                {
                    // Faixa sobre a propria curva, para o anel de foco acompanhar o
                    // arredondamento em vez de ser cortado pela mascara.
                    float d = (dist - (R - arco * 0.5f)) / arco;  // 0..1 na faixa
                    float f = d < 0.0f ? -d : (d > 1.0f ? d - 1.0f : 0.0f);
                    a = f > 1.0f ? 0.0f : 1.0f - f;
                }
                linha[x] = ((DWORD)(a * 255.0f) << 24) | 0x00FFFFFF;
            }
        }
        t->UnlockRect(0);
        return t;
    }

    // Emblemas do canto do card: raio para a JUNÇÃO, duas lentes para o CRUZAMENTO.
    // Gerados como as máscaras de canto -- nenhum arquivo novo. Em 24x24 a serrilha
    // sai aceitável com quatro amostras por pixel, e o contorno escuro por baixo é o
    // que os faz aparecer sobre capa clara.
    const int RAIO_N = 24;
    D3DTexture *g_raio = NULL;      // junção
    D3DTexture *g_lentes = NULL;    // cruzamento

    bool DentroDoRaio(float x, float y)
    {
        // Sentido horário, em fração do quadrado.
        static const float px[7] = { 0.58f, 0.22f, 0.46f, 0.34f, 0.80f, 0.52f, 0.72f };
        static const float py[7] = { 0.04f, 0.56f, 0.56f, 0.96f, 0.42f, 0.42f, 0.04f };

        bool dentro = false;
        for (int i = 0, j = 6; i < 7; j = i++)
        {
            if ((py[i] > y) == (py[j] > y)) continue;
            float corte = (px[j] - px[i]) * (y - py[i]) / (py[j] - py[i]) + px[i];
            if (x < corte) dentro = !dentro;
        }
        return dentro;
    }

    // Dois circulos VAZADOS com a lente cheia -- a forma classica de intersecao.
    // Vazados de proposito: dois discos cheios a 22 px na tela viram um borrao, e e a
    // sobreposicao que carrega o significado.
    bool DentroDasLentes(float x, float y)
    {
        const float R = 0.27f, CY = 0.5f, AX = 0.35f, BX = 0.65f;
        const float ESP = 0.055f;      // meia espessura do contorno

        const float da = sqrtf((x - AX) * (x - AX) + (y - CY) * (y - CY));
        const float db = sqrtf((x - BX) * (x - BX) + (y - CY) * (y - CY));

        if (da <= R && db <= R) return true;               // a lente, cheia
        if (da > R - ESP && da < R + ESP) return true;     // contorno do primeiro
        if (db > R - ESP && db < R + ESP) return true;     // contorno do segundo
        return false;
    }

    typedef bool (*Forma)(float, float);

    D3DTexture *GerarEmblema(Forma dentroDaForma)
    {
        D3DTexture *t = NULL;
        if (FAILED(ATG::g_pd3dDevice->CreateTexture(RAIO_N, RAIO_N, 1, 0,
                                                    D3DFMT_LIN_A8R8G8B8,
                                                    D3DPOOL_DEFAULT, &t, NULL)))
            return NULL;

        D3DLOCKED_RECT tr;
        if (FAILED(t->LockRect(0, &tr, NULL, 0)))
        {
            t->Release();
            return NULL;
        }

        for (int y = 0; y < RAIO_N; y++)
        {
            DWORD *linha = (DWORD *)((BYTE *)tr.pBits + y * tr.Pitch);
            for (int x = 0; x < RAIO_N; x++)
            {
                // Quatro amostras por pixel: sem isso a diagonal do raio fica em
                // escada, e nesse tamanho a escada e o que mais se ve. A curva dos
                // circulos pede o mesmo.
                int dentro = 0;
                for (int sy = 0; sy < 2; sy++)
                    for (int sx = 0; sx < 2; sx++)
                        if (dentroDaForma(((float)x + 0.25f + sx * 0.5f) / RAIO_N,
                                          ((float)y + 0.25f + sy * 0.5f) / RAIO_N))
                            dentro++;

                linha[x] = ((DWORD)(dentro * 255 / 4) << 24) | 0x00FFFFFF;
            }
        }
        t->UnlockRect(0);
        return t;
    }

    void CriarCanto()
    {
        g_canto    = GerarCanto(-1.0f);
        g_cantoInt = GerarCanto(0.0f);
        g_arco[0]  = GerarCanto(1.0f);
        g_arco[1]  = GerarCanto(3.0f);

        if (g_cantoInt == NULL)
            diario::Escrever("AVISO: sem a mascara interna, o modal fica de canto reto");

        if (g_canto == NULL)
            diario::Escrever("AVISO: sem a mascara de canto, os cards ficam retos");
        if (g_arco[0] == NULL || g_arco[1] == NULL)
            diario::Escrever("AVISO: sem o arco, o contorno do card fica cortado no canto");
        g_raio   = GerarEmblema(DentroDoRaio);
        g_lentes = GerarEmblema(DentroDasLentes);
        if (g_raio == NULL)
            diario::Escrever("AVISO: sem o raio, a juncao nao se distingue no card");
        if (g_lentes == NULL)
            diario::Escrever("AVISO: sem as lentes, o cruzamento nao se distingue no card");

        if (g_canto != NULL)
            diario::Escrever("texturas de canto %dx%d e emblemas %dx%d geradas",
                             CANTO_N, CANTO_N, RAIO_N, RAIO_N);
    }

    // Retângulo CHEIO, com alfa de verdade.
    //
    // Não use DrawScreenSpaceTexturedRectColored com textura NULL para isto: ela faz
    // SetSampler(..., NULL) e desenha com o shader TEXTURIZADO, amostrando um sampler
    // sem nada ligado -- é o glitch que aparecia na letra acesa do índice. E a ATG
    // nunca liga D3DRS_ALPHABLENDENABLE nesse caminho (só o AtgFont mexe nisso), então
    // o alfa da cor era ignorado e o véu do jogo não marcado saía PRETO OPACO,
    // escondendo a capa inteira.
    //
    // DrawScreenSpaceRect com largura 0 desenha cheio e usa o shader de cor constante,
    // sem sampler nenhum. A mistura fica por nossa conta.
    void Preencher(const D3DRECT &r, D3DCOLOR cor)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        d->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        d->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        d->SetRenderState(D3DRS_BLENDOP,   D3DBLENDOP_ADD);

        ATG::DebugDraw::DrawScreenSpaceRect(r, 0.0f, cor);

        // O AtgFont salva e restaura este estado no Begin/End dele, mas só o que ELE
        // mexe. Devolver ao desligado é o que o resto do desenho espera.
        d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }

    // Quebra o nome em ate duas linhas, medindo com a PROPRIA fonte. A segunda linha e
    // cortada por nos, nao com ATGFONT_CENTER_X|ATGFONT_TRUNCATED juntos -- essa
    // combinacao tem um defeito conhecido: o teste de largura nao considera que o
    // cursor comeca deslocado meia largura, e o texto transborda em vez de cortar.
    // Maiuscula sem depender de locale: towupper/_wcsupr so sobem o ASCII enquanto a
    // localidade for a "C", e aqui ela e. A faixa 0xE0-0xFE do Latin-1 sobe subtraindo
    // 0x20 (a com til vira A com til), pulando o 0xF7 que e o sinal de divisao e nao
    // tem par maiusculo. E a mesma faixa que o Larga() ja usa de reserva, entao o que
    // entra aqui nunca passa disso.
    void ParaMaiusculas(WCHAR *t)
    {
        for (; *t != L'\0'; t++)
        {
            if (*t >= L'a' && *t <= L'z')          *t = (WCHAR)(*t - 0x20);
            else if (*t >= 0x00E0 && *t <= 0x00FE && *t != 0x00F7)
                                                   *t = (WCHAR)(*t - 0x20);
        }
    }

    // =========================================================================
    // O DESENHO METRO (decisão 149). Porte do previa/metro.js, tela por tela, com os
    // mesmos números: a prévia é o desenho aprovado, e o que mudar aqui muda lá.
    //
    //   fundo       degradê vertical do tema padrão do Metro
    //   abas        os hubs do Metro: nomes em minúsculas à esquerda, o ativo branco
    //   título      título e contador no canto de cima à direita
    //   coleções    o quadrado de hoje dentro do encarte branco do cartão de jogo; o
    //               encarte e o texto apagam no não selecionado
    //   jogos       os cartões do "My Games": encarte claro, capa e nome escuro
    //   foco        cresce 8 px para cada lado e levanta com sombra; sem anel
    //   marcado     o contorno verde de 2 px de hoje, e o véu no não marcado
    //   menu        o diálogo "Delete Profile" do Metro, medido em 720p
    //   detalhes    uma faixa que sobe de baixo (Y na grade de jogos)
    //   animações   foco, troca de aba (as duas páginas ao mesmo tempo), troca de
    //               tela (esmaecimento cruzado) e a faixa subindo, medidas num vídeo
    //               de captura direta do Metro 2.0.14448
    //   texto       sem a sombra do atlas (ATG::g_fonteSemSombra), menos no nome do
    //               quadrado de coleção, que é o de hoje
    // =========================================================================
    const D3DCOLOR M_TEXTO         = D3DCOLOR_XRGB(251, 250, 255);
    const D3DCOLOR M_APAGADO       = D3DCOLOR_XRGB(225, 224, 230);
    const D3DCOLOR M_LINHA         = D3DCOLOR_XRGB(130, 129, 135);
    const D3DCOLOR M_HUB           = D3DCOLOR_XRGB(178, 177, 184);
    const D3DCOLOR M_VERDE         = D3DCOLOR_XRGB(  0, 126,   4);
    const D3DCOLOR M_VERMELHO      = D3DCOLOR_XRGB(178,  34,  30);
    const D3DCOLOR M_CARTAO        = D3DCOLOR_XRGB(237, 236, 238);
    const D3DCOLOR M_CARTAO_TEXTO  = D3DCOLOR_XRGB( 45,  44,  45);
    const D3DCOLOR M_PAINEL_TEXTO  = D3DCOLOR_XRGB( 46,  53,  54);
    const D3DCOLOR M_PAINEL_ROTULO = D3DCOLOR_XRGB(104, 112, 114);
    const D3DCOLOR M_MARCADO       = D3DCOLOR_XRGB(155, 203,  60);   // o verde de hoje
    const D3DCOLOR M_SEM_CAPA      = D3DCOLOR_XRGB(120, 119, 124);

    const int   CRESCE = 8, CARTAO_PAD = 6, CARTAO_NOME = 34;
    const int   COL_TOPO_ABA = 96;          // a grade de coleções, embaixo das abas
    const float TAU_FOCO = 40.0f, TAU_HUB = 220.0f, FADE_MS = 200.0f, TAU_FAIXA = 70.0f;
    const float RODAPE_ESCALA = 0.67f;

    // ---- texto ------------------------------------------------------------------
    void Escala(float e) { g_fonte.SetScaleFactors(e, e); }

    // GetTextExtent multiplica pela escala corrente (AtgFont.cpp:550): mede na escala
    // em que desenha. É CPU pura, vale dentro ou fora do Begin.
    float Largura(const WCHAR *s, float e)
    {
        FLOAT w = 0.0f, h = 0.0f;
        Escala(e);
        g_fonte.GetTextExtent(s, &w, &h);
        Escala(1.0f);
        return w;
    }

    // Dentro do Begin.
    void Texto(float x, float y, D3DCOLOR cor, float e, const WCHAR *s, DWORD alin = 0)
    {
        if (s == NULL || s[0] == L'\0') return;
        Escala(e);
        g_fonte.DrawText(x, y, cor, s, alin);
        Escala(1.0f);
    }

    // Corta com reticências o que passar de maxw. Busca binária pelo maior começo que
    // cabe com "...": a descrição chega aqui com centenas de letras.
    void Cortar(const WCHAR *txt, float maxw, float e, WCHAR *saida, int cap)
    {
        static WCHAR t[1100];
        wcsncpy(saida, txt, cap - 1); saida[cap - 1] = L'\0';
        if (Largura(saida, e) <= maxw) return;
        int lo = 0, hi = (int)wcslen(saida);
        if (hi > cap - 4) hi = cap - 4;
        if (hi > 1090) hi = 1090;
        while (lo < hi)
        {
            const int m = (lo + hi + 1) / 2;
            wcsncpy(t, saida, m); t[m] = L'\0'; wcscat(t, L"...");
            if (Largura(t, e) <= maxw) lo = m; else hi = m - 1;
        }
        saida[lo] = L'\0';
        wcscat(saida, L"...");
    }

    // Quebra em até maxLinhas linhas de maxw; a última leva o resto, cortado com
    // reticências. A mesma regra do quebrar() da prévia.
    const int LIN_CAP = 200;
    int Quebrar(const WCHAR *txt, float maxw, int maxLinhas, float e, WCHAR (*linhas)[LIN_CAP])
    {
        static WCHAR atual[1100], tenta[1100], resto[1100];
        int n = 0;
        atual[0] = L'\0';
        if (maxLinhas <= 0) return 0;
        const WCHAR *p = txt;
        while (*p != L'\0')
        {
            while (*p == L' ') p++;
            if (*p == L'\0') break;
            const WCHAR *ini = p;
            while (*p != L'\0' && *p != L' ') p++;
            const int lp = (int)(p - ini), la = (int)wcslen(atual);
            if (la + 1 + lp >= 1099) break;

            wcscpy(tenta, atual);
            if (la) wcscat(tenta, L" ");
            wcsncat(tenta, ini, lp);
            if (Largura(tenta, e) <= maxw) { wcscpy(atual, tenta); continue; }

            if (n == maxLinhas - 1)
            {
                wcscpy(resto, atual);
                if (la) wcscat(resto, L" ");
                wcsncat(resto, ini, 1099 - wcslen(resto));
                Cortar(resto, maxw, e, linhas[n], LIN_CAP);
                return n + 1;
            }
            if (la) { Cortar(atual, maxw, e, linhas[n], LIN_CAP); n++; }
            wcsncpy(atual, ini, lp); atual[lp] = L'\0';
        }
        if (atual[0] != L'\0') { Cortar(atual, maxw, e, linhas[n], LIN_CAP); n++; }
        return n;
    }

    // Minúscula sem locale, pelo mesmo motivo do ParaMaiusculas: ASCII e a faixa
    // 0xC0-0xDE do Latin-1 (menos o 0xD7, o sinal de vezes).
    void ParaMinusculas(WCHAR *t)
    {
        for (; *t != L'\0'; t++)
        {
            if (*t >= L'A' && *t <= L'Z')          *t = (WCHAR)(*t + 0x20);
            else if (*t >= 0x00C0 && *t <= 0x00DE && *t != 0x00D7)
                                                   *t = (WCHAR)(*t + 0x20);
        }
    }

    // ---- formas -----------------------------------------------------------------
    void Ret(float x, float y, float l, float a, D3DCOLOR cor)
    {
        D3DRECT r;
        r.x1 = (LONG)floorf(x + 0.5f);     r.y1 = (LONG)floorf(y + 0.5f);
        r.x2 = (LONG)floorf(x + l + 0.5f); r.y2 = (LONG)floorf(y + a + 0.5f);
        if (r.x2 <= r.x1 || r.y2 <= r.y1) return;
        Preencher(r, cor);
    }

    D3DCOLOR ComAlfa(D3DCOLOR cor, float alfa)
    {
        if (alfa < 0.0f) alfa = 0.0f;
        if (alfa > 1.0f) alfa = 1.0f;
        return (cor & 0x00FFFFFF) | ((DWORD)(alfa * 255.0f + 0.5f) << 24);
    }

    D3DCOLOR Mistura3(const int de[3], const int para[3], float t)
    {
        return D3DCOLOR_XRGB((int)(de[0] + (para[0] - de[0]) * t + 0.5f),
                             (int)(de[1] + (para[1] - de[1]) * t + 0.5f),
                             (int)(de[2] + (para[2] - de[2]) * t + 0.5f));
    }

    // O levantado do foco (regra 2 do metro.js): duas camadas pretas deslocadas para
    // baixo e para a direita; quem está em cima cobre o resto.
    void Sombra(float x, float y, float l, float a, float alfa)
    {
        Ret(x + 2.0f, y + 3.0f, l + 4.0f, a + 4.0f, ComAlfa(0, 0.12f * alfa));
        Ret(x + 4.0f, y + 6.0f, l, a, ComAlfa(0, 0.18f * alfa));
    }

    void Contorno(float x, float y, float l, float a, D3DCOLOR cor)
    {
        D3DRECT r;
        r.x1 = (LONG)x; r.y1 = (LONG)y; r.x2 = (LONG)(x + l); r.y2 = (LONG)(y + a);
        ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, cor);
    }

    // Disco cheio de raio r, com a máscara interna do canto nos quatro quadrantes.
    void Disco(float cx, float cy, float r, D3DCOLOR cor)
    {
        if (g_cantoInt == NULL) { Ret(cx - r, cy - r, 2 * r, 2 * r, cor); return; }
        const float px[4] = { cx - r, cx,     cx - r, cx };
        const float py[4] = { cy - r, cy - r, cy,     cy };
        const int fx[4] = { 0, 1, 0, 1 }, fy[4] = { 0, 0, 1, 1 };
        Mistura(true);
        for (int c = 0; c < 4; c++)
        {
            XMFLOAT2 pos[4], uv[4];
            pos[0] = XMFLOAT2(px[c],     py[c]);
            pos[1] = XMFLOAT2(px[c] + r, py[c]);
            pos[2] = XMFLOAT2(px[c] + r, py[c] + r);
            pos[3] = XMFLOAT2(px[c],     py[c] + r);
            const float u0 = (float)fx[c], u1 = 1.0f - u0, v0 = (float)fy[c], v1 = 1.0f - v0;
            uv[0] = XMFLOAT2(u0, v0); uv[1] = XMFLOAT2(u1, v0);
            uv[2] = XMFLOAT2(u1, v1); uv[3] = XMFLOAT2(u0, v1);
            QuadTex(pos, uv, g_cantoInt, cor, true);
        }
        Mistura(false);
    }

    void FundoMetro()
    {
        // (80,78,84) no topo, (177,176,180) a 78% da altura e um chão (162,160,164).
        const float y78 = 720.0f * 0.78f;
        Gradiente(0.0f, 0.0f, 1280.0f, y78, D3DCOLOR_XRGB(80, 78, 84), D3DCOLOR_XRGB(177, 176, 180), false);
        Gradiente(0.0f, y78, 1280.0f, 720.0f, D3DCOLOR_XRGB(177, 176, 180), D3DCOLOR_XRGB(162, 160, 164), false);
    }

    // ---- animação: o foco ---------------------------------------------------------
    // O em foco cresce ~10 px em ~120 ms, desacelerando; o que perdeu o foco encolhe ao
    // mesmo tempo. Exponencial com constante de 40 ms. O que perdeu o foco é desenhado
    // antes do novo, e é ali que a troca começa (a mesma conta do progressoDoFoco).
    const int SEM_FOCO = 0x7FFFFFFF;
    int   g_focoAtual = SEM_FOCO, g_focoAntes = SEM_FOCO;
    DWORD g_focoT0 = 0;
    bool  g_estatico = false;        // desenhando a página que sai na troca de aba

    float ProgressoDoFoco(int chave, bool sel)
    {
        if (g_estatico) return sel ? 1.0f : 0.0f;
        if (!sel && chave == g_focoAtual) { g_focoAntes = chave; g_focoAtual = SEM_FOCO; g_focoT0 = GetTickCount(); }
        if (sel && chave != g_focoAtual)
        {
            if (g_focoAtual != SEM_FOCO) g_focoAntes = g_focoAtual;
            g_focoAtual = chave; g_focoT0 = GetTickCount();
        }
        const float p = 1.0f - expf(-(float)(GetTickCount() - g_focoT0) / TAU_FOCO);
        if (sel) return p;
        return (chave == g_focoAntes) ? 1.0f - p : 0.0f;
    }

    // ---- animação: a troca de aba ---------------------------------------------------
    // As duas grades no mesmo quadro: a nova entra de um lado enquanto a anterior sai
    // pelo outro, com o estado que ela tinha. A distância que falta cai a ~63% a cada
    // 100 ms (constante de 220 ms). Abas, título e rodapé ficam parados.
    struct EstadoTela { Tela tela; int aba, iCol, primeiraLinhaCol, iJogo, primeiraLinha; };
    EstadoTela g_hubAntes;
    DWORD g_hubT0 = 0;
    int   g_hubDir = 0;
    bool  g_hubVivo = false;
    int   g_dx = 0;                  // deslocamento das grades neste desenho

    EstadoTela FotoDoEstado()
    {
        EstadoTela e;
        e.tela = g_tela; e.aba = g_aba; e.iCol = g_iCol; e.primeiraLinhaCol = g_primeiraLinhaCol;
        e.iJogo = g_iJogo; e.primeiraLinha = g_primeiraLinha;
        return e;
    }
    void AplicarEstado(const EstadoTela &e)
    {
        g_tela = e.tela; g_aba = e.aba; g_iCol = e.iCol; g_primeiraLinhaCol = e.primeiraLinhaCol;
        g_iJogo = e.iJogo; g_primeiraLinha = e.primeiraLinha;
    }
    void ComecarTrocaDeHub(const EstadoTela &antes, int dir)
    {
        g_hubAntes = antes; g_hubDir = dir; g_hubT0 = GetTickCount(); g_hubVivo = true;
    }

    // ---- animação: a faixa de detalhes ------------------------------------------------
    bool  g_detalhe = false, g_faixaFechando = false;
    DWORD g_faixaT0 = 0;
    const biblioteca::Jogo *g_detalheJogo = NULL;

    // ---- abas, título e rodapé (dentro do Begin) ---------------------------------
    void Hubs()
    {
        if (g_abas.size() < 2) return;
        float x = (float)MARGEM_X;
        for (size_t i = 0; i < g_abas.size(); i++)
        {
            WCHAR nome[64];
            wcsncpy(nome, NomeDaAba(g_abas[i]), 63); nome[63] = L'\0';
            ParaMinusculas(nome);
            Texto(x, 24.0f, (g_abas[i] == g_aba) ? M_TEXTO : M_HUB, 1.0f, nome);
            x += Largura(nome, 1.0f) + 26.0f;
        }
    }

    // Regra 5: título e contador no canto de cima à direita, como "My Games / 11 of 42".
    void TituloDaTela(const WCHAR *titulo, const WCHAR *contador)
    {
        if (titulo != NULL && titulo[0] != L'\0')
        {
            WCHAR t[LIN_CAP];
            Cortar(titulo, 560.0f, 1.5f, t, LIN_CAP);
            Texto(1180.0f, 20.0f, M_TEXTO, 1.5f, t, ATGFONT_RIGHT);
        }
        if (contador != NULL && contador[0] != L'\0')
            Texto(1180.0f, 60.0f, M_APAGADO, 0.8f, contador, ATGFONT_RIGHT);
    }

    void Rodape(const WCHAR *botoes)
    {
        Texto((float)MARGEM_X, 660.0f, M_TEXTO, RODAPE_ESCALA, botoes);
    }

    // ---- o quadrado de coleção ------------------------------------------------------
    // A colagem de hoje no grafite neutro do Metro: base chapada, três capas a -30°, o
    // tom por cima e o véu embaixo do nome. "vivo" (o progresso do foco) leva a
    // colagem de 60% a 92% e tira metade do tom.
    void Grafite(float x, float y, float l, float a, const colecoes::Colecao *col, float vivo)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        Ret(x, y, l, a, D3DCOLOR_XRGB(46, 46, 50));

        const biblioteca::Jogo *tres[3];
        int n = 0;
        TresDaColecao(col, tres, &n);
        if (n <= 0) return;

        // Scissor: o recorte sem stencil, e o quadrado é alinhado aos eixos. Preso à
        // tela: na troca de aba o quadrado passa da borda.
        RECT sc;
        sc.left = (LONG)x; sc.top = (LONG)y; sc.right = (LONG)(x + l); sc.bottom = (LONG)(y + a);
        if (sc.left < 0) sc.left = 0;
        if (sc.right > 1280) sc.right = 1280;
        if (sc.right <= sc.left) return;
        d->SetScissorRect(&sc);
        d->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

        const float w = l * 0.56f, h = w * 1.425f, g = w * 0.14f;
        const float ang = -30.0f * 3.14159265f / 180.0f, co = cosf(ang), si = sinf(ang);
        const float cx = x + l * 0.5f, cy = y + a * 0.5f;
        const float ox[3] = { -(w + g), 0.0f, 0.0f };
        const float oy[3] = { -h * 0.5f, -(h + g * 0.5f), g * 0.5f };
        const D3DCOLOR alfa = ComAlfa(0x00FFFFFF, 0.60f + 0.32f * vivo);

        Mistura(true);
        for (int k = 0; k < n; k++)
        {
            bool inteira = true;
            D3DTexture *capa = NoCache(tres[k]->id, true, &inteira);
            if (capa == NULL) continue;
            const float u0 = inteira ? 0.0f : FRENTE_U0;
            XMFLOAT2 uv[4];
            uv[0] = XMFLOAT2(u0, 0.0f); uv[1] = XMFLOAT2(1.0f, 0.0f);
            uv[2] = XMFLOAT2(1.0f, 1.0f); uv[3] = XMFLOAT2(u0, 1.0f);
            const float canto[4][2] = { { ox[k], oy[k] }, { ox[k] + w, oy[k] },
                                        { ox[k] + w, oy[k] + h }, { ox[k], oy[k] + h } };
            XMFLOAT2 pos[4];
            for (int c = 0; c < 4; c++)
                pos[c] = XMFLOAT2(cx + canto[c][0] * co - canto[c][1] * si,
                                  cy + canto[c][0] * si + canto[c][1] * co);
            QuadTex(pos, uv, capa, alfa, true);
        }
        Mistura(false);
        d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

        const float tom = 1.0f - 0.5f * vivo;
        Gradiente(x, y, x + l, y + a, ComAlfa(D3DCOLOR_XRGB(28, 28, 32), 0.659f * tom),
                  ComAlfa(D3DCOLOR_XRGB(40, 40, 45), 0.38f * tom), false);
        Gradiente(x, y + a * 0.42f, x + l, y + a, D3DCOLOR_ARGB(0, 26, 26, 29),
                  D3DCOLOR_ARGB(240, 26, 26, 29), false);
    }

    // Raio para a junção, lentes para o cruzamento, no canto de cima à direita.
    void Emblema(float x, float y, float lado, int tipo)
    {
        D3DTexture *emb = (tipo == colecoes::TIPO_UNIAO) ? g_raio
                        : (tipo == colecoes::TIPO_INTERSECAO) ? g_lentes : NULL;
        if (emb == NULL) return;
        const float M = 22.0f, folga = 10.0f, rx = x + lado - folga - M, ry = y + folga;
        XMFLOAT2 pos[4], sombra[4], uv[4];
        pos[0] = XMFLOAT2(rx, ry); pos[1] = XMFLOAT2(rx + M, ry);
        pos[2] = XMFLOAT2(rx + M, ry + M); pos[3] = XMFLOAT2(rx, ry + M);
        uv[0] = XMFLOAT2(0.0f, 0.0f); uv[1] = XMFLOAT2(1.0f, 0.0f);
        uv[2] = XMFLOAT2(1.0f, 1.0f); uv[3] = XMFLOAT2(0.0f, 1.0f);
        for (int c = 0; c < 4; c++) sombra[c] = XMFLOAT2(pos[c].x + 1.0f, pos[c].y + 1.0f);
        Mistura(true);
        QuadTex(sombra, uv, emb, D3DCOLOR_ARGB(170, 0, 0, 0), true);
        QuadTex(pos, uv, emb, COR_ANEL, true);
        Mistura(false);
    }

    int ItensDaColecao(const colecoes::Colecao *c)
    {
        int quantos = 0;
        for (size_t j = 0; j < g_jogos.size(); j++)
            if (NaAba(g_jogos[j]) && colecoes::Tem(c, g_jogos[j].titleId))
                quantos++;
        return quantos;
    }

    // O encarte (6 px, o CARTAO_PAD do jogo) entra TIRANDO do quadrado: o todo segue
    // com COL_LADO. Apagado no não selecionado na proporção do texto de hoje (~62%:
    // 142,156,148 contra 231,237,233), aceso com o progresso do foco.
    void CartaoColecao(int x, int y, const colecoes::Colecao *col, bool sel, bool escolhendo)
    {
        static const int ENC_APAGADO[3] = { 147, 146, 148 }, ENC_ACESO[3] = { 237, 236, 238 };
        const float prog = ProgressoDoFoco(-(col->id + 1), sel);
        const int d = (int)floorf(CRESCE * prog + 0.5f);
        const float X = (float)(x - d), Y = (float)(y - d), LADO = (float)(COL_LADO + 2 * d);
        if (prog > 0.02f) Sombra(X, Y, LADO, LADO, prog);

        Ret(X, Y, LADO, LADO, Mistura3(ENC_APAGADO, ENC_ACESO, prog));
        const float P = (float)CARTAO_PAD, ix = X + P, iy = Y + P, il = LADO - 2 * P, ia = LADO - 2 * P;
        Grafite(ix, iy, il, ia, col, prog);
        Emblema(ix, iy, il, col->tipo);

        bool marcada = false;
        if (escolhendo)
        {
            marcada = (g_tela == TELA_ORIGENS) ? OrigemMarcada(col->id) : ColecaoEscolhida(col->id);
            if (!marcada) Ret(X, Y, LADO, LADO, D3DCOLOR_ARGB(150, 6, 9, 8));   // o véu de hoje
        }

        // O nome é o de hoje: maiúsculas, centrado, ancorado a 86% da altura, com a
        // sombra do atlas e as cores de hoje.
        WCHAR nome[256], linhas[2][LIN_CAP], texto[32];
        Larga(col->nome, nome, 256);
        ParaMaiusculas(nome);
        const int n = Quebrar(nome, il - 28.0f, 2, 1.0f, linhas);
        const float ALT_LINHA = 28.0f, ALT_CONT = 24.0f, meio = ix + il * 0.5f;
        const float topo = iy + ia * 0.86f - (n * ALT_LINHA + ALT_CONT);
        swprintf_s(texto, 32, L"( %d )", ItensDaColecao(col));

        ATG::g_fonteSemSombra = FALSE;
        g_fonte.Begin();
        for (int k = 0; k < n; k++)
            Texto(meio, topo + k * ALT_LINHA, sel ? COR_TEXTO : COR_APAGADO, 1.0f, linhas[k], ATGFONT_CENTER_X);
        Texto(meio, topo + n * ALT_LINHA, COR_FRACO, 1.0f, texto, ATGFONT_CENTER_X);
        g_fonte.End();
        ATG::g_fonteSemSombra = TRUE;

        if (escolhendo && marcada) Contorno(ix + 1.0f, iy + 1.0f, il - 2.0f, ia - 2.0f, M_MARCADO);
    }

    void GradeColecoes()
    {
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();
        if (L.empty()) return;
        const bool escolhendo = (g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR);
        const int base = g_primeiraLinhaCol * COL_POR_LINHA;
        int foco = -1, fx = 0, fy = 0;
        for (int k = 0; k < COL_POR_PAGINA; k++)
        {
            const int i = base + k;
            if (i >= (int)L.size()) break;
            const int x = COL_MARGEM + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP) + g_dx;
            const int y = COL_TOPO_ABA + (k / COL_POR_LINHA) * (COL_LADO + COL_GAP);
            if (i == g_iCol) { foco = i; fx = x; fy = y; continue; }   // o em foco por cima
            CartaoColecao(x, y, L[i], false, escolhendo);
        }
        if (foco >= 0) CartaoColecao(fx, fy, L[foco], true, escolhendo);
    }

    // ---- o cartão de jogo ("My Games") ------------------------------------------------
    void CartaoJogo(int x1, int y1, const biblioteca::Jogo *g, bool sel, bool adicionando)
    {
        const float prog = ProgressoDoFoco(g->id + 1, sel);
        const int d = (int)floorf(CRESCE * prog + 0.5f);
        const float cx = (float)(x1 - CARTAO_PAD - d), cy = (float)(y1 - CARTAO_PAD - d);
        const float cl = (float)(CAPA_L + 2 * CARTAO_PAD + 2 * d);
        const float ca = (float)(CAPA_A + 2 * CARTAO_PAD + CARTAO_NOME + 2 * d);
        if (prog > 0.02f) Sombra(cx, cy, cl, ca, prog);
        Ret(cx, cy, cl, ca, M_CARTAO);

        D3DRECT r;
        r.x1 = x1 - d; r.y1 = y1 - d; r.x2 = x1 + CAPA_L + d; r.y2 = y1 + CAPA_A + d;
        bool inteira = true;
        D3DTexture *capa = NoCache(g->id, true, &inteira);
        if (capa != NULL)
        {
            // Da capa da FreeStyle sai só a frente; a de ROM vai inteira.
            const float u0 = inteira ? 0.0f : FRENTE_U0;
            ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch(
                r, XMFLOAT2(u0, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT2(u0, 1.0f), capa);
        }
        else Preencher(r, M_SEM_CAPA);

        if (adicionando)
        {
            // Como hoje: o não marcado leva o véu na capa, o marcado o contorno verde.
            if (SelecionadoNoRascunho(g->titleId))
            {
                if (g_anelMarcado) ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, M_MARCADO);
            }
            else Preencher(r, D3DCOLOR_ARGB(140, 6, 9, 8));
        }

        WCHAR nome[256], cortado[LIN_CAP];
        Larga(g->nome, nome, 256);
        const float il = (float)(r.x2 - r.x1);
        Cortar(nome, il, 0.9f, cortado, LIN_CAP);
        g_fonte.Begin();
        Texto((float)r.x1, (float)r.y2 + 6.0f, M_CARTAO_TEXTO, 0.9f, cortado);
        g_fonte.End();
    }

    void IndiceAlfabetico(const std::vector<const biblioteca::Jogo *> &L)
    {
        if (L.empty()) return;
        bool tem[27];
        for (int i = 0; i < 27; i++) tem[i] = false;
        for (size_t i = 0; i < L.size(); i++)
        {
            const char c = Inicial(L[i]->nome);
            tem[(c == '#') ? 26 : (c - 'A')] = true;
        }
        const char atual = Inicial(L[g_iJogo < (int)L.size() ? g_iJogo : 0]->nome);
        const float passo = (640.0f - 92.0f) / 27.0f, ESCALA = 0.72f, ALT_LETRA = 27.0f * ESCALA;
        const float dx = (float)g_dx;

        for (int i = 0; i < 27; i++)
            if (((i == 26) ? '#' : (char)('A' + i)) == atual)
                Ret(1204.0f + dx, 92.0f + i * passo, 28.0f, passo, M_VERDE);

        g_fonte.Begin();
        for (int i = 0; i < 27; i++)
        {
            const float y = 92.0f + i * passo;
            WCHAR letra[2] = { (WCHAR)((i == 26) ? L'#' : (L'A' + i)), L'\0' };
            const bool acesa = ((i == 26) ? '#' : (char)('A' + i)) == atual;
            Texto(1218.0f + dx, y + (passo - ALT_LETRA) * 0.5f,
                  (acesa || tem[i]) ? M_TEXTO : M_LINHA, ESCALA, letra, ATGFONT_CENTER_X);
        }
        g_fonte.End();
    }

    void GradeJogos()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        const int total = (int)L.size();
        if (total == 0) return;
        const bool adicionando = (g_tela == TELA_ADICIONAR);
        const int base = g_primeiraLinha * COLUNAS;
        int foco = -1, fx = 0, fy = 0;
        for (int k = 0; k < POR_PAGINA; k++)
        {
            const int i = base + k;
            if (i >= total) break;
            const int x1 = GRID_MARGEM + (k % COLUNAS) * (CAPA_L + ESPACO_X) + g_dx;
            const int y1 = TOPO + 4 + (k / COLUNAS) * (CAPA_A + ESPACO_Y);
            if (i == g_iJogo) { foco = i; fx = x1; fy = y1; continue; }
            CartaoJogo(x1, y1, L[i], false, adicionando);
        }
        if (foco >= 0) CartaoJogo(fx, fy, L[foco], true, adicionando);
        IndiceAlfabetico(L);
    }

    void GradeDaTela()
    {
        if (g_tela == TELA_COLECOES || g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR) GradeColecoes();
        else GradeJogos();
    }

    // A grade da tela e, durante a troca de aba, a anterior um passo de página atrás.
    void Grades()
    {
        g_dx = 0;
        if (!g_hubVivo || (g_tela != TELA_COLECOES && g_tela != TELA_SOLTOS)) { GradeDaTela(); return; }
        const float t = (float)(GetTickCount() - g_hubT0);
        if (t >= 5.0f * TAU_HUB) { g_hubVivo = false; GradeDaTela(); return; }
        const float dx = g_hubDir * 1280.0f * expf(-t / TAU_HUB);

        g_dx = (int)floorf(dx + 0.5f);
        GradeDaTela();

        const EstadoTela agora = FotoDoEstado();
        AplicarEstado(g_hubAntes);
        g_estatico = true;
        g_dx = (int)floorf(dx - g_hubDir * 1280.0f + 0.5f);
        GradeDaTela();
        g_estatico = false;
        AplicarEstado(agora);
        g_dx = 0;
    }

    // ---- as telas ---------------------------------------------------------------------
    void TelaColecoes()
    {
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();
        const bool arquivando = (g_tela == TELA_ARQUIVAR);
        const bool escolhendo = (g_tela == TELA_ORIGENS || arquivando);
        WCHAR texto[256];

        if (L.empty())
        {
            // Interseção aceita junção como origem, junção não aceita: a frase diz o
            // que FALTA, e não a mesma coisa para os dois.
            const WCHAR *vazia = L"No collections yet";
            if (arquivando) vazia = L"No collections to add it to";
            else if (g_tela == TELA_ORIGENS && g_editandoComposta != NULL)
                vazia = (g_editandoComposta->tipo == colecoes::TIPO_UNIAO)
                      ? L"No game collections to combine" : L"No collections to intersect";
            g_fonte.Begin();
            Texto(640.0f, 320.0f, M_TEXTO, 1.3f, vazia, ATGFONT_CENTER_X);
            // Sem coleção nenhuma o exportar continua valendo: ele é da biblioteca.
            Rodape(escolhendo ? GLYPH_B_BUTTON L" Cancel"
                              : GLYPH_X_BUTTON L" New collection     " GLYPH_BACK_BUTTON L" Export");
            g_fonte.End();
            return;
        }

        Grades();

        g_fonte.Begin();
        if (!escolhendo)
        {
            Hubs();
            swprintf_s(texto, 256, L"%d of %d", g_iCol + 1, (int)L.size());
            TituloDaTela(L"", texto);
            Rodape(GLYPH_A_BUTTON L" Open     " GLYPH_X_BUTTON L" New collection     "
                   GLYPH_START_BUTTON L" Options     " GLYPH_BACK_BUTTON L" Export");
        }
        else if (arquivando)
        {
            // Quem diz QUAL jogo está sendo arquivado é o contador, cortado em 400 px.
            WCHAR nome[256], curto[LIN_CAP];
            Larga(g_arquivando != NULL ? g_arquivando->nome : "", nome, 256);
            Cortar(nome, 400.0f, 0.8f, curto, LIN_CAP);
            const int q = (int)g_escolhidas.size();
            swprintf_s(texto, 256, L"%s · %d %s", curto, q, (q == 1) ? L"collection" : L"collections");
            TituloDaTela(L"add to collections", texto);
            Rodape(GLYPH_A_BUTTON L" Select     " GLYPH_B_BUTTON L" Cancel     " GLYPH_START_BUTTON L" Done");
        }
        else
        {
            // Quantos jogos o rascunho daria se fosse concluído agora: um cruzamento
            // esvazia fácil, e descobrir o ( 0 ) depois de gravar é tarde.
            colecoes::Colecao rascunho;
            rascunho.id = 0;
            rascunho.tipo = (g_editandoComposta != NULL) ? g_editandoComposta->tipo : colecoes::TIPO_UNIAO;
            rascunho.origens = g_origens;
            int quantos = 0;
            for (size_t j = 0; j < g_jogos.size(); j++)
                if (colecoes::Tem(&rascunho, g_jogos[j].titleId)) quantos++;
            swprintf_s(texto, 256, L"%d chosen · %d games", (int)g_origens.size(), quantos);
            TituloDaTela(L"choose collections", texto);
            Rodape(GLYPH_A_BUTTON L" Select     " GLYPH_B_BUTTON L" Cancel     " GLYPH_START_BUTTON L" Done");
        }
        g_fonte.End();
    }

    void TelaJogos()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        const bool adicionando = (g_tela == TELA_ADICIONAR);
        const bool soltos = (g_tela == TELA_SOLTOS);
        const int total = (int)L.size();
        WCHAR texto[256], titulo[256];

        if (total == 0)
        {
            // Composta vazia não se resolve adicionando jogos; o cruzamento vazio diz o
            // porquê. E a aba pode ter esvaziado a coleção por dentro: aí a frase diz.
            const WCHAR *vazia = L"No games in this collection";
            if (!adicionando && g_atual != NULL)
            {
                if      (g_atual->tipo == colecoes::TIPO_INTERSECAO) vazia = L"No game is in all of them";
                else if (g_atual->tipo == colecoes::TIPO_UNIAO)      vazia = L"No games in the chosen collections";
            }
            WCHAR porCausaDaAba[128];
            if (!adicionando && !soltos && g_aba != biblioteca::ABA_AUTOMATICA)
            {
                swprintf_s(porCausaDaAba, 128, L"No %s games in this collection", NomeDaAba(g_aba));
                vazia = porCausaDaAba;
            }
            if (soltos) vazia = L"No games outside your collections";
            g_fonte.Begin();
            if (soltos) Hubs();
            Texto(640.0f, 320.0f, M_TEXTO, 1.3f, adicionando ? L"The library is empty" : vazia, ATGFONT_CENTER_X);
            Rodape(soltos ? GLYPH_B_BUTTON L" Back"
                 : adicionando ? GLYPH_B_BUTTON L" Cancel"
                 : colecoes::Composta(g_atual) ? GLYPH_B_BUTTON L" Back     " GLYPH_START_BUTTON L" Options"
                 : GLYPH_B_BUTTON L" Back     " GLYPH_X_BUTTON L" Add games");
            g_fonte.End();
            return;
        }

        Grades();

        g_fonte.Begin();
        if (soltos) Hubs();
        // O "show me" do My Games: a aba que filtra, no alto à esquerda. Só quando filtra:
        // no All, que é o padrão, não diz nada. Ao adicionar a aba não filtra.
        if (!soltos && !adicionando && g_aba != biblioteca::ABA_AUTOMATICA)
        {
            WCHAR aba[64];
            wcsncpy(aba, NomeDaAba(g_aba), 63); aba[63] = L'\0';
            ParaMinusculas(aba);
            Texto((float)MARGEM_X, 18.0f, M_APAGADO, 0.8f, L"showing");
            Texto((float)MARGEM_X, 38.0f, M_TEXTO, 1.0f, aba);
        }
        if (adicionando)
        {
            swprintf_s(texto, 256, L"%d of %d selected", ItensMarcados(), (int)g_jogos.size());
            TituloDaTela(L"add games", texto);
            Rodape(GLYPH_A_BUTTON L" Select     " GLYPH_B_BUTTON L" Cancel     " GLYPH_START_BUTTON L" Done");
        }
        else
        {
            swprintf_s(texto, 256, L"%d of %d", g_iJogo + 1, total);
            if (soltos) TituloDaTela(L"", texto);
            else { Larga(g_atual->nome, titulo, 256); TituloDaTela(titulo, texto); }
            // Sem "Add games" numa composta: o jogo está ali por causa de uma origem.
            Rodape(soltos ? GLYPH_A_BUTTON L" Play     " GLYPH_Y_BUTTON L" Details     " GLYPH_B_BUTTON L" Back     "
                            GLYPH_X_BUTTON L" Add to collections"
                 : colecoes::Composta(g_atual)
                          ? GLYPH_A_BUTTON L" Play     " GLYPH_Y_BUTTON L" Details     " GLYPH_B_BUTTON L" Back     "
                            GLYPH_START_BUTTON L" Options"
                          : GLYPH_A_BUTTON L" Play     " GLYPH_Y_BUTTON L" Details     " GLYPH_B_BUTTON L" Back     "
                            GLYPH_X_BUTTON L" Add games     " GLYPH_START_BUTTON L" Options");
        }
        g_fonte.End();
    }

    // ---- a faixa de detalhes ---------------------------------------------------------
    // Sobe de baixo até y 300, com a capa saindo da borda e o resto num bloco centrado:
    // capa (150) + 36 + meio (470) + 40 + ficha (300). Co-op só do Co-Optimus, e só o
    // local: sem ele, nada. O resto cai no que a FreeStyle guarda quando não há ficha.
    bool Valido(const std::string &s)
    {
        return !s.empty() && _stricmp(s.c_str(), "Not Available") != 0 &&
               _stricmp(s.c_str(), "N/A") != 0 && _stricmp(s.c_str(), "Unknown") != 0;
    }

    void Horas(float h, WCHAR *saida, int cap)
    {
        if (h < 1.0f) { swprintf_s(saida, cap, L"%d min", (int)(h * 60.0f + 0.5f)); return; }
        const float m = floorf(h * 2.0f + 0.5f) / 2.0f;
        if (m == floorf(m)) swprintf_s(saida, cap, L"%d h", (int)m);
        else                swprintf_s(saida, cap, L"%.1f h", m);
    }

    const WCHAR *CorDoMetacritic(int n, D3DCOLOR *fundo, D3DCOLOR *letra)
    {
        *fundo = (n >= 75) ? D3DCOLOR_XRGB(102, 204, 51) : (n >= 50) ? D3DCOLOR_XRGB(255, 204, 51) : D3DCOLOR_XRGB(255, 0, 0);
        *letra = (n < 50) ? M_TEXTO : D3DCOLOR_XRGB(20, 20, 20);
        return NULL;
    }

    void FaixaDetalhes()
    {
        const biblioteca::Jogo *g = g_detalheJogo;
        if (g == NULL) { g_detalhe = false; return; }

        // k: 0 = no lugar, 1 = escondida embaixo. Sobe com constante de 70 ms (95% do
        // caminho em ~210 ms) e desce igual ao fechar.
        const float t = (float)(GetTickCount() - g_faixaT0);
        float k = expf(-t / TAU_FAIXA);
        if (g_faixaFechando) k = 1.0f - k;
        if (t >= 5.0f * TAU_FAIXA)
        {
            if (g_faixaFechando) { g_detalhe = false; g_faixaFechando = false; g_detalheJogo = NULL; return; }
            k = 0.0f;
        }

        const fichas::Ficha *f = fichas::Por(g->titleId);
        const float FY = 300.0f, SOBE = 720.0f - (FY - 60.0f), off = floorf(SOBE * k + 0.5f);
        const float CL = 150.0f, MW = 470.0f, RWf = 300.0f, LARG = CL + 36.0f + MW + 40.0f + RWf;
        const float CX = floorf((1280.0f - LARG) / 2.0f + 0.5f), CY = FY - 60.0f + off;
        const float CA = floorf(CL * CAPA_A / CAPA_L + 0.5f);
        const float NX = CX + CL + 36.0f, NW = LARG - CL - 36.0f, MX = NX, RX = MX + MW + 40.0f;
        const float y0 = FY + off;

        // os dados, com a FreeStyle de reserva
        WCHAR genero[96] = L"", dev[160] = L"", pub[160] = L"", ano[16] = L"";
        std::string sGen = (f && !f->genero.empty()) ? f->genero : (Valido(g->genero) ? g->genero : "");
        std::string sDev = (f && !f->dev.empty()) ? f->dev : (Valido(g->desenvolvedora) ? g->desenvolvedora : "");
        std::string sPub = (f && !f->pub.empty()) ? f->pub : (Valido(g->publicadora) ? g->publicadora : "");
        Larga(sGen, genero, 96); ParaMinusculas(genero);
        Larga(sDev, dev, 160); Larga(sPub, pub, 160);
        int nAno = f ? f->ano : 0;
        if (nAno == 0 && g->lancamento.size() >= 4 && isdigit((unsigned char)g->lancamento[0]))
            nAno = atoi(g->lancamento.substr(0, 4).c_str());
        if (nAno > 0) swprintf_s(ano, 16, L"%d", nAno);
        const int metacritic = f ? f->metacritic : 0;
        float notaJog = f ? f->notaJogadores : 0.0f;
        if (metacritic == 0 && notaJog == 0.0f && Valido(g->nota)) notaJog = (float)atof(g->nota.c_str());
        const int coop = f ? f->coop : 0;

        // a coluna da direita tem altura fixa por campo (uma linha cada), então a caixa
        // do Metacritic tem lugar conhecido antes do texto
        int nFicha = 0;
        if (dev[0]) nFicha++;
        if (pub[0]) nFicha++;
        const float yNota = y0 + 100.0f + 56.0f * nFicha + 20.0f;

        // ---- formas
        Ret(0.0f, 0.0f, 1280.0f, 720.0f, ComAlfa(D3DCOLOR_XRGB(8, 8, 8), 0.6f * (1.0f - k)));
        Ret(0.0f, y0 - 6.0f, 1280.0f, 6.0f, ComAlfa(0, 0.25f));
        Gradiente(0.0f, y0, 1280.0f, 720.0f + off, D3DCOLOR_XRGB(231, 234, 233), D3DCOLOR_XRGB(206, 210, 211), false);
        Sombra(CX - 6.0f, CY - 6.0f, CL + 12.0f, CA + 12.0f, 1.0f);
        Ret(CX - 6.0f, CY - 6.0f, CL + 12.0f, CA + 12.0f, M_CARTAO);
        {
            D3DRECT r;
            r.x1 = (LONG)CX; r.y1 = (LONG)CY; r.x2 = (LONG)(CX + CL); r.y2 = (LONG)(CY + CA);
            bool inteira = true;
            D3DTexture *capa = NoCache(g->id, true, &inteira);
            if (capa != NULL)
            {
                const float u0 = inteira ? 0.0f : FRENTE_U0;
                ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch(
                    r, XMFLOAT2(u0, 0.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT2(u0, 1.0f), capa);
            }
            else Preencher(r, M_SEM_CAPA);
        }
        D3DCOLOR mcFundo = 0, mcLetra = 0;
        if (metacritic > 0)
        {
            CorDoMetacritic(metacritic, &mcFundo, &mcLetra);
            Ret(RX, yNota, floorf(44.0f * 0.82f + 0.5f), floorf(32.0f * 0.82f + 0.5f), mcFundo);
        }
        Gradiente(0.0f, 676.0f + off, 1280.0f, 720.0f + off, D3DCOLOR_XRGB(146, 150, 152), D3DCOLOR_XRGB(130, 136, 139), false);

        // ---- texto
        WCHAR linhas[12][LIN_CAP], buf[1024];
        g_fonte.Begin();
        float y = y0 + 18.0f;
        if (genero[0]) { Cortar(genero, NW, 0.8f, linhas[0], LIN_CAP); Texto(NX, y, M_PAINEL_ROTULO, 0.8f, linhas[0]); y += 26.0f; }
        Larga(g->nome, buf, 1024);
        for (WCHAR *c = buf; *c; c++) if (*c == 0x2122 || *c == 0x00AE) *c = L' ';
        Cortar(buf, NW, 1.35f, linhas[0], LIN_CAP);
        Texto(NX, y, M_PAINEL_TEXTO, 1.35f, linhas[0]);

        // sob a capa, os números
        float ny = CY + CA + 24.0f;
        if (coop > 0)
        {
            Texto(CX, ny, M_PAINEL_ROTULO, 0.7f, L"COUCH CO-OP");
            if (coop > 1) swprintf_s(buf, 1024, L"up to %d players", coop); else wcscpy(buf, L"1 player");
            const int n = Quebrar(buf, CL + 10.0f, 2, 0.9f, linhas);
            for (int i = 0; i < n; i++) Texto(CX, ny + 20.0f + i * 26.0f, M_PAINEL_TEXTO, 0.9f, linhas[i]);
            ny += 74.0f;
        }
        if (ano[0])
        {
            Texto(CX, ny, M_PAINEL_ROTULO, 0.7f, L"RELEASED");
            Texto(CX, ny + 20.0f, M_PAINEL_TEXTO, 0.9f, ano);
        }

        // no meio, o Co-Optimus e o sobre
        float my = y0 + 100.0f;
        if (coop > 0 && f && (!f->coopExtras.empty() || !f->coopTexto.empty()))
        {
            Texto(MX, my, M_PAINEL_ROTULO, 0.7f, L"ON CO-OPTIMUS");
            my += 20.0f;
            if (!f->coopExtras.empty())
            {
                Larga(f->coopExtras, buf, 1024);
                const int n = Quebrar(buf, MW, 2, 0.76f, linhas);
                for (int i = 0; i < n; i++) { Texto(MX, my, M_PAINEL_TEXTO, 0.76f, linhas[i]); my += 22.0f; }
            }
            if (!f->coopTexto.empty())
            {
                Larga(f->coopTexto, buf, 1024);
                const int n = Quebrar(buf, MW, 2, 0.78f, linhas);
                for (int i = 0; i < n; i++) { Texto(MX, my, M_PAINEL_TEXTO, 0.78f, linhas[i]); my += 23.0f; }
            }
            my += 12.0f;
        }
        if (f && !f->descricao.empty())
        {
            const int cabe = (int)floorf((662.0f + off - my - 20.0f) / 23.0f);
            if (cabe >= 2)
            {
                Texto(MX, my, M_PAINEL_ROTULO, 0.7f, L"ABOUT");
                my += 20.0f;
                Larga(f->descricao, buf, 1024);
                const int n = Quebrar(buf, MW, cabe > 12 ? 12 : cabe, 0.78f, linhas);
                for (int i = 0; i < n; i++) { Texto(MX, my, M_PAINEL_TEXTO, 0.78f, linhas[i]); my += 23.0f; }
            }
        }

        // à direita, a ficha
        float ry = y0 + 100.0f;
        if (dev[0])
        {
            Texto(RX, ry, M_PAINEL_ROTULO, 0.7f, L"DEVELOPER");
            Cortar(dev, RWf, 0.82f, linhas[0], LIN_CAP); Texto(RX, ry + 20.0f, M_PAINEL_TEXTO, 0.82f, linhas[0]);
            ry += 56.0f;
        }
        if (pub[0])
        {
            Texto(RX, ry, M_PAINEL_ROTULO, 0.7f, L"PUBLISHER");
            Cortar(pub, RWf, 0.82f, linhas[0], LIN_CAP); Texto(RX, ry + 20.0f, M_PAINEL_TEXTO, 0.82f, linhas[0]);
            ry += 56.0f;
        }
        if (metacritic > 0)
        {
            Texto(RX, ry, M_PAINEL_ROTULO, 0.7f, L"METACRITIC");
            swprintf_s(buf, 1024, L"%d", metacritic);
            Texto(RX + 18.0f, yNota + 2.0f, mcLetra, 0.82f, buf, ATGFONT_CENTER_X);
            ry = yNota + 28.0f + 12.0f;
        }
        else if (notaJog > 0.0f)
        {
            Texto(RX, ry, M_PAINEL_ROTULO, 0.7f, L"USER RATING");
            const int cheias = (int)floorf(notaJog + 0.5f);
            float sx = RX;
            for (int i = 0; i < 5; i++)
            {
                Texto(sx, ry + 20.0f, (i < cheias) ? M_VERDE : M_PAINEL_ROTULO, 0.82f, L"\x0145");
                sx += 20.0f;
            }
            swprintf_s(buf, 1024, L"%.1f of 5", notaJog);
            Texto(sx + 6.0f, ry + 20.0f, M_PAINEL_TEXTO, 0.82f, buf);
            ry += 20.0f + 23.0f + 12.0f;
        }
        if (f && (f->horas[0] > 0.0f || f->horas[1] > 0.0f || f->horas[2] > 0.0f))
        {
            static const WCHAR *NOMES[3] = { L"main story", L"main + extras", L"100%" };
            Texto(RX, ry, M_PAINEL_ROTULO, 0.7f, L"HOW LONG TO BEAT");
            float ty = ry + 20.0f;
            for (int i = 0; i < 3; i++)
            {
                if (f->horas[i] <= 0.0f) continue;
                Horas(f->horas[i], buf, 1024);
                Texto(RX, ty, M_PAINEL_TEXTO, 0.82f, NOMES[i]);
                Texto(RX + 120.0f, ty, M_PAINEL_TEXTO, 0.82f, buf);
                ty += 23.0f;
            }
        }
        Texto(CX, 676.0f + off + (44.0f - 27.0f * 0.75f) / 2.0f, M_TEXTO, 0.75f,
              GLYPH_A_BUTTON L" Play     " GLYPH_B_BUTTON L" Back");
        g_fonte.End();
    }

    // ---- o menu: o diálogo "Delete Profile" do Metro ---------------------------------
    // Painel de 548 px centrado, de y 45 a 618, altura FIXA (sobra vazio entre o texto e
    // as opções, como lá); barra de 47 px em degradê com o título em branco; corpo em
    // degradê com o texto escuro no alto; opções encostadas embaixo, linhas de 40 px com
    // separador, a em foco verde (vermelha na destrutiva); véu quase preto atrás.
    void DesenharMenu()
    {
        const float DX = 366.0f, DL = 548.0f, TOPO_D = 45.0f, BARRA = 47.0f, FIM = 618.0f, LINHA = 40.0f;
        WCHAR titulo[256], rotulo[64], texto[256], linhas[4][LIN_CAP];
        Larga(g_menuTitulo, titulo, 256);
        Larga(g_menuRotulo, rotulo, 64);

        const WCHAR *barra = (g_menuDe == MENU_EXPORTAR) ? L"Export to Xbox Vault" : rotulo;
        const bool semNome = (g_menuDe == MENU_EXPORTAR);
        const WCHAR *frase =
            (g_menuDe == MENU_COLECAO)  ? L"What do you want to do with this collection?" :
            (g_menuDe == MENU_JOGO)     ? L"What do you want to do with this game in this collection?" :
            (g_menuDe == MENU_EXPORTAR) ? L"Saves your collections to vault.txt" :
            (g_menuDe == MENU_TIPO)     ? L"What kind of collection will it be?" :
                                          L"Delete this collection? The games stay on the console; only the collection goes away.";

        Ret(0.0f, 0.0f, 1280.0f, 720.0f, D3DCOLOR_ARGB(230, 8, 8, 8));
        Gradiente(DX, TOPO_D, DX + DL, TOPO_D + BARRA, D3DCOLOR_XRGB(146, 150, 152), D3DCOLOR_XRGB(130, 136, 139), false);
        Gradiente(DX, TOPO_D + BARRA, DX + DL, FIM, D3DCOLOR_XRGB(231, 234, 233), D3DCOLOR_XRGB(206, 210, 211), false);
        float tx = DX + 14.0f;
        if (g_menuDe == MENU_APAGAR)
        {
            Disco(DX + 22.0f, TOPO_D + BARRA / 2.0f, 11.0f, D3DCOLOR_XRGB(249, 190, 55));   // o "!" amarelo
            tx = DX + 42.0f;
        }
        for (int i = 0; i < g_menuQtd; i++)
        {
            const float ly = FIM - (g_menuQtd - i) * LINHA;
            Ret(DX, ly, DL, LINHA, D3DCOLOR_XRGB(201, 207, 208));
            Ret(DX, ly, DL, 1.0f, D3DCOLOR_XRGB(186, 192, 193));
            if (i == g_menuFoco) Ret(DX, ly, DL, LINHA, (i == g_menuPerigo) ? M_VERMELHO : M_VERDE);
        }

        g_fonte.Begin();
        if (g_menuDe == MENU_APAGAR)
            Texto(DX + 22.0f, TOPO_D + BARRA / 2.0f - 11.0f, D3DCOLOR_XRGB(60, 50, 20), 0.85f, L"!", ATGFONT_CENTER_X);
        Cortar(barra, DL - (tx - DX) - 14.0f, 0.95f, linhas[0], LIN_CAP);
        Texto(tx, TOPO_D + (BARRA - 27.0f * 0.95f) / 2.0f, M_TEXTO, 0.95f, linhas[0]);

        float ty = TOPO_D + BARRA + 16.0f;
        if (!semNome)
        {
            const int n = Quebrar(titulo, DL - 32.0f, 2, 1.0f, linhas);
            for (int i = 0; i < n; i++) { Texto(DX + 16.0f, ty, M_PAINEL_TEXTO, 1.0f, linhas[i]); ty += 28.0f; }
            ty += 12.0f;
        }
        {
            const int n = Quebrar(frase, DL - 32.0f, 4, 0.9f, linhas);
            for (int i = 0; i < n; i++) { Texto(DX + 16.0f, ty, M_PAINEL_TEXTO, 0.9f, linhas[i]); ty += 25.0f; }
        }
        for (int i = 0; i < g_menuQtd; i++)
        {
            const float ly = FIM - (g_menuQtd - i) * LINHA;
            Larga(g_menuItens[i], texto, 256);
            Cortar(texto, DL - 32.0f, 0.95f, linhas[0], LIN_CAP);
            Texto(DX + 16.0f, ly + (LINHA - 27.0f * 0.95f) / 2.0f,
                  (i == g_menuFoco) ? M_TEXTO : M_PAINEL_TEXTO, 0.95f, linhas[0]);
        }
        Texto(DX + 4.0f, FIM + 10.0f, M_TEXTO, 0.8f, GLYPH_A_BUTTON L" Select     " GLYPH_B_BUTTON L" Close");
        g_fonte.End();
    }

    // Usa a notificacao do SISTEMA, o balao do canto, em vez de uma caixa desenhada por
    // nos no meio da tela. E o mesmo caminho que o hiddriver usa.
    void Avisar(const char *texto)
    {
        som::Tocar(som::SOM_ERRO);
        diario::Escrever("aviso: %s", texto);

        // ESTATICO, nao da pilha: XNotifyQueueUI ENFILEIRA, e o balao e desenhado
        // depois. Se o xam nao copiar a string, a pilha ja morreu quando ele ler. Todos
        // os usos conhecidos passam global ou literal, nunca pilha.
        static WCHAR largo[192];
        Larga(texto, largo, 192);
        XNotifyQueueUI(XNOTIFY_GENERIC, XUSER_INDEX_ANY, XNOTIFY_PRIORIDADE_ALTA, largo, NULL);
    }

    // ---- foto (roteiro de controle) ---------------------------------------------
    // O quadro inteiro, como está no render target, resolvido numa textura antes do
    // Present. O Resolve grava em blocos (tiling) MESMO numa textura D3DFMT_LIN_: a
    // primeira rodada saiu em listras, com as cores certas. Então a textura é a comum e o
    // tiling se desfaz com o XGUntileSurface, como o BT3_SHOT do bt3xbx. Só existe com roteiro: a textura de 3,5 MB
    // nasce no primeiro pedido. Grava BMP de 24 bits, de baixo para cima, num WriteFile
    // só (no bt3xbx, uma escrita por linha custava 260 ms por captura). O PowerPC é
    // big-endian: os campos do cabeçalho vão byte a byte, em little-endian.
    D3DTexture *g_fotoTex = NULL;

    void Poe16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
    void Poe32(unsigned char *p, unsigned v) { Poe16(p, v & 0xFFFF); Poe16(p + 2, v >> 16); }

    bool TirarFoto(const char *nome)
    {
        const int W = 1280, H = 720;
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        if (g_fotoTex == NULL &&
            FAILED(d->CreateTexture(W, H, 1, 0, D3DFMT_X8R8G8B8, 0, &g_fotoTex, NULL)))
        {
            diario::Escrever("foto: sem textura linear");
            g_fotoTex = NULL;
            return false;
        }
        d->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_fotoTex, NULL, 0, 0, NULL, 0.0f, 0, NULL);
        d->BlockUntilIdle();

        static DWORD *linear = NULL;
        if (linear == NULL) linear = (DWORD *)malloc(W * H * 4);
        D3DLOCKED_RECT r;
        if (linear == NULL || FAILED(g_fotoTex->LockRect(0, &r, NULL, D3DLOCK_READONLY)))
        {
            diario::Escrever("foto: sem memoria ou LockRect falhou");
            return false;
        }
        XGUntileSurface(linear, W * 4, NULL, r.pBits, W, H, NULL, 4);
        g_fotoTex->UnlockRect(0);

        const unsigned tamLinha = W * 3, tamDados = tamLinha * H, tam = 54 + tamDados;
        static unsigned char *bmp = NULL;
        if (bmp == NULL) bmp = (unsigned char *)malloc(tam);
        if (bmp == NULL)
        {
            diario::Escrever("foto: sem memoria");
            return false;
        }
        ZeroMemory(bmp, 54);
        bmp[0] = 'B'; bmp[1] = 'M';
        Poe32(bmp + 2, tam); Poe32(bmp + 10, 54); Poe32(bmp + 14, 40);
        Poe32(bmp + 18, W); Poe32(bmp + 22, H); Poe16(bmp + 26, 1); Poe16(bmp + 28, 24);
        Poe32(bmp + 34, tamDados); Poe32(bmp + 38, 2835); Poe32(bmp + 42, 2835);

        unsigned fnv = 2166136261u;
        for (int y = 0; y < H; y++)
        {
            const DWORD *src = linear + (H - 1 - y) * W;
            unsigned char *o = bmp + 54 + y * tamLinha;
            for (int x = 0; x < W; x++, o += 3)
            {
                DWORD c = src[x];                     // X8R8G8B8 como u32 nativo
                o[0] = (unsigned char)c;              // B
                o[1] = (unsigned char)(c >> 8);       // G
                o[2] = (unsigned char)(c >> 16);      // R
                fnv = ((fnv ^ o[0]) * 16777619u ^ o[1]) * 16777619u;
            }
        }

        CreateDirectory("game:\\fotos", NULL);
        char caminho[96];
        _snprintf(caminho, sizeof(caminho), "game:\\fotos\\%s.bmp", nome);
        caminho[sizeof(caminho) - 1] = '\0';
        HANDLE h = CreateFile(caminho, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
        {
            diario::Escrever("foto: nao abriu %s", caminho);
            return false;
        }
        DWORD n = 0;
        BOOL ok = WriteFile(h, bmp, tam, &n, NULL) && n == tam;
        CloseHandle(h);
        diario::Escrever("foto: %s %s (fnv %08x)", caminho, ok ? "gravada" : "INCOMPLETA", fnv);
        return ok != FALSE;
    }

    // ---- o esmaecimento cruzado (troca de tela e menu) ----------------------------
    // Abrir uma página ou o menu é um esmaecimento cruzado de ~200 ms, como no Metro.
    // O quadro anterior fica numa textura, resolvido no fim de cada quadro (o mesmo
    // Resolve da foto do roteiro), e é desenhado por cima do novo apagando. Durante o
    // esmaecimento ele não é regravado: segura o quadro de antes da troca. Coleções e
    // Unsorted são troca de ABA (deslizam), e a faixa de detalhes tem animação própria.
    D3DTexture *g_antesTex = NULL;
    bool  g_temAntes = false, g_fadeVivo = false, g_ultimoMenu = false;
    DWORD g_fadeT0 = 0;
    int   g_ultimaTela = -1;
    const int TELA_DE_DETALHES = 100;

    void Desenhar()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(80, 78, 84), 1.0f, 0);

        const int telaAgora = g_detalhe ? TELA_DE_DETALHES : (int)g_tela;
        const bool trocaDeHub = (telaAgora == TELA_SOLTOS && g_ultimaTela == TELA_COLECOES) ||
                                (telaAgora == TELA_COLECOES && g_ultimaTela == TELA_SOLTOS);
        const bool semFade = trocaDeHub || telaAgora == TELA_DE_DETALHES || g_ultimaTela == TELA_DE_DETALHES;
        const bool mudou = g_ultimaTela != -1 &&
                           ((telaAgora != g_ultimaTela && !semFade) || g_menuAberto != g_ultimoMenu);
        if (mudou && g_temAntes) { g_fadeVivo = true; g_fadeT0 = GetTickCount(); }
        g_ultimaTela = telaAgora; g_ultimoMenu = g_menuAberto;

        FundoMetro();

        // TELA_ARQUIVAR é uma grade de coleções; TELA_SOLTOS é uma grade de capas.
        if (g_tela == TELA_COLECOES || g_tela == TELA_ORIGENS ||
            g_tela == TELA_ARQUIVAR)                           TelaColecoes();
        else                                                   TelaJogos();

        if (g_detalhe)
            FaixaDetalhes();
        if (g_menuAberto)
            DesenharMenu();

        if (g_fadeVivo)
        {
            const float t = (float)(GetTickCount() - g_fadeT0);
            if (t >= FADE_MS) g_fadeVivo = false;
            else if (g_antesTex != NULL)
            {
                XMFLOAT2 pos[4], uv[4];
                pos[0] = XMFLOAT2(0.0f, 0.0f);     pos[1] = XMFLOAT2(1280.0f, 0.0f);
                pos[2] = XMFLOAT2(1280.0f, 720.0f); pos[3] = XMFLOAT2(0.0f, 720.0f);
                uv[0] = XMFLOAT2(0.0f, 0.0f); uv[1] = XMFLOAT2(1.0f, 0.0f);
                uv[2] = XMFLOAT2(1.0f, 1.0f); uv[3] = XMFLOAT2(0.0f, 1.0f);
                Mistura(true);
                QuadTex(pos, uv, g_antesTex, ComAlfa(0x00FFFFFF, 1.0f - t / FADE_MS), true);
                Mistura(false);
                d->SetTexture(0, NULL);
            }
        }

        // A foto antes do Present: o Resolve lê o render target, que o Present troca.
        if (const char *foto = roteiro::FotoPedida())
            roteiro::FotoTirada(TirarFoto(foto));

        if (!g_fadeVivo)
        {
            if (g_antesTex == NULL &&
                FAILED(d->CreateTexture(1280, 720, 1, 0, D3DFMT_X8R8G8B8, 0, &g_antesTex, NULL)))
            {
                g_antesTex = NULL;
                static bool avisado = false;
                if (!avisado) { avisado = true; diario::Escrever("AVISO: sem textura do quadro anterior; troca de tela sem esmaecer"); }
            }
            if (g_antesTex != NULL)
            {
                d->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_antesTex, NULL, 0, 0, NULL, 0.0f, 0, NULL);
                g_temAntes = true;
            }
        }

        d->Present(NULL, NULL, NULL, NULL);
    }

    // ---- navegação -------------------------------------------------------------
    void SeguirFocoColecao()
    {
        int linha = g_iCol / COL_POR_LINHA;
        if (linha < g_primeiraLinhaCol) g_primeiraLinhaCol = linha;
        else if (linha >= g_primeiraLinhaCol + COL_LINHAS)
            g_primeiraLinhaCol = linha - COL_LINHAS + 1;
    }

    void SeguirFoco()
    {
        int linha = g_iJogo / COLUNAS;
        if (linha < g_primeiraLinha) g_primeiraLinha = linha;
        else if (linha >= g_primeiraLinha + LINHAS) g_primeiraLinha = linha - LINHAS + 1;
    }

    void MoverJogo(int delta)
    {
        int total = (int)ListaAtual().size();
        if (total == 0) return;

        int novo = g_iJogo + delta;

        // Descer numa última linha incompleta gruda no último jogo. Sem isto, com 12
        // jogos e o foco no 8º, Baixo não faz nada e os dois últimos só se alcançam
        // andando para o lado.
        if (delta == COLUNAS && novo >= total && g_iJogo < total - 1)
            novo = total - 1;

        if (novo < 0 || novo >= total) return;
        g_iJogo = novo;
        som::Tocar(som::SOM_FOCO);
        SeguirFoco();
    }

    // O foco é corrigido AQUI, uma vez por quadro, e não no desenho: a tela de
    // coleções não é desenhada enquanto estamos nos jogos, então um clamp que só
    // rodava ao desenhar deixava g_iCol fora de faixa para quem lesse antes.
    void ClampFoco()
    {
        // As abas primeiro: a lista de coleções depende da aba ativa, e apagar a
        // última coleção de uma categoria apaga a aba em que se está.
        AtualizarAbas();
        bool abaVive = false;
        for (size_t i = 0; i < g_abas.size(); i++)
            if (g_abas[i] == g_aba) abaVive = true;
        if (!abaVive)
        {
            g_aba = biblioteca::ABA_AUTOMATICA;
            // Arquivar o último solto apaga a aba em que se está. Sem isto o app
            // ficaria desenhando a grade de capas de uma lista vazia.
            if (g_tela == TELA_SOLTOS) g_tela = TELA_COLECOES;
        }

        int nCol = (int)ListaDeColecoes().size();
        if (g_iCol >= nCol) g_iCol = nCol - 1;
        if (g_iCol < 0)     g_iCol = 0;
        SeguirFocoColecao();

        // Arquivando, a grade à vista é a de COLEÇÕES: o g_iJogo que está guardado é
        // a posição na lista dos soltos, para onde se volta. Clampá-lo contra a lista
        // de jogos (que ali é vazia, porque g_atual é nulo) zerava o foco por baixo, e
        // ao concluir a tela voltava sempre para o primeiro item.
        if (g_tela != TELA_ARQUIVAR)
        {
            int nJogos = (int)ListaAtual().size();
            if (g_iJogo >= nJogos) g_iJogo = nJogos - 1;
            if (g_iJogo < 0)       g_iJogo = 0;
            SeguirFoco();
        }
    }

    void SaltoLetra(int dir)
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty()) return;

        char cur = Inicial(L[g_iJogo]->nome);
        int i = g_iJogo;
        while (i + dir >= 0 && i + dir < (int)L.size() && Inicial(L[i + dir]->nome) == cur)
            i += dir;

        int alvo = i + dir;
        if (alvo < 0) alvo = 0;
        if (alvo >= (int)L.size()) alvo = (int)L.size() - 1;
        if (alvo == g_iJogo) return;    // já estava na ponta: nada andou, nada soa

        g_iJogo = alvo;
        som::Tocar(som::SOM_FOCO);
        SeguirFoco();
    }

    void MoverColecao(int delta)
    {
        int total = (int)ListaDeColecoes().size();
        if (total == 0) return;

        int novo = g_iCol + delta;
        if (delta == COL_POR_LINHA && novo >= total && g_iCol < total - 1)
            novo = total - 1;

        if (novo >= 0 && novo < total)
        {
            g_iCol = novo;
            som::Tocar(som::SOM_FOCO);
            SeguirFocoColecao();
        }
    }

    // LB/RB na tela de coleções. É o ÚNICO lugar onde eles estão livres: na grade de
    // jogos são o salto por letra, e escolhendo origens a aba não vale.
    void TrocarAba(int dir)
    {
        if (g_abas.size() < 2) return;

        const EstadoTela antes = FotoDoEstado();
        int i = IndiceDaAba() + dir;
        // SEM dar a volta. O ombro repete sozinho quando segurado, e com a volta
        // segurar passa reto pelo Todos -- perde-se a referência de onde a lista
        // inteira está, que é justamente a âncora da tela.
        if (i < 0 || i >= (int)g_abas.size()) return;

        g_aba = g_abas[i];
        g_iCol = 0; g_primeiraLinhaCol = 0;

        // O Soltos é uma TELA alcançada por uma aba: entrar nele troca a grade de
        // coleções pela de capas, e sair dele devolve. O foco da grade de jogos zera
        // junto, senão o item em foco seria o de uma lista que já não existe.
        if (g_aba == ABA_SOLTOS)
        {
            g_tela = TELA_SOLTOS;
            g_iJogo = 0; g_primeiraLinha = 0;
        }
        else if (g_tela == TELA_SOLTOS)
        {
            g_tela = TELA_COLECOES;
        }

        som::Tocar(som::SOM_FOCO);
        ComecarTrocaDeHub(antes, dir);

        // Troca a tela inteira de conteúdo: o que estava a caminho já não serve,
        // como ao abrir uma coleção.
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    // Arquivar: escolher em QUE coleções este jogo entra. Uma tela, não um modal --
    // é o mesmo gesto de escolher origens, com o mesmo vocabulário (anel na marcada,
    // véu na outra, Start conclui), e um modal de lista não mostraria a colagem nem
    // a contagem de cada coleção.
    void AbrirArquivar()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty() || g_iJogo >= (int)L.size()) return;

        // TitleId zero não pode entrar em coleção: gravaria 00000000 e a releitura
        // descartaria, fazendo o item sumir no próximo boot sem aviso. Mesma barra do
        // escolhedor de jogos.
        if (L[g_iJogo]->titleId == 0)
        {
            Avisar("This game has no TitleId and can't go in a collection");
            return;
        }

        som::Tocar(som::SOM_MENU);
        g_arquivando = L[g_iJogo];
        g_escolhidas.clear();
        g_tela = TELA_ARQUIVAR;
        g_iCol = 0; g_primeiraLinhaCol = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    void FecharArquivar(bool gravar)
    {
        if (gravar && g_arquivando != NULL && !g_escolhidas.empty())
        {
            const unsigned int titleId = g_arquivando->titleId;
            for (size_t i = 0; i < g_escolhidas.size(); i++)
            {
                colecoes::Colecao *c = colecoes::PorId(g_escolhidas[i]);
                // Alternar e não um push: se o jogo já estivesse lá, push duplicaria
                // o TitleId. Aqui ele nunca está -- é um solto --, mas quem garante
                // isso é a lista, não esta função.
                if (c != NULL && !colecoes::Tem(c, titleId))
                    colecoes::Alternar(c, titleId);
            }
            colecoes::Gravar();
            diario::Escrever("arquivado '%s' em %d colecoes",
                             g_arquivando->nome.c_str(), (int)g_escolhidas.size());
        }

        g_arquivando = NULL;
        g_escolhidas.clear();
        g_tela = TELA_SOLTOS;
        g_iCol = 0; g_primeiraLinhaCol = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();

        // O jogo arquivado SAIU da lista; o foco segue o que ficou no lugar dele. Se
        // era o último, o ClampFoco devolve para Todos no quadro seguinte.
        int n = (int)ListaAtual().size();
        if (g_iJogo >= n) g_iJogo = n - 1;
        if (g_iJogo < 0)  g_iJogo = 0;
        SeguirFoco();
    }

    void AbrirColecao()
    {
        // A lista da TELA, não a completa: com uma aba ativa as duas divergem, e
        // g_iCol é índice da que está à vista.
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();
        if (L.empty() || g_iCol >= (int)L.size()) return;

        som::Tocar(som::SOM_CONFIRMA);
        g_atual = L[g_iCol];
        g_tela = TELA_JOGOS;
        g_iJogo = 0; g_primeiraLinha = 0;
        carregador::DescartarPendentes();   // o que era da tela anterior já não serve
        g_emVoo.clear();
    }

    // Ao adicionar trabalhamos numa CÓPIA. É o que dá sentido ao B: sem rascunho,
    // "cancelar" não teria o que desfazer, porque cada marcação já estaria gravada.
    void AbrirAdicionar()
    {
        som::Tocar(som::SOM_CONFIRMA);
        g_selecao = g_atual->ids;
        g_tela = TELA_ADICIONAR;
        g_iJogo = 0; g_primeiraLinha = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    void FecharAdicionar(bool gravar)
    {
        if (gravar)
        {
            g_atual->ids = g_selecao;
            colecoes::Gravar();

            // O escolhedor mostra a biblioteca INTEIRA de propósito, então dá para
            // marcar jogo que a aba corrente não mostra -- e aí o Concluir voltaria
            // para uma grade sem nenhum deles. Marcou algo de fora da aba, a aba sai
            // da frente. É a mesma regra do criar e do renomear: quem acabou de fazer
            // uma coisa tem de VÊ-LA.
            if (g_aba != biblioteca::ABA_AUTOMATICA)
                for (size_t i = 0; i < g_jogos.size(); i++)
                    if (g_jogos[i].aba != g_aba &&
                        SelecionadoNoRascunho(g_jogos[i].titleId))
                    {
                        g_aba = biblioteca::ABA_AUTOMATICA;
                        break;
                    }
        }
        g_selecao.clear();
        g_tela = TELA_JOGOS;
        g_iJogo = 0; g_primeiraLinha = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    // Estas duas só DISPARAM o teclado. Quem aplica o resultado é AtenderTeclado,
    // alguns quadros depois -- o laço não pode parar enquanto a Guide está na tela.
    void NovaColecao()
    {
        if (teclado::Abrir("New collection", "What's it called?", ""))
        {
            som::Tocar(som::SOM_CONFIRMA);
            g_pedido = PEDIDO_NOVA;
        }
    }

    void Renomear()
    {
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();   // a da tela
        if (L.empty() || g_iCol >= (int)L.size()) return;

        // Guarda o PONTEIRO, não o índice: até o teclado voltar, g_iCol pode não
        // apontar mais para esta coleção.
        g_renomeando = L[g_iCol];
        if (teclado::Abrir("Rename", "New name", g_renomeando->nome.c_str()))
            g_pedido = PEDIDO_RENOMEAR;
        else
            g_renomeando = NULL;
    }

    void CopiarTitulo(const std::string &origem)
    {
        strncpy(g_menuTitulo, origem.c_str(), sizeof(g_menuTitulo) - 1);
        g_menuTitulo[sizeof(g_menuTitulo) - 1] = '\0';
    }

    void AtenderTeclado()
    {
        bool confirmou = false;
        std::string nome;
        if (!teclado::Terminou(&confirmou, nome))
            return;

        Pedido pedido = g_pedido;
        colecoes::Colecao *alvo = g_renomeando;
        g_pedido = PEDIDO_NENHUM;
        g_renomeando = NULL;

        if (!confirmou)
            return;

        if (pedido == PEDIDO_NOVA)
        {
            // O nome espera aqui enquanto se escolhe o tipo. Nada é criado ainda: uma
            // união sem origem seria apagada na próxima leitura.
            _snprintf(g_nomeNovo, sizeof(g_nomeNovo), "%s", nome.c_str());
            g_nomeNovo[sizeof(g_nomeNovo) - 1] = '\0';

            CopiarTitulo(g_nomeNovo);
            // Nomes curtos, sem a explicação a reboque: quem escolheu Junção ou
            // Interseção cai na tela de origens no quadro seguinte, e é LÁ que o
            // subtítulo diz "em qualquer uma delas" ou "só em todas elas" -- com as
            // coleções à vista e a contagem ao vivo no rodapé, que explica melhor do
            // que qualquer frase no menu.
            g_menuDe = MENU_TIPO;
            g_menuRotulo = "New collection";
            g_menuItens[0] = "Manual collection";
            g_menuItens[1] = "Union";
            g_menuItens[2] = "Intersection";
            g_menuQtd = 3; g_menuFoco = 0; g_menuPerigo = -1; g_menuAberto = true;
        }
        else if (pedido == PEDIDO_RENOMEAR && alvo != NULL)
        {
            // A coleção pode ter sido apagada enquanto o teclado estava aberto? Não
            // hoje (o laço ignora o controle nesse intervalo), mas confirmar é barato.
            std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
            bool vive = false;
            for (size_t i = 0; i < L.size(); i++)
                if (L[i] == alvo) vive = true;
            if (!vive) return;

            alvo->nome = colecoes::Sanear(nome);
            colecoes::Gravar();

            // A lista sai ordenada por nome: renomear muda o lugar. Sem reachar, o
            // foco fica sobre outra coleção.
            FocarColecao(alvo);
        }
    }


    void AbrirMenuColecao()
    {
        // A lista da TELA: g_iCol é índice dela, não da completa.
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();

        // Sem coleção nenhuma o menu não tem mais o que oferecer -- o exportar saiu
        // daqui para o Back. Abrir uma caixa vazia é pior do que não abrir.
        if (L.empty() || g_iCol < 0 || g_iCol >= (int)L.size())
        {
            som::Tocar(som::SOM_VOLTA);
            return;
        }

        som::Tocar(som::SOM_MENU);
        g_menuDe = MENU_COLECAO;
        g_menuQtd = 0;
        g_menuRotulo = "Collection";
        CopiarTitulo(L[g_iCol]->nome);

        g_menuItens[g_menuQtd] = "Rename";
        g_menuAcao [g_menuQtd] = ACAO_RENOMEAR;  g_menuQtd++;

        // Composta ganha a opção de trocar as origens; coleção de jogos não tem o que
        // escolher ali.
        if (colecoes::Composta(L[g_iCol]))
        {
            g_menuItens[g_menuQtd] = "Choose collections";
            g_menuAcao [g_menuQtd] = ACAO_ORIGENS;   g_menuQtd++;
        }

        g_menuPerigo = g_menuQtd;                 // a destrutiva é sempre a última
        g_menuItens[g_menuQtd] = "Delete collection";
        g_menuAcao [g_menuQtd] = ACAO_APAGAR;    g_menuQtd++;

        g_menuFoco = 0; g_menuAberto = true;
    }

    // O exportar NÃO é da coleção: o exportar::ParaOVault recebe g_jogos, então vai a
    // biblioteca inteira e todas as coleções. Dentro de um menu intitulado com o nome
    // de uma coleção, ele dizia o contrário do que faz. Agora mora no Back -- que não
    // era usado em lugar nenhum do app -- e abre ESTE modal em vez de exportar na
    // hora: o vault.txt é reescrito por cima do anterior, e um Back sem querer faria
    // isso calado. O rodapé não tem largura para a frase inteira; aqui tem.
    void AbrirMenuExportar()
    {
        som::Tocar(som::SOM_MENU);
        g_menuDe = MENU_EXPORTAR;
        g_menuRotulo = "Export";
        CopiarTitulo("Library and collections for Xbox Vault");
        g_menuItens[0] = "Export";
        g_menuItens[1] = "Cancel";
        g_menuQtd = 2; g_menuFoco = 0; g_menuPerigo = -1;
        g_menuAberto = true;
    }

    void AbrirMenuJogo()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty()) return;
        // Numa composta o jogo está ali por causa de uma origem; para tirar, tira-se
        // da origem. Sem "Remover" aqui, então o menu não tem o que oferecer.
        if (colecoes::Composta(g_atual))
        {
            som::Tocar(som::SOM_VOLTA);   // sem isto o botão parece quebrado
            return;
        }

        CopiarTitulo(L[g_iJogo]->nome);
        som::Tocar(som::SOM_MENU);
        g_menuDe = MENU_JOGO;
        g_menuRotulo = "Game";

        // A coleção guarda TitleId, então remover tira TODOS os itens que o
        // compartilham -- os dois discos de um multi-disco, as duas cópias de uma
        // instalação duplicada. Dizer isso é mais barato que surpreender.
        int quantos = ItensComTitleId(L[g_iJogo]->titleId);
        if (quantos > 1)
            _snprintf(g_menuItemBuf, sizeof(g_menuItemBuf),
                      "Remove from collection (%d items)", quantos);
        else
            _snprintf(g_menuItemBuf, sizeof(g_menuItemBuf), "Remove from collection");
        g_menuItemBuf[sizeof(g_menuItemBuf) - 1] = '\0';

        g_menuItens[0] = g_menuItemBuf;
        g_menuQtd = 1; g_menuFoco = 0; g_menuPerigo = 0; g_menuAberto = true;
    }

    // Entra na escolha de origens, com o rascunho partindo do que a composta já tem.
    void AbrirOrigens(colecoes::Colecao *composta)
    {
        g_editandoComposta = composta;
        g_origens = composta->origens;
        g_tela = TELA_ORIGENS;
        g_iCol = 0;
        g_primeiraLinhaCol = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    void FecharOrigens(bool gravar)
    {
        colecoes::Colecao *sobreviveu = NULL;

        if (gravar && g_editandoComposta != NULL)
        {
            const char *palavra = colecoes::Palavra(g_editandoComposta->tipo);
            g_editandoComposta->origens = g_origens;

            if (g_origens.empty())
            {
                // Composta sem origem não é coleção vazia, é coleção sem sentido.
                diario::Escrever("%s '%s' concluida sem origem: apagada",
                                 palavra, g_editandoComposta->nome.c_str());
                colecoes::Apagar(g_editandoComposta);
            }
            else
            {
                colecoes::Gravar();
                diario::Escrever("%s '%s' com %d origens", palavra,
                                 g_editandoComposta->nome.c_str(), (int)g_origens.size());
                sobreviveu = g_editandoComposta;
            }
        }
        else if (g_editandoComposta != NULL && g_editandoComposta->origens.empty())
        {
            // Cancelou a criação: a composta nunca chegou a existir de verdade.
            colecoes::Apagar(g_editandoComposta);
        }

        g_editandoComposta = NULL;
        g_origens.clear();
        g_tela = TELA_COLECOES;

        // O g_iCol que sobrou e indice da lista FILTRADA da escolha de origens; na
        // grade completa ele cai em outra coleção. Reacha a composta por PONTEIRO,
        // como o Renomear faz -- dois nomes iguais focariam errado.
        FocarColecao(sobreviveu);

        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    void EscolherNoMenu()
    {
        // Apagar coleção e remover jogo são as duas ações mais consequentes do app.
        // Eram também as duas mais silenciosas.
        som::Tocar(som::SOM_CONFIRMA);
        g_menuAberto = false;

        if (g_menuDe == MENU_EXPORTAR)
        {
            if (g_menuFoco == 0)
            {
                // A lista já tem as ROMs dentro: elas entraram como itens comuns no
                // arranque, então o vault recebe jogo e ROM pela mesma varredura. As
                // coleções o próprio exportar busca.
                std::string erro;
                if (exportar::ParaOVault(g_jogos, g_bancoOk, erro))
                    Avisar("vault.txt saved");
                else
                    Avisar(erro.c_str());
            }
        }
        else if (g_menuDe == MENU_TIPO)
        {
            if (g_menuFoco == 0)
            {
                // Compara por PONTEIRO: dois nomes iguais focariam a coleção errada.
                colecoes::Colecao *nova = colecoes::Criar(g_nomeNovo);
                FocarColecao(nova);   // vazia: isto volta para Todos sozinho
                diario::Escrever("colecao criada: %s", nova->nome.c_str());
            }
            else
            {
                // 1 = junção, 2 = cruzamento, na ordem em que o menu monta.
                const colecoes::Tipo t = (g_menuFoco == 1) ? colecoes::TIPO_UNIAO
                                                           : colecoes::TIPO_INTERSECAO;
                AbrirOrigens(colecoes::CriarComposta(g_nomeNovo, t));
            }
        }
        else if (g_menuDe == MENU_APAGAR)
        {
            // Segundo menu, com "Cancel" em foco. Apagar e a unica acao do app que
            // destroi algo sem volta, e estava a um A de distancia.
            if (g_menuFoco == 1)
            {
                // A lista da TELA. Apagar pela lista completa apagaria OUTRA coleção
                // sempre que uma aba estivesse ativa -- a pior consequência possível
                // dessa divergência, e a razão de ela ter sido caçada uma por uma.
                std::vector<colecoes::Colecao *> L = ListaDeColecoes();
                if (!L.empty() && g_iCol < (int)L.size())
                {
                    diario::Escrever("apagando colecao: %s", L[g_iCol]->nome.c_str());
                    colecoes::Apagar(L[g_iCol]);
                    if (g_iCol > 0) g_iCol--;
                }
            }
        }
        else if (g_menuDe == MENU_COLECAO)
        {
            std::vector<colecoes::Colecao *> Lc = ListaDeColecoes();   // a da tela
            const AcaoCol acao = g_menuAcao[g_menuFoco];

            if (acao == ACAO_RENOMEAR) Renomear();
            else if (acao == ACAO_ORIGENS && !Lc.empty() && g_iCol < (int)Lc.size())
            {
                AbrirOrigens(Lc[g_iCol]);
            }
            // Comparacao EXPLICITA, e nao um "else": apagar e a acao destrutiva, e
            // como fallback ela recebia tudo o que nao casasse acima -- inclusive uma
            // acao nova que alguem acrescentasse ao enum sem mexer nesta cadeia.
            else if (acao == ACAO_APAGAR)
            {
                // Nao apaga aqui: abre a confirmacao.
                std::vector<colecoes::Colecao *> L = ListaDeColecoes();   // a da tela
                if (!L.empty() && g_iCol < (int)L.size())
                {
                    CopiarTitulo(L[g_iCol]->nome);
                    g_menuDe = MENU_APAGAR;
                    g_menuRotulo = "Delete";
                    g_menuItens[0] = "Cancel";
                    g_menuItens[1] = "Delete this collection";
                    g_menuQtd = 2;
                    g_menuFoco = 0;          // o seguro em foco
                    g_menuPerigo = 1;
                    g_menuAberto = true;
                }
            }
        }
        else
        {
            std::vector<const biblioteca::Jogo *> L = ListaAtual();
            if (!L.empty())
            {
                colecoes::Remover(g_atual, L[g_iJogo]->titleId);
                if (g_iJogo > 0) g_iJogo--;
                SeguirFoco();
            }
        }
    }

    // Solta TUDO antes de lançar. Não é economia de memória -- é que a doc do XDK
    // proíbe lançar com I/O de disco pendente, e a thread do carregador está
    // justamente lendo .assets.
    void SoltarTudo()
    {
        som::Parar();
        carregador::Parar();
        carregador::DescartarPendentes();
        g_emVoo.clear();
        g_falhou.clear();

        ATG::g_pd3dDevice->SetTexture(0, NULL);
        ATG::g_pd3dDevice->BlockUntilIdle();
        for (int i = 0; i < CACHE_MAX; i++)
            if (g_cache[i].textura != NULL)
            {
                g_cache[i].textura->Release();
                g_cache[i].textura = NULL;
                g_cache[i].jogoId = -1;
            }
    }

    // Fim de linha: dando certo, o console reinicia no jogo e este app morre. Voltar
    // para cá não faz parte da experiência -- então só há caminho de volta no ERRO.
    void Jogar()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty()) return;

        const biblioteca::Jogo *j = L[g_iJogo];

        // Vai sair cortado: o SoltarTudo logo abaixo destrói a voz em poucos
        // milissegundos. É de propósito -- esperar os 525 ms do som atrasaria o
        // lançamento, e o estalo já é retorno suficiente de que o A pegou.
        som::Tocar(som::SOM_CONFIRMA);
        SoltarTudo();

        std::string erro;
        if (lancador::Lancar(*j, erro))
            return;                     // nunca acontece: sucesso não devolve

        // Voltamos vivos, então falhou. O app continua usável: remonta o carregador E O
        // SOM antes de avisar -- o SoltarTudo destruiu as vozes, e sem remontar o aviso
        // sairia mudo e o app ficaria silencioso para sempre.
        carregador::Iniciar();
        som::Iniciar();
        Avisar(erro.c_str());
    }

    void Confirmar()
    {
        // O som mora em cada ramo, não aqui: com a lista vazia, A não faz nada, e um
        // clique de confirmação sem confirmação nenhuma é ruído.
        if (g_tela == TELA_COLECOES) AbrirColecao();
        else if (g_tela == TELA_ADICIONAR)
        {
            std::vector<const biblioteca::Jogo *> L = ListaAtual();
            if (L.empty()) return;

            unsigned int titleId = L[g_iJogo]->titleId;

            // Barra na ESCRITA, não só na leitura. TitleId zero gravaria 00000000 e a
            // releitura descartaria -- o item sumiria da coleção no próximo boot, sem
            // aviso.
            //
            // Isto virou REDE DE SEGURANÇA: o biblioteca::IdDoApp agora dá id
            // sintético a todo item que chega com zero, então só sobra aqui o caso de
            // item sem caminho, que não dá para semear. Fica porque a perda seria
            // silenciosa e o teste custa nada.
            if (titleId == 0)
            {
                Avisar("This game has no TitleId and can't go in a collection");
                return;
            }

            // Rastro cercando CADA etapa do ato de marcar. O carregamento ja foi
            // inocentado pelo log (120 de 120 texturas prontas e estaveis); o que resta
            // e isto aqui, e cada linha ausente aponta para a etapa seguinte a ela.
            diario::Detalhe("marcar: i=%d id=%d titleId=%08X selecao=%d",
                            g_iJogo, L[g_iJogo]->id, titleId, (int)g_selecao.size());

            som::Tocar(som::SOM_CONFIRMA);

            for (size_t i = 0; i < g_selecao.size(); i++)
            {
                if (g_selecao[i] == titleId)
                {
                    g_selecao.erase(g_selecao.begin() + i);
                    return;
                }
            }
            g_selecao.push_back(titleId);
        }
        else Jogar();
    }

    // A faixa de detalhes (Y na grade de jogos e no Unsorted). O A da grade continua
    // jogando direto: os detalhes são um botão à parte, para não deixar mais lento o
    // caminho de quem só quer jogar.
    void AbrirDetalhes()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty() || g_iJogo >= (int)L.size()) return;
        som::Tocar(som::SOM_MENU);
        g_detalheJogo = L[g_iJogo];
        g_detalhe = true; g_faixaFechando = false; g_faixaT0 = GetTickCount();
    }

    void FecharDetalhes()
    {
        som::Tocar(som::SOM_VOLTA);
        g_faixaFechando = true; g_faixaT0 = GetTickCount();
    }

    // Sem saída pelo B.
    //
    // Houve uma tentativa: B na tela de coleções chamaria XLaunchNewImage(NULL, 0) para
    // voltar ao dashboard, depois da mesma desmontagem do lançamento de jogo. Ela nunca
    // funcionou -- o ramo de TELA_COLECOES no laço de entrada trata A, X e ☰, e nunca
    // chamou Voltar(), então o "B Sair" existia só no rodapé. E o usuário preferiu não
    // ter a opção. Sai-se do app lançando um jogo, ou pela Guide.
    void Voltar()
    {
        if (g_tela == TELA_COLECOES)
            return;

        som::Tocar(som::SOM_VOLTA);
        if (g_tela == TELA_SOLTOS)
        {
            // Volta para o Todos, que é de onde se veio, deslizando como no RB.
            ComecarTrocaDeHub(FotoDoEstado(), 1);
            g_aba = biblioteca::ABA_AUTOMATICA;
            g_tela = TELA_COLECOES;
            g_iCol = 0; g_primeiraLinhaCol = 0;
            carregador::DescartarPendentes();
            g_emVoo.clear();
        }
        else if (g_tela == TELA_ADICIONAR) FecharAdicionar(false);
        else if (g_tela == TELA_JOGOS)
        {
            g_tela = TELA_COLECOES;
            g_atual = NULL;
            carregador::DescartarPendentes();
            g_emVoo.clear();
            g_falhou.clear();   // falha pode ter sido de memória; na volta tenta de novo
        }
    }
}

// Último recurso: roda quando ninguém mais trata a exceção.
//
// Faz três coisas, nessa ordem de importância: registra ONDE morreu (o Iar é o endereço
// da instrução que falhou, e o .map do link resolve para função), cala o motor de áudio,
// e garante o log no disco.
//
// Devolve EXCEPTION_CONTINUE_SEARCH de propósito. A doc do SetUnhandledExceptionFilter
// avisa que devolver EXCEPTION_EXECUTE_HANDLER "usually results in the game console
// freezing" -- e congelar é exatamente o que a gente está tentando evitar.
LONG WINAPI AoMorrer(LPEXCEPTION_POINTERS p)
{
    if (p != NULL && p->ExceptionRecord != NULL && p->ContextRecord != NULL)
    {
        // Para 0xC0000005, ExceptionInformation[0] diz leitura(0) ou escrita(1) e [1]
        // e o ENDERECO acessado. Esse par fecha sozinho casos que de outro jeito
        // exigem desassemblar o binario -- foi o que faltou no crash do DrawText.
        unsigned acesso = 0, onde = 0;
        if (p->ExceptionRecord->NumberParameters >= 2)
        {
            acesso = (unsigned)p->ExceptionRecord->ExceptionInformation[0];
            onde   = (unsigned)p->ExceptionRecord->ExceptionInformation[1];
        }

        diario::Escrever("CRASH code=0x%08X addr=0x%08X Iar=0x%08X Lr=0x%08X %s=0x%08X",
                         (unsigned)p->ExceptionRecord->ExceptionCode,
                         (unsigned)(ULONG_PTR)p->ExceptionRecord->ExceptionAddress,
                         (unsigned)p->ContextRecord->Iar,
                         (unsigned)p->ContextRecord->Lr,
                         acesso ? "escrevendo" : "lendo", onde);
    }
    else
    {
        diario::Escrever("CRASH sem contexto");
    }

    // StopEngine, não Parar(): aqui não se aloca, não se espera e não se desmonta nada.
    som::Calar();
    diario::Fechar();

    return EXCEPTION_CONTINUE_SEARCH;
}

void __cdecl main()
{
    diario::Abrir("game:\\collectionui.log");

    // Antes de tudo: é o que transforma morte silenciosa em endereço no log, e o que
    // cala o áudio para o console não pendurar.
    SetUnhandledExceptionFilter(AoMorrer);
    diario::Escrever("CollectionUI");

    for (int i = 0; i < CACHE_MAX; i++)
    {
        g_cache[i].jogoId = -1; g_cache[i].textura = NULL; g_cache[i].uso = 0;
    }

    // No Xbox 360 não há indireção de COM: Direct3D::CreateDevice é método ESTÁTICO
    // que encaminha para Direct3D_CreateDevice, e o ponteiro do Direct3DCreate9 nunca
    // é dereferenciado.
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    (void)d3d;

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth      = 1280;
    pp.BackBufferHeight     = 720;
    pp.BackBufferFormat     = D3DFMT_X8R8G8B8;
    pp.BackBufferCount      = 1;
    pp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    ::D3DDevice *dispositivo = NULL;
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dispositivo);
    ATG::g_pd3dDevice = (ATG::D3DDevice *)dispositivo;
    if (FAILED(hr))
    {
        diario::Escrever("ERRO: CreateDevice = 0x%08X", hr);
        for (;;) {}
    }
    diario::Escrever("D3D pronto em 1280x720");

    // Registrar a intenção ANTES de cada chamada arriscada: logar só o sucesso faz a
    // falha aparecer como silêncio.
    diario::Escrever("SimpleShaders::Initialize (espera game:\\media\\effects\\simpleshaders.fxobj)");
    ATG::SimpleShaders::Initialize(NULL, NULL);
    diario::Escrever("SimpleShaders ok");

    diario::Escrever("fonte game:\\media\\Arial_16.xpr");
    if (FAILED(g_fonte.Create("game:\\media\\Arial_16.xpr")))
    {
        // Sem fonte não há app: toda a interface é texto sobre capa. Antes isto era só
        // um aviso e o laço seguia -- e o primeiro DrawText morria desreferenciando
        // m_TranslatorTable nulo (AtgFont.cpp:710, sem checagem). Melhor sair aqui,
        // dizendo o que falta, do que dar fatal crash três linhas adiante.
        diario::Escrever("ERRO: a fonte nao carregou. Falta game:\\media\\Arial_16.xpr?");
        diario::Fechar();
        return;
    }

    // Um título só enxerga "game:" por padrão; sem montar, o HD não existe para nós.
    CriarCanto();

    // O texto do Metro não tem a sombra do atlas (só o nome do quadrado de coleção,
    // que liga e desliga em volta do próprio desenho).
    ATG::g_fonteSemSombra = TRUE;

    // Ligado ANTES de ler o banco. Ficava la embaixo, junto das outras chaves do .ini,
    // e com isso nada do arranque entrava no canal de detalhe -- quem escreve detalhe
    // mais cedo e a propria leitura da biblioteca.
    //
    // Ao contrario das outras chaves, esta e desligada por PADRAO: so liga com
    // "logDetalhe=1" explicito. Mora no diario, nao aqui, porque quem mais escreve
    // detalhe e a thread do carregador, noutro modulo.
    {
        bool detalhe = config::LigadoSeDito("logDetalhe");
        diario::DefinirDetalhe(detalhe);
        diario::Escrever("log detalhado: %s", detalhe ? "ligado" : "desligado");
    }

    // Relogio do arranque. Sem isto, "esta demorando" vira adivinhacao: a leitura do
    // banco, o quick_check, a ordenacao e a varredura de ROM sao candidatos parecidos.
    DWORD t0 = GetTickCount(), tm = t0;

    diario::Escrever("montando dispositivos");
    dispositivos::MontarTodos();
    diario::Escrever("  [%u ms] dispositivos", (unsigned)(GetTickCount() - tm)); tm = GetTickCount();

    std::vector<biblioteca::Candidato> candidatos;
    biblioteca::ListarCandidatos(candidatos);

    g_caminhoBanco = config::LerBanco();
    if (!g_caminhoBanco.empty() && !biblioteca::Existe(g_caminhoBanco))
    {
        diario::Escrever("o banco gravado nao existe mais: %s", g_caminhoBanco.c_str());
        g_caminhoBanco.clear();
    }
    if (g_caminhoBanco.empty() && candidatos.size() == 1)
    {
        g_caminhoBanco = candidatos[0].caminho;
        config::GravarBanco(g_caminhoBanco);
    }

    g_bancoOk = true;
    if (!g_caminhoBanco.empty())
        g_bancoOk = biblioteca::Ler(g_caminhoBanco.c_str(), g_jogos);
    diario::Escrever("  [%u ms] ler o banco", (unsigned)(GetTickCount() - tm)); tm = GetTickCount();
    diario::Escrever("biblioteca: %d jogos", (int)g_jogos.size());

    // Biblioteca vazia sem explicacao parece app quebrado. Com a FreeStyle varrendo, e
    // so esperar o scan acabar -- mas isso o usuario precisa ser informado.
    if (!g_bancoOk)
        Avisar("Couldn't read the library. Is FreeStyle scanning? Try again later.");

    // As ROMs entram como itens comuns, e dai em diante nada no app sabe que elas sao
    // diferentes: mesma grade, mesmas colecoes, mesmo lancador. "game:" e a pasta de
    // onde este xex foi lancado -- e nela que mora capas\.
    {
        std::vector<biblioteca::Jogo> roms;
        emuladores::Ler(g_jogos, "game:", roms);
        if (!roms.empty())
        {
            biblioteca::Juntar(g_jogos, roms);
            diario::Escrever("biblioteca com ROMs: %d itens", (int)g_jogos.size());
        }
        diario::Escrever("  [%u ms] varrer ROMs", (unsigned)(GetTickCount() - tm)); tm = GetTickCount();
    }

    colecoes::Carregar();
    fichas::Carregar("game:\\fichas.txt");
    diario::Escrever("  [%u ms] colecoes / [%u ms] ARRANQUE TOTAL",
                     (unsigned)(GetTickCount() - tm), (unsigned)(GetTickCount() - t0));
    carregador::Iniciar();

    // Chaves do .ini para separar hipotese sem recompilar -- o crash ao marcar e
    // intermitente e marcar mexe em exatamente duas coisas: o som e o anel do marcado.
    if (config::Ligado("som"))
        som::Iniciar();
    else
        diario::Escrever("som DESLIGADO pelo .ini");

    g_anelMarcado = config::Ligado("anel");
    if (!g_anelMarcado)
        diario::Escrever("anel do marcado DESLIGADO pelo .ini");


    // Roteiro de controle (roteiro.h): só com game:\roteiro.txt, que é de uma vez.
    roteiro::Iniciar();

    XINPUT_STATE anterior;
    ZeroMemory(&anterior, sizeof(anterior));
    int   direcaoX = 0, direcaoY = 0, ombroAtual = 0;
    DWORD proximoPasso = 0, proximoOmbro = 0, ultimoRelato = 0, quadro = 0;

    for (;;)
    {
        // As QUATRO portas, somadas. Lia só a 0, e quem estivesse com o segundo
        // controle não conseguia navegar -- não havia motivo para isso, é um launcher
        // de sofá. Botões entram por OU; no analógico vale o que estiver mais longe do
        // centro, então um controle parado não anula o que está sendo usado.
        XINPUT_STATE agora;
        ZeroMemory(&agora, sizeof(agora));

        for (DWORD porta = 0; porta < 4; porta++)
        {
            XINPUT_STATE e;
            ZeroMemory(&e, sizeof(e));
            if (XInputGetState(porta, &e) != ERROR_SUCCESS)
                continue;

            agora.Gamepad.wButtons |= e.Gamepad.wButtons;
            if (abs(e.Gamepad.sThumbLX) > abs(agora.Gamepad.sThumbLX))
                agora.Gamepad.sThumbLX = e.Gamepad.sThumbLX;
            if (abs(e.Gamepad.sThumbLY) > abs(agora.Gamepad.sThumbLY))
                agora.Gamepad.sThumbLY = e.Gamepad.sThumbLY;
        }

        // O roteiro aperta junto com os controles, pelo mesmo caminho: o app não sabe
        // a diferença, e o que se fotografa é o que a mão veria.
        if (roteiro::Ativo())
            agora.Gamepad.wButtons |= roteiro::Quadro(GetTickCount(), g_emVoo.empty());

        WORD novos = agora.Gamepad.wButtons & ~anterior.Gamepad.wButtons;
        anterior = agora;

        // Direção: analógico E direcional pela MESMA máquina de repetição -- passo
        // imediato, pausa, depois repetição enquanto estiver segurado. O direcional era
        // disparado por borda ("apertou agora"), então segurá-lo dava um passo só.
        int ax = 0, ay = 0;
        if (agora.Gamepad.sThumbLX >  ZONA_MORTA) ax =  1;
        if (agora.Gamepad.sThumbLX < -ZONA_MORTA) ax = -1;
        if (agora.Gamepad.sThumbLY >  ZONA_MORTA) ay = -1;
        if (agora.Gamepad.sThumbLY < -ZONA_MORTA) ay =  1;

        // O direcional entra como deflexão total. Vem depois do analógico de propósito:
        // com os dois em uso ao mesmo tempo, o direcional manda, que é o mais preciso.
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ax =  1;
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ax = -1;
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ay =  1;
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)    ay = -1;

        int dx = 0, dy = 0;
        DWORD tAgora = GetTickCount();
        if (ax == 0 && ay == 0) { direcaoX = direcaoY = 0; proximoPasso = 0; }
        else if (ax != direcaoX || ay != direcaoY)
        {
            direcaoX = ax; direcaoY = ay; dx = ax; dy = ay;
            proximoPasso = tAgora + ESPERA_INICIAL;
        }
        else if (tAgora >= proximoPasso)
        {
            dx = ax; dy = ay;
            proximoPasso = tAgora + ESPERA_REPETE;
        }

        // LB/RB na mesma máquina: segurar passa letra atrás de letra, em vez de um
        // salto por clique.
        int ombro = 0;
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ombro = -1;
        if (agora.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ombro =  1;

        int salto = 0;
        if (ombro == 0) { ombroAtual = 0; proximoOmbro = 0; }
        else if (ombro != ombroAtual)
        {
            ombroAtual = ombro; salto = ombro;
            proximoOmbro = tAgora + ESPERA_INICIAL;
        }
        else if (tAgora >= proximoOmbro)
        {
            salto = ombro;
            proximoOmbro = tAgora + ESPERA_REPETE;
        }

        // Enquanto a Guide está na tela o controle é DELA. Continuamos desenhando --
        // é isso que o sistema compõe por baixo do teclado --, mas não reagimos a
        // botão nenhum. "anterior" segue sendo atualizado a cada quadro, então o A que
        // confirmou lá dentro não reaparece aqui como botão novo.
        if (teclado::Aberto())
        {
            // nada
        }
        else if (g_menuAberto)
        {
            if (dy != 0 && g_menuQtd > 0)
            {
                g_menuFoco = (g_menuFoco + dy + g_menuQtd) % g_menuQtd;
                som::Tocar(som::SOM_FOCO);
            }
            // UMA ação por quadro: "novos" é máscara e nada impede A e ☰ juntos.
            // Encadeados com if solto, o segundo rodava sobre o estado que o primeiro
            // acabara de trocar -- inclusive sobre um ponteiro recém-invalidado.
            if      (novos & XINPUT_GAMEPAD_A) EscolherNoMenu();
            else if (novos & XINPUT_GAMEPAD_B)
            {
                // Abrir o menu soa; fechar tem de soar também, ou a assimetria se ouve.
                som::Tocar(som::SOM_VOLTA);
                g_menuAberto = false;
            }
        }
        else if (g_detalhe)
        {
            // A faixa tem o controle: A joga, B fecha. Fechando, nada mais.
            if (!g_faixaFechando)
            {
                if      (novos & XINPUT_GAMEPAD_A) Jogar();
                else if (novos & XINPUT_GAMEPAD_B) FecharDetalhes();
            }
        }
        else if (g_tela == TELA_ARQUIVAR)
        {
            // Mesmo gesto da escolha de origens: A marca, Start conclui, B desfaz.
            if (dx) MoverColecao(dx);
            if (dy) MoverColecao(dy * COL_POR_LINHA);

            if (novos & XINPUT_GAMEPAD_A)
            {
                std::vector<colecoes::Colecao *> L = ListaDeColecoes();
                if (!L.empty() && g_iCol < (int)L.size())
                {
                    som::Tocar(som::SOM_CONFIRMA);
                    const int id = L[g_iCol]->id;
                    bool tirou = false;
                    for (size_t i = 0; i < g_escolhidas.size(); i++)
                        if (g_escolhidas[i] == id)
                        {
                            g_escolhidas.erase(g_escolhidas.begin() + i);
                            tirou = true;
                            break;
                        }
                    if (!tirou) g_escolhidas.push_back(id);
                }
            }
            else if (novos & XINPUT_GAMEPAD_B)     { som::Tocar(som::SOM_VOLTA);     FecharArquivar(false); }
            else if (novos & XINPUT_GAMEPAD_START) { som::Tocar(som::SOM_CONFIRMA); FecharArquivar(true); }
        }
        else if (g_tela == TELA_ORIGENS)
        {
            if (dx) MoverColecao(dx);
            if (dy) MoverColecao(dy * COL_POR_LINHA);

            if (novos & XINPUT_GAMEPAD_A)
            {
                std::vector<colecoes::Colecao *> L = ListaDeColecoes();
                if (!L.empty() && g_iCol < (int)L.size())
                {
                    som::Tocar(som::SOM_CONFIRMA);
                    int id = L[g_iCol]->id;
                    bool tirou = false;
                    for (size_t i = 0; i < g_origens.size(); i++)
                        if (g_origens[i] == id)
                        {
                            g_origens.erase(g_origens.begin() + i);
                            tirou = true;
                            break;
                        }
                    if (!tirou) g_origens.push_back(id);
                }
            }
            else if (novos & XINPUT_GAMEPAD_B)     { som::Tocar(som::SOM_VOLTA); FecharOrigens(false); }
            else if (novos & XINPUT_GAMEPAD_START) { som::Tocar(som::SOM_CONFIRMA); FecharOrigens(true); }
        }
        else if (g_tela == TELA_COLECOES)
        {
            // Trocar de aba é a ÚNICA ação do quadro quando acontece. O ramo do menu
            // acima já diz por quê ("uma ação por quadro"), e aqui é pior do que lá:
            // o TrocarAba troca a lista inteira e zera o foco, então um A no mesmo
            // quadro abriria uma coleção sem relação nenhuma com a que estava à vista.
            if (salto) TrocarAba(salto);
            else
            {
                if (dx) MoverColecao(dx);
                if (dy) MoverColecao(dy * COL_POR_LINHA);
                if      (novos & XINPUT_GAMEPAD_A)     AbrirColecao();
                else if (novos & XINPUT_GAMEPAD_X)     NovaColecao();
                else if (novos & XINPUT_GAMEPAD_START) AbrirMenuColecao();
                // Só aqui: na tela de origens o Back não vale, ali se está escolhendo
                // coleção e o exportar não tem nada a ver com a escolha.
                else if (novos & XINPUT_GAMEPAD_BACK)  AbrirMenuExportar();
            }
        }
        else if (g_tela == TELA_SOLTOS)
        {
            // LB/RB continuam sendo a BARRA, não o salto por letra: a barra está na
            // tela dizendo o que eles fazem, e contrariá-la seria pior que perder o
            // salto -- que de todo jeito continua no segurar ↓.
            if (salto) TrocarAba(salto);
            else
            {
                bool segurando = (direcaoY != 0 && proximoPasso != 0 &&
                                  tAgora >= proximoPasso - ESPERA_REPETE);
                if (dx) MoverJogo(dx);
                if (dy)
                {
                    if (segurando) SaltoLetra(dy);
                    else           MoverJogo(dy * COLUNAS);
                }

                if      (novos & XINPUT_GAMEPAD_A) Confirmar();
                else if (novos & XINPUT_GAMEPAD_B) Voltar();
                else if (novos & XINPUT_GAMEPAD_X) AbrirArquivar();
                else if (novos & XINPUT_GAMEPAD_Y) AbrirDetalhes();
            }
        }
        else
        {
            bool segurando = (direcaoY != 0 && proximoPasso != 0 && tAgora >= proximoPasso - ESPERA_REPETE);
            if (dx) MoverJogo(dx);
            if (dy)
            {
                if (segurando && g_tela != TELA_ADICIONAR) SaltoLetra(dy);
                else MoverJogo(dy * COLUNAS);
            }
            if      (salto) SaltoLetra(salto);
            else if (novos & XINPUT_GAMEPAD_A) Confirmar();
            else if (novos & XINPUT_GAMEPAD_B) Voltar();
            else if ((novos & XINPUT_GAMEPAD_Y) && g_tela == TELA_JOGOS) AbrirDetalhes();
            else if (novos & XINPUT_GAMEPAD_X)
            {
                // Composta não guarda jogos próprios: não há o que acrescentar nela.
                if (g_tela == TELA_JOGOS && g_atual != NULL && !colecoes::Composta(g_atual))
                    AbrirAdicionar();
            }
            else if (novos & XINPUT_GAMEPAD_START)
            {
                if (g_tela == TELA_ADICIONAR)
                {
                    som::Tocar(som::SOM_CONFIRMA);   // conclui a seleção inteira
                    FecharAdicionar(true);
                }
                else AbrirMenuJogo();
            }
        }

        quadro++;
        AtenderTeclado();
        ClampFoco();
        PedirOQueFalta();
        RecolherCarregadas();
        Desenhar();

        if (roteiro::QuerSair())
        {
            SoltarTudo();
            lancador::Dashboard();   // dando certo, não volta
            for (;;) Sleep(1000);    // não deu: parado, com o diário dizendo por quê
        }

        if (tAgora - ultimoRelato > 5000)
        {
            ultimoRelato = tAgora;
            MEMORYSTATUS mem; mem.dwLength = sizeof(mem);
            GlobalMemoryStatus(&mem);

            int cheias = 0;
            for (int i = 0; i < CACHE_MAX; i++)
                if (g_cache[i].textura != NULL) cheias++;

            diario::Escrever("quadro=%u tela=%d cache=%d emVoo=%d falhou=%d livre=%u KB",
                             quadro, (int)g_tela, cheias, (int)g_emVoo.size(),
                             (int)g_falhou.size(), (unsigned)(mem.dwAvailPhys / 1024));
        }
    }
}
