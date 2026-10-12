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
    const int COL_POR_LINHA = 4, COL_LINHAS = 2, COL_TOPO = 72;
    // Centralizada: 4*244 + 3*32 = 1072, sobra 208 dividida nos dois lados.
    const int COL_MARGEM = (1280 - (COL_POR_LINHA * COL_LADO + (COL_POR_LINHA - 1) * COL_GAP)) / 2;
    const int COL_POR_PAGINA = COL_POR_LINHA * COL_LINHAS;

    // O asset tipo 128 é o ENCARTE inteiro (contracapa + lombada + frente). A frente
    // são os 46,8% da direita — fração calibrada contra a biblioteca real. Em vez de
    // recortar a imagem, amostramos só essa parte ao desenhar.
    const float FRENTE_U0 = 1.0f - 0.468f;

    const D3DCOLOR COR_FUNDO   = D3DCOLOR_XRGB(13, 17, 15);
    const D3DCOLOR COR_PAINEL  = D3DCOLOR_XRGB(21, 27, 24);
    const D3DCOLOR COR_TEXTO   = D3DCOLOR_XRGB(231, 237, 233);
    const D3DCOLOR COR_APAGADO = D3DCOLOR_XRGB(142, 156, 148);
    const D3DCOLOR COR_FRACO   = D3DCOLOR_XRGB(92, 104, 98);
    const D3DCOLOR COR_LINHA   = D3DCOLOR_XRGB(43, 53, 47);
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

    // A janela da barra de abas, quando a fileira nao cabe inteira. Mesmo papel do
    // g_primeiraLinha da grade: a janela segue o foco.
    int  g_primeiraAba = 0;
    bool g_lbVivo = false, g_rbVivo = false;   // ha aba para aquele lado?

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
        case biblioteca::ABA_CLASSICO: return L"Xbox Clássico";
        case biblioteca::ABA_HOMEBREW: return L"Homebrew";
        case biblioteca::ABA_EMULADOR: return L"Emulador";
        case biblioteca::ABA_OUTROS:   return L"Outros";
        case ABA_SOLTOS:               return L"Soltos";
        }
        return L"Todos";
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
        // corrente ali e justamente "Soltos", que nao e categoria de jogo nenhum.
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
    // E no limite: com UMA categoria so, "Todos" e ela sao a mesma lista, e a barra
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
        // mesmo sem categoria nenhuma: "Soltos" e "Todos" ja sao escolha de verdade.
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

    // "em todas: Co-op, Não zerado" -- o que responde "por que este jogo está aqui?".
    //
    // Por EXTENSO, e não com os símbolos de conjunto: o atlas Arial_16.xpr vai só até
    // U+20AC e a tabela de tradução manda tudo acima disso para o glifo 0, o
    // "desconhecido". Um '∩' sairia como quadradinho na TV. O mesmo vale para o '✓' e
    // para as reticências U+2026 -- as do ATGFONT_TRUNCATED são três pontos de
    // verdade, montados a partir do L'.', então essas continuam valendo.
    std::string Formula(const colecoes::Colecao *c)
    {
        if (!colecoes::Composta(c)) return std::string();

        // O MESMO PodeSerOrigem que o Tem() usa, nao so "existe": o rotulo e o
        // calculo tem de concordar, senao o subtitulo cita uma origem que o resultado
        // ignora. Hoje a poda roda antes e os dois dariam igual -- mas depender disso
        // e depender de ordem de chamada, e o resto deste commit faz questao de nao.
        std::string s;
        for (size_t i = 0; i < c->origens.size(); i++)
        {
            const colecoes::Colecao *o = colecoes::PorId(c->origens[i]);
            if (!colecoes::PodeSerOrigem(o, c->tipo)) continue;
            if (!s.empty()) s += ", ";
            s += o->nome;
        }
        if (s.empty()) return std::string();

        // As mesmas palavras do menu que criou a coleção, de propósito: quem escolheu
        // "só o que estiver em todas elas" reencontra "em todas" aqui.
        return (c->tipo == colecoes::TIPO_UNIAO ? "em qualquer uma: " : "em todas: ") + s;
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

    // ---- desenho ---------------------------------------------------------------
    void Cabecalho(const WCHAR *titulo, const WCHAR *sub)
    {
        g_fonte.SetScaleFactors(1.5f, 1.5f);
        g_fonte.DrawText((FLOAT)MARGEM_X, 34.0f, COR_TEXTO, titulo, 0);
        g_fonte.SetScaleFactors(1.0f, 1.0f);
        // CORTADO por largura: o subtítulo de uma composta é a lista das origens, e
        // com três nomes de 28 bytes ele passaria da borda direita da tela.
        if (sub != NULL && sub[0] != L'\0')
            g_fonte.DrawText((FLOAT)MARGEM_X + 260.0f, 46.0f, COR_FRACO, sub,
                             ATGFONT_TRUNCATED, 1180.0f - ((FLOAT)MARGEM_X + 260.0f));
    }

    void Rodape(const WCHAR *esquerda, const WCHAR *direita)
    {
        g_fonte.DrawText((FLOAT)MARGEM_X, 656.0f, COR_APAGADO, esquerda, 0);
        if (direita != NULL && direita[0] != L'\0')
            g_fonte.DrawText(1180.0f, 656.0f, COR_FRACO, direita, ATGFONT_RIGHT);
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

    // Uma fonte da verdade para o fundo: o gradiente da tela e a cor dos cantos saem
    // dos MESMOS numeros, senao os cantos aparecem como quadradinhos de outro tom.
    // Mais claros que os da previa de propósito: o console passa a saida por uma rampa
    // de gama que o navegador nao aplica, e os mesmos numeros chegam visivelmente mais
    // escuros na TV. Estes sao os da previa corrigidos por uma curva de 1/1,3 -- e sao
    // um botao: se ainda ficar escuro, sobe; se lavar, desce.
    const int FUNDO_DE[3]   = { 19, 40, 31 };
    const int FUNDO_PARA[3] = { 43, 73, 57 };

    D3DCOLOR CorDoFundo(float x, float y)
    {
        // A MESMA interpolacao do gradiente diagonal, que e bilinear com os cantos
        // laterais na media: t = (x/largura + y/altura) / 2. Com (x+y)/2000 os dois
        // coincidiam so nos extremos e divergiam ~3 de 255 no meio -- pouco, mas e um
        // degrau de borda dura em fundo escuro, que e o pior caso para banding.
        float t = 0.5f * (x / 1280.0f + y / 720.0f);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return D3DCOLOR_XRGB(
            (int)(FUNDO_DE[0] + (FUNDO_PARA[0] - FUNDO_DE[0]) * t),
            (int)(FUNDO_DE[1] + (FUNDO_PARA[1] - FUNDO_DE[1]) * t),
            (int)(FUNDO_DE[2] + (FUNDO_PARA[2] - FUNDO_DE[2]) * t));
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

    // So os cards de colecao passam por aqui -- e por isso que o caixa alta fica so
    // neles, e a grade de jogos continua com o nome como ele e.
    int QuebrarNome(const std::string &nome, float maxLargura, WCHAR l1[96], WCHAR l2[96])
    {
        WCHAR largo[256];
        Larga(nome, largo, 256);

        // ANTES de medir: maiuscula e mais larga, e medir o original deixaria a quebra
        // de linha errada por alguns pixels.
        ParaMaiusculas(largo);

        l1[0] = l2[0] = L'\0';

        FLOAT w = 0.0f, h = 0.0f;
        g_fonte.GetTextExtent(largo, &w, &h);
        if (w <= maxLargura)
        {
            wcsncpy(l1, largo, 95); l1[95] = L'\0';
            return 1;
        }

        // O limite e o TAMANHO DE l1, nao um numero solto: corte maior que 95 escreve
        // fora do buffer na pilha. Hoje nao dispara porque a tabela de larguras da
        // fonte nao deixa 95 caracteres caberem em 216 px -- mas quem segura o buffer
        // passa a ser a metrica da fonte, e nao o programa. Um colecoes.txt editado a
        // mao basta para provar o ponto.
        int corte = -1;
        for (int i = 0; largo[i] != L'\0' && i < 95; i++)
        {
            if (largo[i] != L' ') continue;
            WCHAR t[256];
            wcsncpy(t, largo, i); t[i] = L'\0';
            g_fonte.GetTextExtent(t, &w, &h);
            if (w <= maxLargura) corte = i; else break;
        }

        if (corte <= 0)                      // palavra unica maior que a linha
        {
            wcsncpy(l1, largo, 95); l1[95] = L'\0';
            while (l1[0] != L'\0')
            {
                g_fonte.GetTextExtent(l1, &w, &h);
                if (w <= maxLargura) break;
                l1[wcslen(l1) - 1] = L'\0';
            }
            return 1;
        }

        wcsncpy(l1, largo, corte); l1[corte] = L'\0';
        wcsncpy(l2, largo + corte + 1, 95); l2[95] = L'\0';

        // corta a segunda a mao, com reticencias
        g_fonte.GetTextExtent(l2, &w, &h);
        while (w > maxLargura && wcslen(l2) > 1)
        {
            size_t n = wcslen(l2);
            l2[n - 1] = L'\0';
            if (n >= 4) { l2[n - 2] = L'.'; l2[n - 3] = L'.'; l2[n - 4] = L'.'; }
            g_fonte.GetTextExtent(l2, &w, &h);
        }
        return 2;
    }

    // Card de coleção: colagem de três capas inclinadas, tom por cima, véu embaixo
    // para o texto, e os quatro cantos arredondados por último -- eles comem o canto da
    // colagem E do anel de foco, então o anel sai arredondado de brinde.
    void DesenharCard(int x, int y, int lado, const colecoes::Colecao *col, bool focado)
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        D3DRECT r;
        r.x1 = x; r.y1 = y; r.x2 = x + lado; r.y2 = y + lado;

        // Todas as cores do card passaram pela MESMA correcao de gama do fundo (curva
        // de 1/1,3): sem isso o card fica escuro contra um fundo que clareou.
        Preencher(r, D3DCOLOR_XRGB(33, 47, 39));

        const biblioteca::Jogo *tres[3];
        int n = 0;
        TresDaColecao(col, tres, &n);

        if (n > 0)
        {
            // Scissor: é o recorte que o D3D oferece sem stencil, e o card é alinhado
            // aos eixos, então serve exatamente.
            RECT sc;
            sc.left = x; sc.top = y; sc.right = x + lado; sc.bottom = y + lado;
            d->SetScissorRect(&sc);
            d->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

            const float w = lado * 0.56f;
            const float h = w * 1.425f;                 // proporção da frente do encarte
            const float g = w * 0.14f;                  // folga entre as capas
            const float ang = -30.0f * 3.14159265f / 180.0f;
            const float co = cosf(ang), si = sinf(ang);
            const float cx = x + lado * 0.5f, cy = y + lado * 0.5f;

            const float ox[3] = { -(w + g), 0.0f,          0.0f  };
            const float oy[3] = { -h * 0.5f, -(h + g * 0.5f), g * 0.5f };

            Mistura(true);
            for (int k = 0; k < n; k++)
            {
                bool inteira = true;
                D3DTexture *capa = NoCache(tres[k]->id, true, &inteira);
                if (capa == NULL) continue;

                // DENTRO do laco: as tres capas da colagem podem ter origens
                // diferentes -- uma ROM e um jogo da FreeStyle no mesmo card. Calculado
                // uma vez so la fora, a origem da primeira valeria para as tres.
                const float u0 = inteira ? 0.0f : FRENTE_U0;
                XMFLOAT2 uv[4];
                uv[0] = XMFLOAT2(u0,   0.0f);
                uv[1] = XMFLOAT2(1.0f, 0.0f);
                uv[2] = XMFLOAT2(1.0f, 1.0f);
                uv[3] = XMFLOAT2(u0,   1.0f);

                const float px = ox[k], py = oy[k];
                const float canto[4][2] = {
                    { px,     py     }, { px + w, py     },
                    { px + w, py + h }, { px,     py + h }
                };

                XMFLOAT2 pos[4];
                for (int c = 0; c < 4; c++)
                    pos[c] = XMFLOAT2(cx + canto[c][0] * co - canto[c][1] * si,
                                      cy + canto[c][0] * si + canto[c][1] * co);

                QuadTex(pos, uv, capa, D3DCOLOR_ARGB(153, 255, 255, 255), true);
            }
            Mistura(false);

            d->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

            // O tom por cima é GRADIENTE, mais forte em cima e aliviando embaixo --
            // era chapado aqui, porque portei a versão anterior à decisão do gradiente.
            Gradiente((float)x, (float)y, (float)(x + lado), (float)(y + lado),
                      D3DCOLOR_ARGB(168, 19, 36, 26),
                      D3DCOLOR_ARGB( 97, 27, 49, 36), false);

            // Véu só na parte de baixo: é o que mantém nome e contagem legíveis.
            Gradiente((float)x, (float)(y + lado * 0.42f),
                      (float)(x + lado), (float)(y + lado),
                      D3DCOLOR_ARGB(0, 18, 27, 21), D3DCOLOR_ARGB(240, 18, 27, 21), false);
        }

        // Emblema no canto superior direito: marca que esta colecao e composta, e
        // QUAL das duas. Vai ANTES do contorno e dos cantos, para ser recortado junto
        // se encostar.
        D3DTexture *emb = NULL;
        if (col != NULL)
        {
            if      (col->tipo == colecoes::TIPO_UNIAO)      emb = g_raio;
            else if (col->tipo == colecoes::TIPO_INTERSECAO) emb = g_lentes;
        }

        if (emb != NULL)
        {
            const int M = 22, folga = 10;
            XMFLOAT2 pos[4], uvr[4];
            const float rx = (float)(x + lado - folga - M), ry = (float)(y + folga);
            pos[0] = XMFLOAT2(rx,     ry);
            pos[1] = XMFLOAT2(rx + M, ry);
            pos[2] = XMFLOAT2(rx + M, ry + M);
            pos[3] = XMFLOAT2(rx,     ry + M);
            uvr[0] = XMFLOAT2(0.0f, 0.0f);
            uvr[1] = XMFLOAT2(1.0f, 0.0f);
            uvr[2] = XMFLOAT2(1.0f, 1.0f);
            uvr[3] = XMFLOAT2(0.0f, 1.0f);

            Mistura(true);
            // Sombra um pixel abaixo, para o emblema nao sumir sobre capa clara.
            XMFLOAT2 sombra[4];
            for (int c = 0; c < 4; c++) sombra[c] = XMFLOAT2(pos[c].x + 1.0f, pos[c].y + 1.0f);
            QuadTex(sombra, uvr, emb, D3DCOLOR_ARGB(170, 0, 0, 0), true);
            QuadTex(pos,    uvr, emb, COR_ANEL, true);
            Mistura(false);
        }

        // O contorno sai em QUATRO SEGMENTOS RETOS, parando antes dos cantos -- e a
        // curva vem depois, da textura de arco. Desenhar o retângulo inteiro e deixar a
        // máscara comer os cantos deixava o contorno CORTADO, com falha nas quinas, em
        // vez de arredondado.
        const int R = (g_canto != NULL) ? CANTO_N : 0;
        const int esp = focado ? 3 : 1;
        const D3DCOLOR corAnel = focado ? COR_ANEL : COR_LINHA;

        {
            D3DRECT seg;
            seg.x1 = x + R; seg.x2 = x + lado - R;
            seg.y1 = y;             seg.y2 = y + esp;            Preencher(seg, corAnel);
            seg.y1 = y + lado - esp; seg.y2 = y + lado;          Preencher(seg, corAnel);

            seg.y1 = y + R; seg.y2 = y + lado - R;
            seg.x1 = x;              seg.x2 = x + esp;           Preencher(seg, corAnel);
            seg.x1 = x + lado - esp; seg.x2 = x + lado;          Preencher(seg, corAnel);
        }

        if (g_canto != NULL)
        {
            const float u[2] = { 0.0f, 1.0f };
            // superior esquerdo, superior direito, inferior esquerdo, inferior direito
            const int px[4] = { x, x + lado - R, x,            x + lado - R };
            const int py[4] = { y, y,            y + lado - R, y + lado - R };
            const int fx[4] = { 0, 1, 0, 1 };      // espelha em U
            const int fy[4] = { 0, 0, 1, 1 };      // espelha em V

            D3DTexture *arco = g_arco[focado ? 1 : 0];

            for (int c = 0; c < 4; c++)
            {
                XMFLOAT2 pos[4], uvc[4];
                pos[0] = XMFLOAT2((float)px[c],     (float)py[c]);
                pos[1] = XMFLOAT2((float)px[c] + R, (float)py[c]);
                pos[2] = XMFLOAT2((float)px[c] + R, (float)py[c] + R);
                pos[3] = XMFLOAT2((float)px[c],     (float)py[c] + R);

                const float u0 = u[fx[c]], u1 = u[1 - fx[c]];
                const float v0 = u[fy[c]], v1 = u[1 - fy[c]];
                uvc[0] = XMFLOAT2(u0, v0);
                uvc[1] = XMFLOAT2(u1, v0);
                uvc[2] = XMFLOAT2(u1, v1);
                uvc[3] = XMFLOAT2(u0, v1);

                Mistura(true);
                // Máscara primeiro: ela apaga o canto da colagem. Depois o arco, na cor
                // do contorno -- se viesse antes, a máscara o apagaria junto.
                QuadTex(pos, uvc, g_canto, CorDoFundo((float)px[c], (float)py[c]), true);
                if (arco != NULL)
                    QuadTex(pos, uvc, arco, corAnel, true);
                Mistura(false);
            }
        }
    }

    void Caixa(int x, int y, int l, int a, bool focada)
    {
        D3DRECT r;
        r.x1 = x; r.y1 = y; r.x2 = x + l; r.y2 = y + a;
        Preencher(r, COR_PAINEL);
        ATG::DebugDraw::DrawScreenSpaceRect(r, focada ? 3.0f : 1.0f,
                                            focada ? COR_ANEL : COR_LINHA);
    }

    // Painel do modal, arredondado POR DENTRO: o corpo sai em dois retangulos e os
    // quatro cantos sao a mascara interna pintada na cor do painel. Assim o canto e
    // desenhado SOBRE o que ja esta na tela, sem depender de adivinhar a cor do fundo
    // -- que sob o veu varia, porque ha card e capa por baixo. E por isso que aqui a
    // mascara e a interna e nao a dos cards.
    void PainelRedondo(int x, int y, int l, int a, D3DCOLOR cor, D3DCOLOR contorno)
    {
        const int R = (g_cantoInt != NULL) ? CANTO_N : 0;
        D3DRECT r;

        r.x1 = x + R; r.y1 = y;     r.x2 = x + l - R; r.y2 = y + a;      Preencher(r, cor);
        if (R == 0)
        {
            if (contorno != 0) ATG::DebugDraw::DrawScreenSpaceRect(r, 1.0f, contorno);
            return;
        }
        r.x1 = x;     r.y1 = y + R; r.x2 = x + l;     r.y2 = y + a - R;  Preencher(r, cor);

        const float u[2] = { 0.0f, 1.0f };
        const int px[4] = { x, x + l - R, x,         x + l - R };
        const int py[4] = { y, y,         y + a - R, y + a - R };
        const int fx[4] = { 0, 1, 0, 1 };      // espelha em U
        const int fy[4] = { 0, 0, 1, 1 };      // espelha em V

        Mistura(true);
        for (int c = 0; c < 4; c++)
        {
            XMFLOAT2 pos[4], uvc[4];
            pos[0] = XMFLOAT2((float)px[c],           (float)py[c]);
            pos[1] = XMFLOAT2((float)(px[c] + R),     (float)py[c]);
            pos[2] = XMFLOAT2((float)(px[c] + R),     (float)(py[c] + R));
            pos[3] = XMFLOAT2((float)px[c],           (float)(py[c] + R));

            const float u0 = u[fx[c]], u1 = u[1 - fx[c]];
            const float v0 = u[fy[c]], v1 = u[1 - fy[c]];
            uvc[0] = XMFLOAT2(u0, v0);
            uvc[1] = XMFLOAT2(u1, v0);
            uvc[2] = XMFLOAT2(u1, v1);
            uvc[3] = XMFLOAT2(u0, v1);

            QuadTex(pos, uvc, g_cantoInt, cor, true);
            // O contorno vem no MESMO laço, por cima do canto já pintado: separado,
            // seria percorrer os quatro cantos duas vezes para o mesmo resultado.
            if (contorno != 0 && g_arco[0] != NULL)
                QuadTex(pos, uvc, g_arco[0], contorno, true);
        }
        Mistura(false);

        if (contorno == 0) return;

        // Os quatro segmentos RETOS param antes dos cantos; a curva já saiu do arco.
        // Desenhar o retângulo inteiro deixaria o contorno cortado nas quinas.
        r.x1 = x + R;     r.x2 = x + l - R;
        r.y1 = y;         r.y2 = y + 1;         Preencher(r, contorno);
        r.y1 = y + a - 1; r.y2 = y + a;         Preencher(r, contorno);
        r.y1 = y + R;     r.y2 = y + a - R;
        r.x1 = x;         r.x2 = x + 1;         Preencher(r, contorno);
        r.x1 = x + l - 1; r.x2 = x + l;         Preencher(r, contorno);
    }

    // ---- a barra de categorias -------------------------------------------------
    //
    // Mora no alto da tela, que NESTA tela sempre foi vazio: a grade de coleções não
    // tem cabeçalho. A grade desce de COL_TOPO para COL_TOPO_ABA quando a barra
    // existe, e volta sozinha quando não existe.
    //
    // 96 + 244 + 32 + 244 = 616, e o rodapé só começa em 656. Cabe sem apertar nada.
    const int ABA_Y = 26, ABA_ALT = 32, ABA_PAD = 14, ABA_GAP = 10;
    const int COL_TOPO_ABA = 96;
    const int CHIP_L = 34, CHIP_ALT = 22;          // a caixinha do LB e do RB
    const D3DCOLOR COR_SOBRE_ANEL = D3DCOLOR_XRGB(11, 16, 4);

    struct PilulaAba { int x, l, aba; };

    // A fileira ocupa a faixa entre os dois chips. Com as sete categorias da
    // FreeStyle mais o Soltos ela soma 989 px numa faixa de 980: passa por NOVE, e
    // esconde uma pilula. Neste console, que tem seis categorias, sao 882 e nada
    // rola. Entao a rolagem e rede de seguranca, nao compromisso diario.
    const int ABA_X0  = MARGEM_X + CHIP_L + 16;
    const int ABA_FIM = 1180 - CHIP_L - 16;

    // Quantas pilulas inteiras cabem a partir de 'de'. INTEIRAS: meia pilula cortada
    // na TV parece defeito, e o scissor cortaria no meio de uma letra.
    int AbasQueCabem(const int *larg, int n, int de)
    {
        int x = 0, quantas = 0;
        for (int j = de; j < n; j++)
        {
            const int novo = x + larg[j] + (quantas ? ABA_GAP : 0);
            if (novo > ABA_FIM - ABA_X0) break;
            x = novo; quantas++;
        }
        return quantas;
    }

    // Mede e posiciona a fileira VISIVEL. SEPARADO do desenho porque as pilulas sao
    // retangulos e os rotulos sao texto: o Begin/End da fonte salva e restaura estado
    // de render, e intercalar os dois embaralha o D3D -- a mesma razao pela qual a
    // grade de capas desenha as imagens primeiro e os nomes todos depois.
    int MedirAbas(PilulaAba *p, int capacidade, int *xRB)
    {
        const int n = (int)g_abas.size();
        if (n <= 0) { *xRB = ABA_X0; return 0; }

        int larg[biblioteca::ABA_QUANTAS + 1];
        for (int i = 0; i < n && i < biblioteca::ABA_QUANTAS + 1; i++)
        {
            FLOAT w = 0.0f, h = 0.0f;
            g_fonte.GetTextExtent(NomeDaAba(g_abas[i]), &w, &h);
            larg[i] = (int)w + ABA_PAD * 2;
        }

        // A janela segue o foco, como o SeguirFoco da grade.
        const int foco = IndiceDaAba();
        if (g_primeiraAba > foco) g_primeiraAba = foco;
        while (foco >= g_primeiraAba + AbasQueCabem(larg, n, g_primeiraAba)) g_primeiraAba++;
        // E volta assim que couber de novo: sem isto a barra ficava rolada para
        // sempre depois de uma visita a ultima aba.
        while (g_primeiraAba > 0 &&
               AbasQueCabem(larg, n, g_primeiraAba - 1) > foco - g_primeiraAba + 1)
            g_primeiraAba--;

        const int ate = g_primeiraAba + AbasQueCabem(larg, n, g_primeiraAba);

        int quantas = 0, x = ABA_X0;
        for (int i = g_primeiraAba; i < ate && quantas < capacidade; i++)
        {
            p[quantas].aba = g_abas[i];
            p[quantas].x   = x;
            p[quantas].l   = larg[i];
            x += larg[i] + ABA_GAP;
            quantas++;
        }
        *xRB = x - ABA_GAP + 16;

        // Chip apagado na ponta: LB na primeira aba e RB na ultima nao fazem nada, e
        // a tela tem de dizer isso antes de a pessoa apertar. Vale desde sempre, sem
        // o Soltos: no "Todos" o LB ja nao fazia nada e nada avisava.
        g_lbVivo = (foco > 0);
        g_rbVivo = (foco < n - 1);
        return quantas;
    }

    void PilulaCheia(int x, int y, int l, int a, D3DCOLOR cor)
    {
        D3DRECT r;
        r.x1 = x; r.y1 = y; r.x2 = x + l; r.y2 = y + a;
        Preencher(r, cor);

        if (g_canto == NULL) return;      // sem a máscara, fica reta e pronto

        const int R = CANTO_N;
        const float u[2] = { 0.0f, 1.0f };
        const int px[4] = { x, x + l - R, x,         x + l - R };
        const int py[4] = { y, y,         y + a - R, y + a - R };
        const int fx[4] = { 0, 1, 0, 1 };
        const int fy[4] = { 0, 0, 1, 1 };

        for (int c = 0; c < 4; c++)
        {
            XMFLOAT2 pos[4], uvc[4];
            pos[0] = XMFLOAT2((float)px[c],     (float)py[c]);
            pos[1] = XMFLOAT2((float)px[c] + R, (float)py[c]);
            pos[2] = XMFLOAT2((float)px[c] + R, (float)py[c] + R);
            pos[3] = XMFLOAT2((float)px[c],     (float)py[c] + R);

            const float u0 = u[fx[c]], u1 = u[1 - fx[c]];
            const float v0 = u[fy[c]], v1 = u[1 - fy[c]];
            uvc[0] = XMFLOAT2(u0, v0);
            uvc[1] = XMFLOAT2(u1, v0);
            uvc[2] = XMFLOAT2(u1, v1);
            uvc[3] = XMFLOAT2(u0, v1);

            Mistura(true);
            QuadTex(pos, uvc, g_canto, CorDoFundo((float)px[c], (float)py[c]), true);
            Mistura(false);
        }
    }

    void AbasFundo(const PilulaAba *p, int n, int xRB)
    {
        Caixa(MARGEM_X, ABA_Y + 5, CHIP_L, CHIP_ALT, false);
        Caixa(xRB,      ABA_Y + 5, CHIP_L, CHIP_ALT, false);

        for (int i = 0; i < n; i++)
            if (p[i].aba == g_aba)
                PilulaCheia(p[i].x, ABA_Y, p[i].l, ABA_ALT, COR_ANEL);
    }

    // Dentro do Begin da fonte.
    void AbasTexto(const PilulaAba *p, int n, int xRB)
    {
        g_fonte.SetScaleFactors(0.72f, 0.72f);
        g_fonte.DrawText((FLOAT)MARGEM_X + CHIP_L * 0.5f, (FLOAT)ABA_Y + 7.0f,
                         g_lbVivo ? COR_FRACO : COR_LINHA, L"LB", ATGFONT_CENTER_X);
        g_fonte.DrawText((FLOAT)xRB + CHIP_L * 0.5f, (FLOAT)ABA_Y + 7.0f,
                         g_rbVivo ? COR_FRACO : COR_LINHA, L"RB", ATGFONT_CENTER_X);
        g_fonte.SetScaleFactors(1.0f, 1.0f);

        for (int i = 0; i < n; i++)
        {
            const bool ativa = (p[i].aba == g_aba);
            g_fonte.DrawText((FLOAT)(p[i].x + ABA_PAD), (FLOAT)ABA_Y + 3.0f,
                             ativa ? COR_SOBRE_ANEL : COR_APAGADO,
                             NomeDaAba(p[i].aba), 0);
        }
    }

    void TelaColecoes()
    {
        std::vector<colecoes::Colecao *> L = ListaDeColecoes();
        WCHAR texto[256];

        if (L.empty())
        {
            g_fonte.Begin();
            g_fonte.SetScaleFactors(1.3f, 1.3f);
            // Interseção aceita junção como origem, junção não aceita -- então a
            // frase tem de dizer o que FALTA, e não a mesma coisa para os dois.
            const WCHAR *vazia = L"Nenhuma coleção ainda";
            if (g_tela == TELA_ARQUIVAR)
                vazia = L"Nenhuma coleção para arquivar";
            else if (g_tela == TELA_ORIGENS && g_editandoComposta != NULL)
                vazia = (g_editandoComposta->tipo == colecoes::TIPO_UNIAO)
                      ? L"Nenhuma coleção de jogos para juntar"
                      : L"Nenhuma coleção para cruzar";
            g_fonte.DrawText(640.0f, 320.0f, COR_APAGADO, vazia, ATGFONT_CENTER_X);
            g_fonte.SetScaleFactors(1.0f, 1.0f);
            // Sem coleção nenhuma o exportar continua valendo: ele é da biblioteca.
            Rodape((g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR)
                   ? GLYPH_B_BUTTON L" Cancelar"
                   : GLYPH_X_BUTTON L" Nova coleção     " GLYPH_BACK_BUTTON L" Exportar",
                   L"");
            g_fonte.End();
            return;
        }

        // A barra de categorias, quando houver categoria para escolher. Nas telas de
        // origens e de arquivar ela não aparece: ali a aba não vale, e mostrá-la
        // acesa no "Soltos" enquanto se escolhe coleção seria dizer que a lista
        // abaixo está filtrada por ela -- não está.
        PilulaAba pilulas[biblioteca::ABA_QUANTAS + 1];   // +1: o Soltos
        int xRB = 0, nAbas = 0;
        if (g_tela == TELA_COLECOES && g_abas.size() >= 2)
            nAbas = MedirAbas(pilulas, biblioteca::ABA_QUANTAS + 1, &xRB);
        if (nAbas >= 2) AbasFundo(pilulas, nAbas, xRB);

        const int topo = (nAbas >= 2) ? COL_TOPO_ABA : COL_TOPO;

        // Rola de uma linha por vez, como a tela de jogos -- não troca a página
        // inteira. Paginar fazia a tela mudar de uma vez ao passar do 8º item, e com
        // tudo saindo junto perde-se a referência de onde se estava.
        int paginaInicio = g_primeiraLinhaCol * COL_POR_LINHA;

        for (int k = 0; k < COL_POR_PAGINA; k++)
        {
            int i = paginaInicio + k;
            if (i >= (int)L.size()) break;

            int x = COL_MARGEM + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
            int y = topo + (k / COL_POR_LINHA) * (COL_LADO + COL_GAP);
            DesenharCard(x, y, COL_LADO, L[i], i == g_iCol);

            // Escolhendo origens -- ou arquivando um jogo --, a não marcada recebe
            // o mesmo véu dos jogos não marcados, e a marcada ganha o anel verde. O
            // vocabulário é o mesmo de propósito: é o mesmo gesto.
            if (g_tela == TELA_ORIGENS || g_tela == TELA_ARQUIVAR)
            {
                const bool marcada = (g_tela == TELA_ORIGENS)
                                   ? OrigemMarcada(L[i]->id)
                                   : ColecaoEscolhida(L[i]->id);
                D3DRECT rc;
                rc.x1 = x; rc.y1 = y; rc.x2 = x + COL_LADO; rc.y2 = y + COL_LADO;
                if (!marcada) Preencher(rc, D3DCOLOR_ARGB(150, 6, 9, 8));
                else          ATG::DebugDraw::DrawScreenSpaceRect(rc, 2.0f, COR_ANEL);
            }
        }

        g_fonte.Begin();
        if (nAbas >= 2) AbasTexto(pilulas, nAbas, xRB);

        for (int k = 0; k < COL_POR_PAGINA; k++)
        {
            int i = paginaInicio + k;
            if (i >= (int)L.size()) break;

            int x = COL_MARGEM + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
            int y = topo + (k / COL_POR_LINHA) * (COL_LADO + COL_GAP);

            // Nome e contagem centrados na HORIZONTAL, e o bloco ancorado pela base a
            // 86% da altura do card, crescendo para cima quando o nome ocupa duas
            // linhas. Nem centralizado na vertical, nem colado no rodapé.
            const float ALT_LINHA = 28.0f, ALT_CONT = 24.0f;
            const float meio = (FLOAT)x + COL_LADO * 0.5f;

            WCHAR l1[96], l2[96];
            int nLinhas = QuebrarNome(L[i]->nome, (FLOAT)COL_LADO - 28.0f, l1, l2);

            float topo = (FLOAT)y + COL_LADO * 0.86f - (nLinhas * ALT_LINHA + ALT_CONT);

            g_fonte.DrawText(meio, topo, (i == g_iCol) ? COR_TEXTO : COR_APAGADO,
                             l1, ATGFONT_CENTER_X);
            if (nLinhas == 2 && l2[0] != L'\0')
                g_fonte.DrawText(meio, topo + ALT_LINHA,
                                 (i == g_iCol) ? COR_TEXTO : COR_APAGADO,
                                 l2, ATGFONT_CENTER_X);

            // Conta os ITENS que a coleção mostra, não quantos TitleIds guarda: um
            // TitleId de multi-disco casa com dois itens. Entre parênteses e sem a
            // palavra "jogos" -- vazia vira "( 0 )".
            int quantos = 0;
            for (size_t j = 0; j < g_jogos.size(); j++)
                if (NaAba(g_jogos[j]) && colecoes::Tem(L[i], g_jogos[j].titleId))
                    quantos++;

            swprintf_s(texto, 256, L"( %d )", quantos);
            g_fonte.DrawText(meio, topo + nLinhas * ALT_LINHA, COR_FRACO,
                             texto, ATGFONT_CENTER_X);
        }

        swprintf_s(texto, 256, L"%d de %d", g_iCol + 1, (int)L.size());
        if (g_tela == TELA_ARQUIVAR)
        {
            // Sem cabeçalho, como a tela de origens -- quem diz QUAL jogo está sendo
            // arquivado é esta linha. Cortado em 420 px: nome de jogo passa disso
            // fácil, e o que importa aqui é a contagem no fim.
            WCHAR nome[256];
            Larga(g_arquivando != NULL ? g_arquivando->nome : "", nome, 256);
            FLOAT w = 0.0f, h = 0.0f;
            g_fonte.GetTextExtent(nome, &w, &h);
            if (w > 420.0f)
            {
                size_t n = wcslen(nome);
                while (n > 1 && w > 420.0f)
                {
                    nome[--n] = L'\0';
                    if (n >= 3) { nome[n-1] = L'.'; nome[n-2] = L'.'; nome[n-3] = L'.'; }
                    g_fonte.GetTextExtent(nome, &w, &h);
                }
            }
            const int q = (int)g_escolhidas.size();
            swprintf_s(texto, 256, L"%s · %d %s", nome, q,
                       (q == 1) ? L"coleção" : L"coleções");
            Rodape(GLYPH_A_BUTTON L" Marcar     " GLYPH_B_BUTTON L" Cancelar     "
                   GLYPH_START_BUTTON L" Concluir", texto);
        }
        else if (g_tela == TELA_ORIGENS)
        {
            // Quantos jogos o rascunho daria SE fosse concluido agora. E a razao de
            // ser desta linha: um cruzamento esvazia facil, e descobrir o ( 0 ) so
            // depois de gravar e tarde. O '·' e U+00B7, que ESTA no atlas Arial_16
            // (glifo 107) -- o '•' de U+2022 nao estaria, e sairia quadradinho.
            //
            // Uma colecao de pilha, so para perguntar: o rascunho ainda nao esta na
            // composta de verdade, e escrever nela antes do Concluir tiraria do B o
            // que ele tem para desfazer.
            colecoes::Colecao rascunho;
            rascunho.id = 0;
            rascunho.tipo = (g_editandoComposta != NULL) ? g_editandoComposta->tipo
                                                         : colecoes::TIPO_UNIAO;
            rascunho.origens = g_origens;

            int quantos = 0;
            for (size_t j = 0; j < g_jogos.size(); j++)
                if (colecoes::Tem(&rascunho, g_jogos[j].titleId))
                    quantos++;

            WCHAR sub[64];
            swprintf_s(sub, 64, L"%d escolhidas · %d jogos",
                       (int)g_origens.size(), quantos);
            Rodape(GLYPH_A_BUTTON L" Marcar     " GLYPH_B_BUTTON L" Cancelar     "
                   GLYPH_START_BUTTON L" Concluir", sub);
        }
        else
            Rodape(GLYPH_A_BUTTON L" Abrir     " GLYPH_X_BUTTON L" Nova coleção     "
                   GLYPH_START_BUTTON L" Opções     " GLYPH_BACK_BUTTON L" Exportar", texto);
        g_fonte.End();
    }

    void IndiceAlfabetico(const std::vector<const biblioteca::Jogo *> &L)
    {
        if (L.empty()) return;

        bool tem[27];
        for (int i = 0; i < 27; i++) tem[i] = false;
        for (size_t i = 0; i < L.size(); i++)
        {
            char c = Inicial(L[i]->nome);
            tem[(c == '#') ? 26 : (c - 'A')] = true;
        }

        char atual = Inicial(L[g_iJogo]->nome);
        const float passo = (640.0f - 92.0f) / 27.0f;      // 20,3 px por letra

        // A fonte tem 27 px de altura e o passo e 20,3: no tamanho cheio a letra nao
        // cabe na propria faixa -- transborda da caixa verde e encosta nas vizinhas.
        // Reduzida, ela cabe, e aí a caixa pode ser exatamente a faixa.
        const float ESCALA = 0.72f;
        const float ALT_LETRA = 27.0f * ESCALA;

        g_fonte.SetScaleFactors(ESCALA, ESCALA);
        g_fonte.Begin();
        for (int i = 0; i < 27; i++)
        {
            float y = 92.0f + i * passo;
            bool acesa = ((i == 26) ? '#' : (char)('A' + i)) == atual;

            // Centra a letra na faixa, e a caixa É a faixa.
            float yLetra = y + (passo - ALT_LETRA) * 0.5f;

            if (acesa)
            {
                // Caixa ESCURA com a letra clara, nao o contrario. Letra escura sobre
                // verde forte fica com cara de negrito borrado nesse tamanho -- o
                // antisserrilhado engorda o traco contra o fundo claro.
                D3DRECT r;
                r.x1 = 1204; r.y1 = (LONG)y;
                r.x2 = 1232; r.y2 = (LONG)(y + passo);
                Preencher(r, D3DCOLOR_ARGB(190, 20, 34, 16));
                g_fonte.End();          // o retângulo trocou estado; refaz o lote
                g_fonte.Begin();
            }

            WCHAR letra[2] = { (WCHAR)((i == 26) ? L'#' : (L'A' + i)), L'\0' };
            g_fonte.DrawText(1218.0f, yLetra, acesa ? COR_ANEL
                             : (tem[i] ? COR_APAGADO : COR_LINHA), letra, ATGFONT_CENTER_X);
        }
        g_fonte.End();
        g_fonte.SetScaleFactors(1.0f, 1.0f);
    }

    void TelaJogos()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        const bool adicionando = (g_tela == TELA_ADICIONAR);
        // A grade dos soltos é esta mesma, com três diferenças: no lugar do
        // cabeçalho vai a barra de categorias (a pílula acesa já é o título, como na
        // grade de coleções), a frase de vazio é outra e o rodapé oferece arquivar.
        const bool soltos = (g_tela == TELA_SOLTOS);

        PilulaAba pilulas[biblioteca::ABA_QUANTAS + 1];
        int xRB = 0, nAbas = 0;
        if (soltos && g_abas.size() >= 2)
        {
            nAbas = MedirAbas(pilulas, biblioteca::ABA_QUANTAS + 1, &xRB);
            if (nAbas >= 2) AbasFundo(pilulas, nAbas, xRB);
        }
        const int total = (int)L.size();
        WCHAR texto[256], sub[64];

        if (total == 0)
        {
            g_fonte.Begin();
            if (nAbas >= 2) AbasTexto(pilulas, nAbas, xRB);
            g_fonte.SetScaleFactors(1.3f, 1.3f);
            // Composta vazia nao se resolve adicionando jogos -- nao ha o que
            // adicionar nela. O cruzamento vazio e o caso que mais vai acontecer,
            // entao ele diz o porque em vez de so dizer que esta vazio.
            const WCHAR *vazia = L"Nenhum jogo nesta coleção";
            if (!adicionando && g_atual != NULL)
            {
                if      (g_atual->tipo == colecoes::TIPO_INTERSECAO) vazia = L"Nenhum jogo está em todas elas";
                else if (g_atual->tipo == colecoes::TIPO_UNIAO)      vazia = L"Nenhum jogo nas coleções escolhidas";
            }

            // A ABA pode ter esvaziado a coleção por dentro -- tire o último jogo de
            // 360 de uma coleção que tem 40 de XBLA, na aba Xbox 360, e sobra isto.
            // A frase genérica estaria MENTINDO: a coleção não está vazia. O
            // subtítulo com a aba não serve de consolo aqui, porque este ramo volta
            // antes do Cabecalho -- então quem diz o porquê é a própria frase.
            WCHAR porCausaDaAba[128];
            if (!adicionando && g_aba != biblioteca::ABA_AUTOMATICA)
            {
                swprintf_s(porCausaDaAba, 128, L"Nenhum jogo de %s nesta coleção",
                           NomeDaAba(g_aba));
                vazia = porCausaDaAba;
            }

            if (soltos) vazia = L"Nenhum jogo fora das coleções";

            g_fonte.DrawText(640.0f, 320.0f, COR_APAGADO,
                             adicionando ? L"A biblioteca está vazia" : vazia,
                             ATGFONT_CENTER_X);
            g_fonte.SetScaleFactors(1.0f, 1.0f);
            Rodape(soltos
                   ? GLYPH_B_BUTTON L" Voltar"
                   : (adicionando
                   ? GLYPH_B_BUTTON L" Cancelar"
                   : (colecoes::Composta(g_atual)
                      ? GLYPH_B_BUTTON L" Voltar     " GLYPH_START_BUTTON L" Opções"
                      : GLYPH_B_BUTTON L" Voltar     " GLYPH_X_BUTTON L" Adicionar jogos")),
                   L"");
            g_fonte.End();
            return;
        }

        int base = g_primeiraLinha * COLUNAS;

        // As capas primeiro: o texto vai todo num lote depois, porque o Begin/End da
        // fonte salva e restaura estado de render e intercalar os dois embaralha o D3D.
        struct Nome { FLOAT x, y; int i; };
        Nome nomes[POR_PAGINA];
        int qtdNomes = 0;

        // Composição do que está sendo desenhado. O crash só aparece MARCANDO DURANTE O
        // CARREGAMENTO, e a combinação que só existe nesse caso é item marcado com a
        // capa ainda nula -- aí saem três DrawScreenSpaceRect seguidos no mesmo item.
        // Registrado só quando MUDA, senão seria uma linha por quadro.
        int marcadosVisiveis = 0, semCapaVisiveis = 0;

        for (int k = 0; k < POR_PAGINA; k++)
        {
            int i = base + k;
            if (i >= total) break;

            D3DRECT r;
            r.x1 = GRID_MARGEM + (k % COLUNAS) * (CAPA_L + ESPACO_X);
            r.y1 = TOPO + (k / COLUNAS) * (CAPA_A + ESPACO_Y);
            r.x2 = r.x1 + CAPA_L;
            r.y2 = r.y1 + CAPA_A;

            // A capa é pelo ContentItemId (a pasta de arte é GameData\<id em hex>);
            // a marcação é por TitleId, que é o que a coleção guarda.
            bool inteira = true;
            D3DTexture *capa = NoCache(L[i]->id, true, &inteira);
            bool marcado = !adicionando || SelecionadoNoRascunho(L[i]->titleId);

            if (capa != NULL)
            {
                // Do encarte da FreeStyle sai so a frente, de U 0,532 ate 1,0. A capa
                // de ROM vem pronta do PC, so com a arte, e vai inteira -- o mesmo
                // recorte aplicado nela comia a metade esquerda do desenho.
                const float u0 = inteira ? 0.0f : FRENTE_U0;
                ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch(
                    r, XMFLOAT2(u0, 0.0f), XMFLOAT2(1.0f, 0.0f),
                    XMFLOAT2(u0, 1.0f), capa);

                // Apagar é um véu por cima, não um desenho diferente: a variante
                // Colored fixa UV 0..1 lá dentro (AtgDebugDraw.cpp:662) e mostrava o
                // encarte INTEIRO -- contracapa e lombada espremidas no 5:7 -- justo
                // no jogo não marcado, que é o estado inicial de todos eles.
                if (!marcado)
                    Preencher(r, D3DCOLOR_ARGB(140, 6, 9, 8));
            }
            else
            {
                ATG::DebugDraw::DrawScreenSpaceRect(r, 1.0f, COR_FRACO);
            }

            if (g_anelMarcado && adicionando && SelecionadoNoRascunho(L[i]->titleId))
                ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, COR_ANEL);

            if (i == g_iJogo)
            {
                D3DRECT anel = r;
                anel.x1 -= 4; anel.y1 -= 4; anel.x2 += 4; anel.y2 += 4;
                ATG::DebugDraw::DrawScreenSpaceRect(anel, 3.0f, COR_ANEL);
            }

            if (marcado)   marcadosVisiveis++;
            if (capa == NULL) semCapaVisiveis++;

            nomes[qtdNomes].x = (FLOAT)r.x1;
            nomes[qtdNomes].y = (FLOAT)r.y2 + 8.0f;
            nomes[qtdNomes].i = i;
            qtdNomes++;
        }

        g_fonte.Begin();
        if (soltos)
        {
            // SEM CABEÇALHO: a barra ocupa o alto, e a pílula acesa já é o título --
            // a mesma razão de a grade de coleções não ter cabeçalho. Os dois no
            // mesmo lugar brigariam pelos mesmos 36 px.
            if (nAbas >= 2) AbasTexto(pilulas, nAbas, xRB);
        }
        else if (adicionando)
        {
            swprintf_s(sub, 64, L"%d de %d marcados", ItensMarcados(), (int)g_jogos.size());
            Cabecalho(L"Adicionar jogos", sub);
        }
        else
        {
            Larga(g_atual->nome, texto, 256);

            // Numa composta o subtítulo é a lista das origens; numa de jogos não há o
            // que dizer, e o cabeçalho fica como sempre foi.
            WCHAR formula[256];
            formula[0] = L'\0';
            const std::string f = Formula(g_atual);
            if (!f.empty()) Larga(f, formula, 256);

            // A ABA tem de aparecer aqui. Ela foi escolhida na tela anterior, e uma
            // coleção de 40 jogos mostrando 6 sem dizer por quê parece defeito. Entra
            // junto da fórmula, que responde a mesma pergunta pelo outro lado. O '·'
            // é U+00B7, que ESTÁ no atlas Arial_16 (glifo 107).
            if (g_aba != biblioteca::ABA_AUTOMATICA)
            {
                // A fórmula entra CORTADA em 199, e não inteira: swprintf_s com
                // buffer curto não trunca -- chama o invalid parameter handler, que
                // por padrão derruba o processo. Um colecoes.txt editado à mão com
                // muitas origens basta para chegar lá. 199 + 5 + 13 + 1 cabe nos 256.
                WCHAR curto[200], tudo[256];
                wcsncpy(curto, formula, 199);
                curto[199] = L'\0';
                swprintf_s(tudo, 256, L"%s%s%s", curto,
                           (curto[0] != L'\0') ? L"  ·  " : L"", NomeDaAba(g_aba));
                wcsncpy(formula, tudo, 255);
                formula[255] = L'\0';
            }
            Cabecalho(texto, formula);
        }

        for (int k = 0; k < qtdNomes; k++)
        {
            Larga(L[nomes[k].i]->nome, texto, 256);
            g_fonte.DrawText(nomes[k].x, nomes[k].y,
                             (nomes[k].i == g_iJogo) ? COR_TEXTO : COR_APAGADO,
                             texto, ATGFONT_TRUNCATED, (FLOAT)CAPA_L);
        }

        if (adicionando)
        {
            swprintf_s(texto, 256, L"%d marcados", ItensMarcados());
            Rodape(GLYPH_A_BUTTON L" Marcar     " GLYPH_B_BUTTON L" Cancelar     "
                   GLYPH_START_BUTTON L" Concluir", texto);
        }
        else if (soltos)
        {
            swprintf_s(texto, 256, L"%d de %d", g_iJogo + 1, total);
            // O B volta para Todos. Nas outras abas ele não faz nada, mas esta é uma
            // TELA, e sair dela por onde se sai de toda tela é mais barato que
            // lembrar do LB.
            Rodape(GLYPH_A_BUTTON L" Jogar     " GLYPH_B_BUTTON L" Voltar     "
                   GLYPH_X_BUTTON L" Adicionar a coleções", texto);
        }
        else
        {
            swprintf_s(texto, 256, L"%d de %d", g_iJogo + 1, total);
            // Sem "Adicionar jogos" numa composta: o jogo está ali por causa de uma
            // origem, e para tirá-lo tira-se da origem. Oferecer o botão seria
            // oferecer um que não faz nada.
            Rodape(colecoes::Composta(g_atual)
                   ? GLYPH_A_BUTTON L" Jogar     " GLYPH_B_BUTTON L" Voltar     "
                     GLYPH_START_BUTTON L" Opções"
                   : GLYPH_A_BUTTON L" Jogar     " GLYPH_B_BUTTON L" Voltar     "
                     GLYPH_X_BUTTON L" Adicionar jogos     " GLYPH_START_BUTTON L" Opções",
                   texto);
        }
        g_fonte.End();

        IndiceAlfabetico(L);

        static int ultMarcados = -1, ultSemCapa = -1, ultBase = -1;
        if (marcadosVisiveis != ultMarcados || semCapaVisiveis != ultSemCapa || base != ultBase)
        {
            ultMarcados = marcadosVisiveis; ultSemCapa = semCapaVisiveis; ultBase = base;
            diario::Detalhe("tela: base=%d visiveis=%d marcados=%d semCapa=%d",
                            base, qtdNomes, marcadosVisiveis, semCapaVisiveis);
        }
    }

    // ---- o modal ---------------------------------------------------------------
    //
    // Uma funcao so para os cinco casos do MenuDe. O desenho saiu do estudo em
    // previa/modais.html: rotulo pequeno, nome, regua, linhas de borda a borda e o
    // rodape dentro da caixa.
    //
    // O veu e mais leve que o antigo (172 contra 200): este painel nao cobre a tela
    // inteira, e ver a grade por tras e metade da ideia.
    //
    // A linha em foco e NEUTRA, faixa branca a 9% com a barra verde. A destrutiva nao
    // tem cor propria parada -- so fica vermelha quando esta em foco.
    const int MOD_PAD = 26, MOD_ALT = 48, MOD_CABECA = 128, MOD_PE = 80;
    const int MOD_MIN = 460, MOD_MAX = 760;
    const D3DCOLOR COR_VEU       = D3DCOLOR_ARGB(172, 6, 9, 8);
    const D3DCOLOR COR_FAIXA     = D3DCOLOR_ARGB( 23, 233, 240, 235);   // branco a 9%
    const D3DCOLOR COR_PERIGO    = D3DCOLOR_XRGB(201, 66, 58);
    const D3DCOLOR COR_FAIXA_MAL = D3DCOLOR_ARGB( 56, 201, 66, 58);     // vermelho a 22%
    const D3DCOLOR COR_TEXTO_MAL = D3DCOLOR_XRGB(255, 226, 223);

    const WCHAR *RODAPE_MODAL = GLYPH_A_BUTTON L" Escolher     " GLYPH_B_BUTTON L" Fechar";

    // Largura pelo CONTEUDO, e nao os 460 fixos de antes: com um item so sobrava caixa
    // vazia, e um nome de jogo comprido era cortado. GetTextExtent e CPU pura (le a
    // tabela de glifos), entao pode ser chamado fora do Begin/End.
    int LarguraDoModal(const WCHAR *titulo)
    {
        FLOAT w = 0.0f, h = 0.0f;
        WCHAR largo[256];

        // O GetTextExtent multiplica pelo m_fXScaleFactor (AtgFont.cpp:550). Todo
        // mundo devolve a escala a 1,0 antes de sair, mas a largura do painel inteiro
        // depende disto: medir a 1,3 por engano grudaria o painel no teto de 760 px
        // em todos os casos.
        g_fonte.SetScaleFactors(1.0f, 1.0f);

        g_fonte.GetTextExtent(titulo, &w, &h);
        FLOAT maior = w * 1.3f;                 // o titulo e desenhado a 1,3x

        for (int i = 0; i < g_menuQtd; i++)
        {
            Larga(g_menuItens[i], largo, 256);
            g_fonte.GetTextExtent(largo, &w, &h);
            if (w > maior) maior = w;
        }
        g_fonte.GetTextExtent(RODAPE_MODAL, &w, &h);
        if (w > maior) maior = w;

        int L = (int)maior + (MOD_PAD + 18) * 2;
        if (L < MOD_MIN) L = MOD_MIN;
        if (L > MOD_MAX) L = MOD_MAX;
        return L;
    }

    void DesenharMenu()
    {
        WCHAR titulo[256], rotulo[64], texto[256];
        Larga(g_menuTitulo, titulo, 256);
        Larga(g_menuRotulo, rotulo, 64);
        ParaMaiusculas(rotulo);

        const int L = LarguraDoModal(titulo);
        const int A = MOD_CABECA + g_menuQtd * MOD_ALT + MOD_PE;
        const int x = (1280 - L) / 2, y = (720 - A) / 2;

        D3DRECT r;
        r.x1 = 0; r.y1 = 0; r.x2 = 1280; r.y2 = 720;
        Preencher(r, COR_VEU);

        PainelRedondo(x, y, L, A, COR_PAINEL, COR_LINHA);

        // As duas reguas: uma sob o cabecalho, outra sobre o rodape.
        r.x1 = x + MOD_PAD; r.x2 = x + L - MOD_PAD;
        r.y1 = y + 108;     r.y2 = r.y1 + 1;         Preencher(r, COR_LINHA);
        r.y1 = y + A - 56;  r.y2 = r.y1 + 1;         Preencher(r, COR_LINHA);

        // A faixa de foco vai de borda a borda. Nao encosta em canto nenhum: a
        // primeira linha comeca 128 px abaixo do topo e a ultima termina 80 px acima
        // do fim, contra os 16 px de raio -- por isso ela pode ser retangulo cheio.
        if (g_menuFoco >= 0 && g_menuFoco < g_menuQtd)
        {
            const bool mau = (g_menuFoco == g_menuPerigo);
            const int ly = y + MOD_CABECA + g_menuFoco * MOD_ALT;

            r.x1 = x + 1; r.x2 = x + L - 1;
            r.y1 = ly;    r.y2 = ly + MOD_ALT;
            Preencher(r, mau ? COR_FAIXA_MAL : COR_FAIXA);

            r.x2 = x + 5;
            Preencher(r, mau ? COR_PERIGO : COR_ANEL);
        }

        g_fonte.Begin();

        g_fonte.SetScaleFactors(0.72f, 0.72f);
        g_fonte.DrawText((FLOAT)x + MOD_PAD, (FLOAT)y + 26.0f, COR_FRACO, rotulo, 0);

        g_fonte.SetScaleFactors(1.3f, 1.3f);
        g_fonte.DrawText((FLOAT)x + MOD_PAD, (FLOAT)y + 48.0f, COR_TEXTO, titulo,
                         ATGFONT_TRUNCATED, (FLOAT)(L - MOD_PAD * 2));
        g_fonte.SetScaleFactors(1.0f, 1.0f);

        for (int i = 0; i < g_menuQtd; i++)
        {
            const bool mau = (i == g_menuPerigo);
            // Parada, a destrutiva e uma linha como as outras.
            const D3DCOLOR cor = (i != g_menuFoco) ? COR_APAGADO
                                                   : (mau ? COR_TEXTO_MAL : COR_TEXTO);
            Larga(g_menuItens[i], texto, 256);
            g_fonte.DrawText((FLOAT)x + MOD_PAD,
                             (FLOAT)(y + MOD_CABECA + i * MOD_ALT) + (MOD_ALT - 27) / 2.0f,
                             cor, texto, ATGFONT_TRUNCATED, (FLOAT)(L - MOD_PAD * 2));
        }

        // Dentro da caixa. No rodape da tela ficaria por cima do rodape que a tela de
        // baixo continua desenhando.
        g_fonte.DrawText((FLOAT)x + MOD_PAD, (FLOAT)y + (FLOAT)A - 42.0f, COR_FRACO,
                         RODAPE_MODAL, 0);
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

    void Desenhar()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->Clear(0, NULL, D3DCLEAR_TARGET, COR_FUNDO, 1.0f, 0);

        // Fundo em gradiente, na diagonal. Nao e imagem: e cor por vertice, entao custa
        // um retangulo e nenhum arquivo -- e nao tem como ser cortado por overscan.
        Gradiente(0.0f, 0.0f, 1280.0f, 720.0f,
                  D3DCOLOR_XRGB(FUNDO_DE[0],   FUNDO_DE[1],   FUNDO_DE[2]),
                  D3DCOLOR_XRGB(FUNDO_PARA[0], FUNDO_PARA[1], FUNDO_PARA[2]), true);

        // TELA_ARQUIVAR é uma grade de coleções; TELA_SOLTOS é uma grade de capas.
        if (g_tela == TELA_COLECOES || g_tela == TELA_ORIGENS ||
            g_tela == TELA_ARQUIVAR)                           TelaColecoes();
        else                                                   TelaJogos();

        if (g_menuAberto)
            DesenharMenu();

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
            Avisar("Este jogo nao tem TitleId e nao pode entrar numa colecao");
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
        if (teclado::Abrir("Nova coleção", "Como se chama?", ""))
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
        if (teclado::Abrir("Renomear", "Novo nome", g_renomeando->nome.c_str()))
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
            g_menuRotulo = "Nova coleção";
            g_menuItens[0] = "Coleção Manual";
            g_menuItens[1] = "Junção";
            g_menuItens[2] = "Interseção";
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
        g_menuRotulo = "Coleção";
        CopiarTitulo(L[g_iCol]->nome);

        g_menuItens[g_menuQtd] = "Renomear";
        g_menuAcao [g_menuQtd] = ACAO_RENOMEAR;  g_menuQtd++;

        // Composta ganha a opção de trocar as origens; coleção de jogos não tem o que
        // escolher ali.
        if (colecoes::Composta(L[g_iCol]))
        {
            g_menuItens[g_menuQtd] = "Escolher coleções";
            g_menuAcao [g_menuQtd] = ACAO_ORIGENS;   g_menuQtd++;
        }

        g_menuPerigo = g_menuQtd;                 // a destrutiva é sempre a última
        g_menuItens[g_menuQtd] = "Apagar coleção";
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
        g_menuRotulo = "Exportar";
        CopiarTitulo("Biblioteca e coleções para o Xbox Vault");
        g_menuItens[0] = "Exportar";
        g_menuItens[1] = "Cancelar";
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
        g_menuRotulo = "Jogo";

        // A coleção guarda TitleId, então remover tira TODOS os itens que o
        // compartilham -- os dois discos de um multi-disco, as duas cópias de uma
        // instalação duplicada. Dizer isso é mais barato que surpreender.
        int quantos = ItensComTitleId(L[g_iJogo]->titleId);
        if (quantos > 1)
            _snprintf(g_menuItemBuf, sizeof(g_menuItemBuf),
                      "Remover da coleção (%d itens)", quantos);
        else
            _snprintf(g_menuItemBuf, sizeof(g_menuItemBuf), "Remover da coleção");
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
                    Avisar("vault.txt gravado");
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
            // Segundo menu, com "Cancelar" em foco. Apagar e a unica acao do app que
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
                    g_menuRotulo = "Apagar";
                    g_menuItens[0] = "Cancelar";
                    g_menuItens[1] = "Apagar esta coleção";
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
                Avisar("Este jogo nao tem TitleId e nao pode entrar numa colecao");
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
            // Volta para o Todos, que é de onde se veio.
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
        Avisar("Nao consegui ler a biblioteca. A FreeStyle esta varrendo? Tente depois.");

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
