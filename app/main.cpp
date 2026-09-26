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
#include "diario.h"
#include "biblioteca.h"
#include "colecoes.h"
#include "dispositivos.h"
#include "config.h"
#include "carregador.h"
#include "teclado.h"
#include "fsda.h"
#include "lancador.h"
#include "AtgDevice.h"
#include "AtgFont.h"
#include "AtgDebugDraw.h"
#include "AtgSimpleShaders.h"

// A ATG espera este ponteiro global; normalmente quem o define é o AtgApp.cpp, que não
// usamos. Atenção ao tipo: dentro do namespace, D3DDevice é o ATG::D3DDevice do
// AtgDevice.h, que herda do global. Definir com o tipo global compila e falha no link.
namespace ATG { D3DDevice *g_pd3dDevice = NULL; }

namespace
{
    // ---- geometria, em pixels de 1280x720 --------------------------------------
    const int COLUNAS    = 5;
    const int LINHAS     = 2;          // visíveis por vez; o resto rola
    const int CAPA_L     = 170;
    const int CAPA_A     = 238;        // 5:7, a proporção da capa de 360
    const int ESPACO_X   = 56;
    const int ESPACO_Y   = 40;
    const int MARGEM_X   = 100;
    const int TOPO       = 92;
    const int POR_PAGINA = COLUNAS * LINHAS;

    const int COL_LADO = 180, COL_GAP = 30;
    const int COL_POR_LINHA = 5, COL_LINHAS = 2, COL_TOPO = 64;
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
    const int CACHE_MAX = 128;

    struct Entrada
    {
        int         jogoId;
        D3DTexture *textura;
        DWORD       uso;      // relógio lógico, para descartar o usado há mais tempo
    };

    Entrada g_cache[CACHE_MAX];
    DWORD   g_relogio = 0;
    std::vector<int> g_emVoo;
    std::vector<int> g_falhou;
    const int EM_VOO_MAX = 6;

    ATG::Font g_fonte;

    // ---- estado ----------------------------------------------------------------
    enum Tela { TELA_COLECOES, TELA_JOGOS, TELA_ADICIONAR };

    std::string                   g_caminhoBanco;
    std::vector<biblioteca::Jogo> g_jogos;

    Tela g_tela = TELA_COLECOES;
    colecoes::Colecao *g_atual = NULL;
    std::vector<int>   g_selecao;      // rascunho ao adicionar; ver AbrirAdicionar
    int g_iCol = 0, g_iJogo = 0, g_primeiraLinha = 0;

    // De qual menu se trata. Despachar pelo g_tela dava errado: ☰ na tela de jogos
    // abria o menu do jogo, mas EscolherNoMenu lia g_tela de novo -- e se a ação
    // anterior do mesmo quadro tivesse mudado de tela, executava o item do outro menu.
    enum MenuDe { MENU_COLECAO, MENU_JOGO };

    bool   g_menuAberto = false;
    MenuDe g_menuDe = MENU_COLECAO;
    int    g_menuFoco = 0, g_menuQtd = 0;
    const char *g_menuItens[4];
    char   g_menuTitulo[128] = "";   // CÓPIA: o c_str() de uma coleção apagada morre

    bool g_bloqueou = false;         // uma tela do sistema parou o laço neste quadro

    char  g_aviso[192] = "";         // erro mostrado por alguns segundos
    DWORD g_avisoAte = 0;

    // ---- texto -----------------------------------------------------------------
    // O SQLite devolve UTF-8. Converter com CP_ACP quebra o que não for ASCII: o
    // "BLAZBLUE　CONTINUUM SHIFT" tem um espaço ideográfico japonês (três bytes)
    // que virava lixo. E acima de 0x100 ficam os GLIFOS DE BOTÃO da fonte, então um
    // caractere japonês que passasse desenharia um botão no meio do nome.
    void Larga(const std::string &origem, WCHAR *destino, int capacidade)
    {
        if (MultiByteToWideChar(CP_UTF8, 0, origem.c_str(), -1, destino, capacidade) <= 0)
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
    std::vector<const biblioteca::Jogo *> ListaAtual()
    {
        std::vector<const biblioteca::Jogo *> saida;

        if (g_tela == TELA_ADICIONAR)
        {
            for (size_t i = 0; i < g_jogos.size(); i++)
                saida.push_back(&g_jogos[i]);
        }
        else if (g_atual != NULL)
        {
            for (size_t i = 0; i < g_jogos.size(); i++)
                if (colecoes::Tem(g_atual, g_jogos[i].id))
                    saida.push_back(&g_jogos[i]);
        }
        return saida;      // g_jogos já vem ordenada de biblioteca::Ler
    }

    bool SelecionadoNoRascunho(int id)
    {
        for (size_t i = 0; i < g_selecao.size(); i++)
            if (g_selecao[i] == id)
                return true;
        return false;
    }

    // ---- cache -----------------------------------------------------------------
    D3DTexture *NoCache(int jogoId, bool marcarUso)
    {
        for (int i = 0; i < CACHE_MAX; i++)
        {
            if (g_cache[i].textura != NULL && g_cache[i].jogoId == jogoId)
            {
                if (marcarUso)
                    g_cache[i].uso = ++g_relogio;
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

    void Guardar(int jogoId, D3DTexture *textura)
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
    }

    void PedirOQueFalta()
    {
        if (g_tela == TELA_COLECOES)
            return;

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

                std::string pasta = biblioteca::PastaArte(g_caminhoBanco, id);
                if (pasta.empty())
                    continue;

                char arquivo[512];
                _snprintf(arquivo, sizeof(arquivo), "%s\\%08X.assets", pasta.c_str(), id);
                arquivo[sizeof(arquivo) - 1] = '\0';
                carregador::Pedir(id, arquivo);
                g_emVoo.push_back(id);
            }
        }
    }

    void RecolherCarregadas()
    {
        int jogoId;
        std::vector<unsigned char> bytes;

        // UMA por quadro. Mesmo a 3 ms, cinco de uma vez dariam um solavanco.
        if (!carregador::Retirar(&jogoId, bytes))
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

        D3DFORMAT formato = D3DFMT_UNKNOWN;
        if (bytes.size() > 88)
        {
            if (memcmp(&bytes[84], "DXT5", 4) == 0)      formato = D3DFMT_DXT5;
            else if (memcmp(&bytes[84], "DXT1", 4) == 0) formato = D3DFMT_DXT1;
        }

        // A versão Ex com estes parâmetros é o que o FreeStyle faz. A versão sem Ex usa
        // D3DX_DEFAULT em tudo: redimensiona para potência de 2 com filtragem e gera a
        // cadeia inteira de mipmaps. Era o que travava a cada linha nova.
        D3DTexture *textura = NULL;
        HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
            ATG::g_pd3dDevice, &bytes[0], (UINT)bytes.size(),
            D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2,
            1,                               // um mipmap só
            D3DUSAGE_CPU_CACHED_MEMORY, formato, D3DPOOL_DEFAULT,
            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
            0, NULL, NULL, &textura);

        if (SUCCEEDED(hr) && textura != NULL)
            Guardar(jogoId, textura);
        else
            g_falhou.push_back(jogoId);
    }

    // ---- desenho ---------------------------------------------------------------
    void Cabecalho(const WCHAR *titulo, const WCHAR *sub)
    {
        g_fonte.SetScaleFactors(1.5f, 1.5f);
        g_fonte.DrawText((FLOAT)MARGEM_X, 34.0f, COR_TEXTO, titulo, 0);
        g_fonte.SetScaleFactors(1.0f, 1.0f);
        if (sub != NULL && sub[0] != L'\0')
            g_fonte.DrawText((FLOAT)MARGEM_X + 260.0f, 46.0f, COR_FRACO, sub, 0);
    }

    void Rodape(const WCHAR *esquerda, const WCHAR *direita)
    {
        g_fonte.DrawText((FLOAT)MARGEM_X, 656.0f, COR_APAGADO, esquerda, 0);
        if (direita != NULL && direita[0] != L'\0')
            g_fonte.DrawText(1180.0f, 656.0f, COR_FRACO, direita, ATGFONT_RIGHT);
    }

    void Caixa(int x, int y, int l, int a, bool focada)
    {
        D3DRECT r;
        r.x1 = x; r.y1 = y; r.x2 = x + l; r.y2 = y + a;
        ATG::DebugDraw::DrawScreenSpaceTexturedRectColored(r, NULL, COR_PAINEL);
        ATG::DebugDraw::DrawScreenSpaceRect(r, focada ? 3.0f : 1.0f,
                                            focada ? COR_ANEL : COR_LINHA);
    }

    void TelaColecoes()
    {
        std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
        WCHAR texto[256];

        if (L.empty())
        {
            g_fonte.Begin();
            g_fonte.SetScaleFactors(1.3f, 1.3f);
            g_fonte.DrawText(640.0f, 320.0f, COR_APAGADO, L"Nenhuma coleção ainda",
                             ATGFONT_CENTER_X);
            g_fonte.SetScaleFactors(1.0f, 1.0f);
            Rodape(GLYPH_X_BUTTON L" Nova coleção", L"");
            g_fonte.End();
            return;
        }

        int paginaInicio = (g_iCol / COL_POR_PAGINA) * COL_POR_PAGINA;

        for (int k = 0; k < COL_POR_PAGINA; k++)
        {
            int i = paginaInicio + k;
            if (i >= (int)L.size()) break;

            int x = MARGEM_X + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
            int y = COL_TOPO + (k / COL_POR_LINHA) * (COL_LADO + COL_GAP);
            Caixa(x, y, COL_LADO, COL_LADO, i == g_iCol);
        }

        g_fonte.Begin();
        for (int k = 0; k < COL_POR_PAGINA; k++)
        {
            int i = paginaInicio + k;
            if (i >= (int)L.size()) break;

            int x = MARGEM_X + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
            int y = COL_TOPO + (k / COL_POR_LINHA) * (COL_LADO + COL_GAP);

            Larga(L[i]->nome, texto, 256);
            g_fonte.DrawText((FLOAT)x + 14.0f, (FLOAT)y + COL_LADO - 56.0f,
                             (i == g_iCol) ? COR_TEXTO : COR_APAGADO,
                             texto, ATGFONT_TRUNCATED, (FLOAT)COL_LADO - 28.0f);

            if (L[i]->ids.empty())
                swprintf_s(texto, 256, L"vazia");
            else
                swprintf_s(texto, 256, L"%d jogos", (int)L[i]->ids.size());
            g_fonte.DrawText((FLOAT)x + 14.0f, (FLOAT)y + COL_LADO - 30.0f,
                             COR_FRACO, texto, 0);
        }

        swprintf_s(texto, 256, L"%d de %d", g_iCol + 1, (int)L.size());
        Rodape(GLYPH_A_BUTTON L" Abrir     " GLYPH_X_BUTTON L" Nova coleção     "
               GLYPH_START_BUTTON L" Opções", texto);
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
        float passo = (640.0f - 92.0f) / 27.0f;

        g_fonte.Begin();
        for (int i = 0; i < 27; i++)
        {
            float y = 92.0f + i * passo;
            bool acesa = ((i == 26) ? '#' : (char)('A' + i)) == atual;

            if (acesa)
            {
                D3DRECT r;
                r.x1 = 1204; r.y1 = (LONG)y - 1;
                r.x2 = 1232; r.y2 = (LONG)(y + passo);
                ATG::DebugDraw::DrawScreenSpaceTexturedRectColored(r, NULL, COR_ANEL);
                g_fonte.End();          // o retângulo trocou estado; refaz o lote
                g_fonte.Begin();
            }

            WCHAR letra[2] = { (WCHAR)((i == 26) ? L'#' : (L'A' + i)), L'\0' };
            g_fonte.DrawText(1218.0f, y, acesa ? COR_FUNDO
                             : (tem[i] ? COR_APAGADO : COR_LINHA), letra, ATGFONT_CENTER_X);
        }
        g_fonte.End();
    }

    void TelaJogos()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        const bool adicionando = (g_tela == TELA_ADICIONAR);
        const int total = (int)L.size();
        WCHAR texto[256], sub[64];

        if (total == 0)
        {
            g_fonte.Begin();
            g_fonte.SetScaleFactors(1.3f, 1.3f);
            g_fonte.DrawText(640.0f, 320.0f, COR_APAGADO,
                             adicionando ? L"A biblioteca está vazia"
                                         : L"Nenhum jogo nesta coleção",
                             ATGFONT_CENTER_X);
            g_fonte.SetScaleFactors(1.0f, 1.0f);
            Rodape(adicionando
                   ? GLYPH_B_BUTTON L" Cancelar"
                   : GLYPH_B_BUTTON L" Voltar     " GLYPH_X_BUTTON L" Adicionar jogos",
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

        for (int k = 0; k < POR_PAGINA; k++)
        {
            int i = base + k;
            if (i >= total) break;

            D3DRECT r;
            r.x1 = MARGEM_X + (k % COLUNAS) * (CAPA_L + ESPACO_X);
            r.y1 = TOPO + (k / COLUNAS) * (CAPA_A + ESPACO_Y);
            r.x2 = r.x1 + CAPA_L;
            r.y2 = r.y1 + CAPA_A;

            D3DTexture *capa = NoCache(L[i]->id, true);
            bool marcado = !adicionando || SelecionadoNoRascunho(L[i]->id);

            if (capa != NULL)
            {
                // Só a frente do encarte: amostra de U 0,532 até 1,0.
                if (marcado)
                    ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch(
                        r, XMFLOAT2(FRENTE_U0, 0.0f), XMFLOAT2(1.0f, 0.0f),
                        XMFLOAT2(FRENTE_U0, 1.0f), capa);
                else
                    ATG::DebugDraw::DrawScreenSpaceTexturedRectColored(
                        r, capa, D3DCOLOR_ARGB(150, 255, 255, 255));
            }
            else
            {
                ATG::DebugDraw::DrawScreenSpaceRect(r, 1.0f, COR_FRACO);
            }

            if (adicionando && SelecionadoNoRascunho(L[i]->id))
                ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, COR_ANEL);

            if (i == g_iJogo)
            {
                D3DRECT anel = r;
                anel.x1 -= 4; anel.y1 -= 4; anel.x2 += 4; anel.y2 += 4;
                ATG::DebugDraw::DrawScreenSpaceRect(anel, 3.0f, COR_ANEL);
            }

            nomes[qtdNomes].x = (FLOAT)r.x1;
            nomes[qtdNomes].y = (FLOAT)r.y2 + 8.0f;
            nomes[qtdNomes].i = i;
            qtdNomes++;
        }

        g_fonte.Begin();
        if (adicionando)
        {
            swprintf_s(sub, 64, L"%d de %d marcados", (int)g_selecao.size(), (int)g_jogos.size());
            Cabecalho(L"Adicionar jogos", sub);
        }
        else
        {
            Larga(g_atual->nome, texto, 256);
            Cabecalho(texto, L"");
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
            swprintf_s(texto, 256, L"%d marcados", (int)g_selecao.size());
            Rodape(GLYPH_A_BUTTON L" Marcar     " GLYPH_B_BUTTON L" Cancelar     "
                   GLYPH_START_BUTTON L" Concluir", texto);
        }
        else
        {
            swprintf_s(texto, 256, L"%d de %d", g_iJogo + 1, total);
            Rodape(GLYPH_A_BUTTON L" Jogar     " GLYPH_B_BUTTON L" Voltar     "
                   GLYPH_X_BUTTON L" Adicionar jogos     " GLYPH_START_BUTTON L" Opções", texto);
        }
        g_fonte.End();

        IndiceAlfabetico(L);
    }

    void DesenharMenu()
    {
        const int L = 460, A = 94 + g_menuQtd * 52;   // 34 a mais: a linha de botões
        const int x = (1280 - L) / 2, y = (720 - A) / 2;

        D3DRECT fundo;
        fundo.x1 = 0; fundo.y1 = 0; fundo.x2 = 1280; fundo.y2 = 720;
        ATG::DebugDraw::DrawScreenSpaceTexturedRectColored(fundo, NULL,
                                                           D3DCOLOR_ARGB(200, 6, 9, 8));
        Caixa(x, y, L, A, false);

        for (int i = 0; i < g_menuQtd; i++)
            if (i == g_menuFoco)
            {
                D3DRECT r;
                r.x1 = x + 14; r.y1 = y + 46 + i * 52;
                r.x2 = x + L - 14; r.y2 = r.y1 + 42;
                ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, COR_ANEL);
            }

        WCHAR texto[256];
        g_fonte.Begin();
        Larga(g_menuTitulo, texto, 256);
        g_fonte.DrawText((FLOAT)x + 20.0f, (FLOAT)y + 14.0f, COR_TEXTO,
                         texto, ATGFONT_TRUNCATED, (FLOAT)L - 40.0f);

        for (int i = 0; i < g_menuQtd; i++)
        {
            Larga(g_menuItens[i], texto, 256);
            g_fonte.DrawText((FLOAT)x + 30.0f, (FLOAT)y + 56.0f + i * 52.0f,
                             (i == g_menuFoco) ? COR_TEXTO : COR_APAGADO, texto, 0);
        }
        // Dentro da caixa. No rodapé da tela ficaria por cima do rodapé que a tela
        // de baixo continua desenhando.
        g_fonte.DrawText((FLOAT)x + 20.0f, (FLOAT)y + (FLOAT)A - 34.0f, COR_APAGADO,
                         GLYPH_A_BUTTON L" Escolher     " GLYPH_B_BUTTON L" Fechar", 0);
        g_fonte.End();
    }

    void Avisar(const char *texto)
    {
        _snprintf(g_aviso, sizeof(g_aviso), "%s", texto);
        g_aviso[sizeof(g_aviso) - 1] = '\0';
        g_avisoAte = GetTickCount() + 6000;
    }

    void DesenharAviso()
    {
        const int L = 760, A = 74;
        const int x = (1280 - L) / 2, y = 560;

        Caixa(x, y, L, A, true);

        WCHAR texto[256];
        Larga(g_aviso, texto, 256);
        g_fonte.Begin();
        g_fonte.DrawText(640.0f, (FLOAT)y + 24.0f, COR_TEXTO, texto,
                         ATGFONT_CENTER_X | ATGFONT_TRUNCATED, (FLOAT)L - 40.0f);
        g_fonte.End();
    }

    void Desenhar()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->Clear(0, NULL, D3DCLEAR_TARGET, COR_FUNDO, 1.0f, 0);

        if (g_tela == TELA_COLECOES) TelaColecoes();
        else                          TelaJogos();

        if (g_menuAberto)
            DesenharMenu();

        if (g_aviso[0] != '\0' && GetTickCount() < g_avisoAte)
            DesenharAviso();

        d->Present(NULL, NULL, NULL, NULL);
    }

    // ---- navegação -------------------------------------------------------------
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
        SeguirFoco();
    }

    // O foco é corrigido AQUI, uma vez por quadro, e não no desenho: a tela de
    // coleções não é desenhada enquanto estamos nos jogos, então um clamp que só
    // rodava ao desenhar deixava g_iCol fora de faixa para quem lesse antes.
    void ClampFoco()
    {
        int nCol = (int)colecoes::Ordenadas().size();
        if (g_iCol >= nCol) g_iCol = nCol - 1;
        if (g_iCol < 0)     g_iCol = 0;

        int nJogos = (int)ListaAtual().size();
        if (g_iJogo >= nJogos) g_iJogo = nJogos - 1;
        if (g_iJogo < 0)       g_iJogo = 0;
        SeguirFoco();
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
        g_iJogo = alvo;
        SeguirFoco();
    }

    void MoverColecao(int delta)
    {
        int total = (int)colecoes::Ordenadas().size();
        if (total == 0) return;

        int novo = g_iCol + delta;
        if (delta == COL_POR_LINHA && novo >= total && g_iCol < total - 1)
            novo = total - 1;

        if (novo >= 0 && novo < total) g_iCol = novo;
    }

    void AbrirColecao()
    {
        std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
        if (L.empty()) return;
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
        }
        g_selecao.clear();
        g_tela = TELA_JOGOS;
        g_iJogo = 0; g_primeiraLinha = 0;
        carregador::DescartarPendentes();
        g_emVoo.clear();
    }

    void NovaColecao()
    {
        std::string nome;
        bool ok = teclado::Pedir("Nova coleção", "Como se chama?", "", nome);
        g_bloqueou = true;              // o teclado segurou o laço; ver o fim do main
        if (!ok) return;

        // Comparar por PONTEIRO, não por nome: dois nomes iguais focariam a errada.
        colecoes::Colecao *nova = colecoes::Criar(nome);
        std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
        for (size_t i = 0; i < L.size(); i++)
            if (L[i] == nova) { g_iCol = (int)i; break; }

        diario::Escrever("colecao criada: %s", nova->nome.c_str());
    }

    void Renomear()
    {
        std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
        if (L.empty()) return;

        colecoes::Colecao *alvo = L[g_iCol];

        std::string nome;
        bool ok = teclado::Pedir("Renomear", "Novo nome", alvo->nome.c_str(), nome);
        g_bloqueou = true;
        if (!ok) return;

        alvo->nome = colecoes::Sanear(nome);
        colecoes::Gravar();

        // A lista sai ordenada por nome: renomear muda o lugar. Sem reachar, o foco
        // fica sobre outra coleção.
        std::vector<colecoes::Colecao *> depois = colecoes::Ordenadas();
        for (size_t i = 0; i < depois.size(); i++)
            if (depois[i] == alvo) { g_iCol = (int)i; break; }
    }

    void CopiarTitulo(const std::string &origem)
    {
        strncpy(g_menuTitulo, origem.c_str(), sizeof(g_menuTitulo) - 1);
        g_menuTitulo[sizeof(g_menuTitulo) - 1] = '\0';
    }

    void AbrirMenuColecao()
    {
        std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
        if (L.empty()) return;

        CopiarTitulo(L[g_iCol]->nome);
        g_menuDe = MENU_COLECAO;
        g_menuItens[0] = "Renomear";
        g_menuItens[1] = "Apagar coleção";
        g_menuQtd = 2; g_menuFoco = 0; g_menuAberto = true;
    }

    void AbrirMenuJogo()
    {
        std::vector<const biblioteca::Jogo *> L = ListaAtual();
        if (L.empty()) return;
        CopiarTitulo(L[g_iJogo]->nome);
        g_menuDe = MENU_JOGO;
        g_menuItens[0] = "Remover da coleção";
        g_menuQtd = 1; g_menuFoco = 0; g_menuAberto = true;
    }

    void EscolherNoMenu()
    {
        g_menuAberto = false;

        if (g_menuDe == MENU_COLECAO)
        {
            if (g_menuFoco == 0) Renomear();
            else
            {
                std::vector<colecoes::Colecao *> L = colecoes::Ordenadas();
                if (!L.empty())
                {
                    colecoes::Apagar(L[g_iCol]);
                    if (g_iCol > 0) g_iCol--;
                }
            }
        }
        else
        {
            std::vector<const biblioteca::Jogo *> L = ListaAtual();
            if (!L.empty())
            {
                colecoes::Remover(g_atual, L[g_iJogo]->id);
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
        SoltarTudo();

        std::string erro;
        if (lancador::Lancar(*j, erro))
            return;                     // nunca acontece: sucesso não devolve

        // Voltamos vivos, então falhou. O app continua usável: remonta o carregador e
        // diz na tela o que houve, em vez de ficar mudo com a grade vazia.
        Avisar(erro.c_str());
        carregador::Iniciar();
    }

    void Confirmar()
    {
        if (g_tela == TELA_COLECOES) AbrirColecao();
        else if (g_tela == TELA_ADICIONAR)
        {
            std::vector<const biblioteca::Jogo *> L = ListaAtual();
            if (L.empty()) return;

            int id = L[g_iJogo]->id;
            for (size_t i = 0; i < g_selecao.size(); i++)
            {
                if (g_selecao[i] == id) { g_selecao.erase(g_selecao.begin() + i); return; }
            }
            g_selecao.push_back(id);
        }
        else Jogar();
    }

    void Voltar()
    {
        if (g_tela == TELA_ADICIONAR) FecharAdicionar(false);
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

void __cdecl main()
{
    diario::Abrir("game:\\collectionui.log");
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
        diario::Escrever("AVISO: fonte nao carregou");

    // Um título só enxerga "game:" por padrão; sem montar, o HD não existe para nós.
    diario::Escrever("montando dispositivos");
    dispositivos::MontarTodos();

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

    if (!g_caminhoBanco.empty())
        biblioteca::Ler(g_caminhoBanco.c_str(), g_jogos);
    diario::Escrever("biblioteca: %d jogos", (int)g_jogos.size());

    colecoes::Carregar();
    carregador::Iniciar();

    XINPUT_STATE anterior;
    ZeroMemory(&anterior, sizeof(anterior));
    int   direcaoX = 0, direcaoY = 0;
    DWORD proximoPasso = 0, ultimoRelato = 0, quadro = 0;

    for (;;)
    {
        XINPUT_STATE agora;
        ZeroMemory(&agora, sizeof(agora));
        XInputGetState(0, &agora);

        WORD novos = agora.Gamepad.wButtons & ~anterior.Gamepad.wButtons;
        anterior = agora;

        int dx = 0, dy = 0;
        if (novos & XINPUT_GAMEPAD_DPAD_RIGHT) dx =  1;
        if (novos & XINPUT_GAMEPAD_DPAD_LEFT)  dx = -1;
        if (novos & XINPUT_GAMEPAD_DPAD_DOWN)  dy =  1;
        if (novos & XINPUT_GAMEPAD_DPAD_UP)    dy = -1;

        // O analógico não tem "apertou agora": é posição contínua. Ganha comportamento
        // de tecla segurada — passo imediato, pausa, depois repetição.
        int ax = 0, ay = 0;
        if (agora.Gamepad.sThumbLX >  ZONA_MORTA) ax =  1;
        if (agora.Gamepad.sThumbLX < -ZONA_MORTA) ax = -1;
        if (agora.Gamepad.sThumbLY >  ZONA_MORTA) ay = -1;
        if (agora.Gamepad.sThumbLY < -ZONA_MORTA) ay =  1;

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

        if (g_menuAberto)
        {
            if (dy != 0 && g_menuQtd > 0)
                g_menuFoco = (g_menuFoco + dy + g_menuQtd) % g_menuQtd;
            // UMA ação por quadro: "novos" é máscara e nada impede A e ☰ juntos.
            // Encadeados com if solto, o segundo rodava sobre o estado que o primeiro
            // acabara de trocar -- inclusive sobre um ponteiro recém-invalidado.
            if      (novos & XINPUT_GAMEPAD_A) EscolherNoMenu();
            else if (novos & XINPUT_GAMEPAD_B) g_menuAberto = false;
        }
        else if (g_tela == TELA_COLECOES)
        {
            if (dx) MoverColecao(dx);
            if (dy) MoverColecao(dy * COL_POR_LINHA);
            if      (novos & XINPUT_GAMEPAD_A)     AbrirColecao();
            else if (novos & XINPUT_GAMEPAD_X)     NovaColecao();
            else if (novos & XINPUT_GAMEPAD_START) AbrirMenuColecao();
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
            if      (novos & XINPUT_GAMEPAD_LEFT_SHOULDER)  SaltoLetra(-1);
            else if (novos & XINPUT_GAMEPAD_RIGHT_SHOULDER) SaltoLetra(1);
            else if (novos & XINPUT_GAMEPAD_A) Confirmar();
            else if (novos & XINPUT_GAMEPAD_B) Voltar();
            else if (novos & XINPUT_GAMEPAD_X) { if (g_tela == TELA_JOGOS) AbrirAdicionar(); }
            else if (novos & XINPUT_GAMEPAD_START)
            {
                if (g_tela == TELA_ADICIONAR) FecharAdicionar(true);
                else                          AbrirMenuJogo();
            }
        }

        // Voltando do teclado do sistema, "anterior" é um retrato de segundos atrás:
        // o A que confirmou lá reapareceria como botão novo e abriria uma coleção
        // sozinho. Relê o estado e zera a repetição do direcional.
        if (g_bloqueou)
        {
            g_bloqueou = false;
            ZeroMemory(&anterior, sizeof(anterior));
            XInputGetState(0, &anterior);
            direcaoX = direcaoY = 0;
            proximoPasso = 0;
            tAgora = GetTickCount();
        }

        quadro++;
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
