// CollectionUI — primeira tela de verdade.
//
// Grade de 5 capas por linha, com os jogos lidos do content.db do FreeStyle e a arte
// tirada dos .assets. Navega pelo controle. É o desenho de docs/interface.md saindo
// do protótipo HTML e indo para o console.
//
// Desenha com a ATG, o conjunto de amostra que vem no próprio XDK: ela dá o
// renderizador de fonte (com corte por LARGURA, não por contagem de caractere) e os
// glifos dos botões do controle, que são as duas coisas que o desenho pedia.

#include <xtl.h>
#include <xgraphics.h>
#include "diario.h"
#include "biblioteca.h"
#include "fsda.h"
#include "AtgDevice.h"
#include "AtgFont.h"
#include "AtgDebugDraw.h"
#include "AtgSimpleShaders.h"

// A ATG espera este ponteiro global; normalmente quem o define é o AtgApp.cpp, que
// não usamos porque não queremos herdar da aplicação dela.
//
// Atenção ao tipo: dentro do namespace, D3DDevice é o ATG::D3DDevice do AtgDevice.h,
// que herda do global e acrescenta métodos. Definir com o tipo global compila e falha
// no link, porque o nome decorado fica diferente.
namespace ATG { D3DDevice *g_pd3dDevice = NULL; }

namespace
{
    // Medidas da grade, em pixels de 1280x720. Saíram do protótipo.
    const int COLUNAS      = 5;
    const int CAPA_L       = 180;
    const int CAPA_A       = 252;   // 5:7, a proporção da capa de 360
    const int ESPACO_X     = 36;
    const int ESPACO_Y     = 54;
    const int MARGEM_X     = 84;
    const int TOPO         = 96;
    const int MAX_TEXTURAS = 20;    // primeira leva; o resto vira streaming depois

    const D3DCOLOR COR_FUNDO   = D3DCOLOR_XRGB(13, 17, 15);
    const D3DCOLOR COR_TEXTO   = D3DCOLOR_XRGB(231, 237, 233);
    const D3DCOLOR COR_APAGADO = D3DCOLOR_XRGB(142, 156, 148);
    const D3DCOLOR COR_ANEL    = D3DCOLOR_XRGB(155, 203, 60);   // o verde do ring of light

    ATG::Font g_fonte;

    struct Item
    {
        std::string  nome;
        D3DTexture  *capa;
    };

    std::vector<Item> g_itens;
    int g_foco = 0;

    void Larga(const std::string &origem, WCHAR *destino, int capacidade)
    {
        int n = MultiByteToWideChar(CP_ACP, 0, origem.c_str(), -1, destino, capacidade);
        if (n <= 0)
            destino[0] = L'\0';
    }

    D3DTexture *CarregarCapa(const std::string &caminhoBanco, const biblioteca::Jogo &jogo)
    {
        std::string pasta = biblioteca::PastaArte(caminhoBanco, jogo.id);
        if (pasta.empty())
            return NULL;

        char arquivo[512];
        sprintf(arquivo, "%s\\%08X.assets", pasta.c_str(), jogo.id);

        std::vector<fsda::Imagem> imagens;
        if (!fsda::Ler(arquivo, imagens))
            return NULL;

        const fsda::Imagem *capa = fsda::Achar(imagens, fsda::TIPO_CAPA);
        if (capa == NULL)
            return NULL;

        std::vector<unsigned char> bytes;
        if (!fsda::LerBytes(arquivo, *capa, bytes))
            return NULL;

        // O DDS sai do .assets inteiro, com cabeçalho, então o D3DX carrega direto
        // da memória: não há arquivo de imagem em disco para abrir.
        D3DTexture *textura = NULL;
        HRESULT hr = D3DXCreateTextureFromFileInMemory(ATG::g_pd3dDevice,
                                                       &bytes[0], (UINT)bytes.size(),
                                                       &textura);
        if (FAILED(hr))
        {
            diario::Escrever("  falhou a textura de %s: hr = 0x%08X", jogo.nome.c_str(), hr);
            return NULL;
        }
        return textura;
    }

    void Desenhar()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->Clear(0, NULL, D3DCLEAR_TARGET, COR_FUNDO, 1.0f, 0);

        WCHAR texto[256];

        // Cabeçalho
        g_fonte.Begin();
        g_fonte.SetScaleFactors(1.4f, 1.4f);
        g_fonte.DrawText((FLOAT)MARGEM_X, 40.0f, COR_TEXTO, L"Todos os jogos", 0);
        g_fonte.SetScaleFactors(1.0f, 1.0f);

        swprintf_s(texto, 256, L"%d jogos", (int)g_itens.size());
        g_fonte.DrawText((FLOAT)MARGEM_X + 240.0f, 48.0f, COR_APAGADO, texto, 0);
        g_fonte.End();

        // Grade
        for (size_t i = 0; i < g_itens.size(); i++)
        {
            int coluna = (int)i % COLUNAS;
            int linha  = (int)i / COLUNAS;

            D3DRECT r;
            r.x1 = MARGEM_X + coluna * (CAPA_L + ESPACO_X);
            r.y1 = TOPO + linha * (CAPA_A + ESPACO_Y);
            r.x2 = r.x1 + CAPA_L;
            r.y2 = r.y1 + CAPA_A;

            if (r.y2 > 640)          // não invade o rodapé
                break;

            if (g_itens[i].capa != NULL)
                ATG::DebugDraw::DrawScreenSpaceTexturedRect(r, g_itens[i].capa);
            else
                ATG::DebugDraw::DrawScreenSpaceRect(r, 1.0f, COR_APAGADO);

            if ((int)i == g_foco)
            {
                D3DRECT anel = r;
                anel.x1 -= 3; anel.y1 -= 3; anel.x2 += 3; anel.y2 += 3;
                ATG::DebugDraw::DrawScreenSpaceRect(anel, 3.0f, COR_ANEL);
            }

            // O nome: cortado pela LARGURA da capa. É a ATG que resolve o problema
            // dos "Call of Duty: Modern Wa..." indistinguíveis do protótipo.
            Larga(g_itens[i].nome, texto, 256);
            g_fonte.Begin();
            g_fonte.DrawText((FLOAT)r.x1, (FLOAT)r.y2 + 6.0f,
                             ((int)i == g_foco) ? COR_TEXTO : COR_APAGADO,
                             texto, ATGFONT_TRUNCATED, (FLOAT)CAPA_L);
            g_fonte.End();
        }

        // Rodapé, com os glifos do controle embutidos na fonte
        g_fonte.Begin();
        g_fonte.DrawText((FLOAT)MARGEM_X, 672.0f, COR_APAGADO,
                         GLYPH_A_BUTTON L" Jogar    " GLYPH_B_BUTTON L" Voltar", 0);
        g_fonte.End();

        d->Present(NULL, NULL, NULL, NULL);
    }

    void Mover(int delta)
    {
        int n = (int)g_itens.size();
        if (n == 0) return;
        g_foco += delta;
        if (g_foco < 0)  g_foco = 0;
        if (g_foco >= n) g_foco = n - 1;
    }
}

void __cdecl main()
{
    diario::Abrir("game:\\collectionui.log");
    diario::Escrever("CollectionUI — primeira tela");

    // --- D3D ---
    // No Xbox 360 nao ha indirecao de COM: Direct3D::CreateDevice e um metodo ESTATICO
    // que encaminha para a funcao global Direct3D_CreateDevice, e o ponteiro devolvido
    // por Direct3DCreate9 nunca e dereferenciado. Guardamos a chamada porque e ela que
    // inicializa o D3D, mas o objeto em si e cerimonia -- dai o (void).
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
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp,
                                   &dispositivo);
    ATG::g_pd3dDevice = (ATG::D3DDevice *)dispositivo;
    if (FAILED(hr))
    {
        diario::Escrever("ERRO: CreateDevice = 0x%08X", hr);
        for (;;) {}
    }
    diario::Escrever("D3D pronto em 1280x720");

    ATG::SimpleShaders::Initialize(NULL, NULL);

    hr = g_fonte.Create("game:\\media\\Arial_16.xpr");
    if (FAILED(hr))
        diario::Escrever("AVISO: fonte nao carregou (0x%08X) — a tela sai sem texto", hr);
    else
        diario::Escrever("fonte carregada");

    // --- biblioteca ---
    std::string caminhoBanco;
    std::vector<biblioteca::Jogo> jogos;

    if (biblioteca::AcharBanco(caminhoBanco) &&
        biblioteca::Ler(caminhoBanco.c_str(), jogos))
    {
        diario::Escrever("biblioteca: %d jogos", (int)jogos.size());

        int carregadas = 0;
        for (size_t i = 0; i < jogos.size() && carregadas < MAX_TEXTURAS; i++)
        {
            Item it;
            it.nome = jogos[i].nome;
            it.capa = CarregarCapa(caminhoBanco, jogos[i]);
            if (it.capa != NULL)
                carregadas++;
            g_itens.push_back(it);
        }
        diario::Escrever("capas carregadas: %d de %d itens", carregadas, (int)g_itens.size());
    }
    else
    {
        diario::Escrever("ERRO: nao consegui ler a biblioteca");
    }

    diario::Escrever("entrando no laco de desenho");

    // --- laço ---
    XINPUT_STATE anterior;
    ZeroMemory(&anterior, sizeof(anterior));

    for (;;)
    {
        XINPUT_STATE agora;
        ZeroMemory(&agora, sizeof(agora));
        XInputGetState(0, &agora);

        WORD novos = agora.Gamepad.wButtons & ~anterior.Gamepad.wButtons;
        if (novos & XINPUT_GAMEPAD_DPAD_RIGHT) Mover(1);
        if (novos & XINPUT_GAMEPAD_DPAD_LEFT)  Mover(-1);
        if (novos & XINPUT_GAMEPAD_DPAD_DOWN)  Mover(COLUNAS);
        if (novos & XINPUT_GAMEPAD_DPAD_UP)    Mover(-COLUNAS);
        anterior = agora;

        Desenhar();
    }
}
