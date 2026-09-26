// CollectionUI — grade de capas navegável pelo controle.
//
// Lê os jogos do content.db do FreeStyle e a arte dos .assets, desenhando com a ATG,
// o conjunto de amostra do próprio XDK. É o desenho de docs/interface.md.

#include <xtl.h>
#include <xgraphics.h>
#include "diario.h"
#include "biblioteca.h"
#include "dispositivos.h"
#include "config.h"
#include "carregador.h"
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
    // Medidas da grade, em pixels de 1280x720.
    //
    // A conta importa: com capa de 252px a segunda linha terminava em 654 e não cabia,
    // então só uma linha aparecia na tela. Com 238 cabem duas, que é o que um 720p
    // comporta com capa legível a três metros. Margem de 100px fica dentro da área
    // segura da TV, que corta as bordas.
    const int COLUNAS    = 5;
    const int LINHAS     = 2;          // visíveis por vez; o resto rola
    const int CAPA_L     = 170;
    const int CAPA_A     = 238;        // 5:7, a proporção da capa de 360
    const int ESPACO_X   = 56;
    const int ESPACO_Y   = 40;
    const int MARGEM_X   = 100;
    const int TOPO       = 92;
    const int POR_PAGINA = COLUNAS * LINHAS;

    // O asset tipo 128 é o ENCARTE inteiro (contracapa + lombada + frente), não a capa.
    // A frente são os 46,8% da direita — fração calibrada à mão contra a biblioteca
    // real. Em vez de recortar a imagem, amostramos só essa parte ao desenhar.
    const float FRENTE_U0 = 1.0f - 0.468f;

    // Analogico: fora desta zona o eixo conta como direcao. O repique imita tecla
    // segurada, para segurar a alavanca percorrer a lista sem virar corrida.
    const short ZONA_MORTA     = 14000;
    const DWORD ESPERA_INICIAL = 380;   // ms antes de comecar a repetir
    const DWORD ESPERA_REPETE  = 110;   // ms entre repeticoes

    const D3DCOLOR COR_FUNDO   = D3DCOLOR_XRGB(13, 17, 15);
    const D3DCOLOR COR_TEXTO   = D3DCOLOR_XRGB(231, 237, 233);
    const D3DCOLOR COR_APAGADO = D3DCOLOR_XRGB(142, 156, 148);
    const D3DCOLOR COR_FRACO   = D3DCOLOR_XRGB(92, 104, 98);
    const D3DCOLOR COR_ANEL    = D3DCOLOR_XRGB(155, 203, 60);   // o verde do ring of light

    ATG::Font g_fonte;

    std::string                   g_caminhoBanco;
    std::vector<biblioteca::Jogo> g_jogos;

    // Só as capas da página visível ficam na memória. Carregar as 120 de uma vez fazia
    // a tela ficar preta por um tempo longo antes do primeiro quadro — cada .assets tem
    // vários MB e é aberto do disco.
    // Cache de capas. A versao anterior tinha uma janela que soltava a textura assim
    // que a linha saia da tela, entao voltar obrigava a reler -- e a capa piscava vazia.
    // O FreeStyle guarda num TextureCache; fazemos o mesmo.
    //
    // Cada capa ocupa cerca de 540 KB (900x600 em DXT5). Com 128 entradas sao uns 69 MB,
    // o que cabe folgado nos 512 MB do console e faz uma biblioteca de 120 jogos ficar
    // inteira em memoria depois da primeira passada: rolar de volta nunca mais recarrega.
    // O teto existe para biblioteca grande, nao para esta.
    const int CACHE_MAX = 128;

    struct Entrada
    {
        int         indice;
        D3DTexture *textura;
        DWORD       uso;      // relogio logico, para descartar o mais antigo
    };

    Entrada g_cache[CACHE_MAX];
    DWORD   g_relogio = 0;
    std::vector<int> g_emVoo;    // indices ja pedidos, para nao pedir duas vezes

    int  g_medidas = 0;
    int  g_foco = 0;
    int  g_primeiraLinha = 0;

    void Larga(const std::string &origem, WCHAR *destino, int capacidade)
    {
        if (MultiByteToWideChar(CP_UTF8, 0, origem.c_str(), -1, destino, capacidade) <= 0)
        {
            destino[0] = L'\0';
            return;
        }

        for (int i = 0; destino[i] != L'\0'; i++)
        {
            if (destino[i] == 0x3000)        // espaco ideografico
                destino[i] = L' ';
            else if (destino[i] >= 0x100)    // fora da fonte, e na faixa dos botoes
                destino[i] = L'?';
        }
    }

    D3DTexture *CarregarCapa(int indice)
    {
        if (indice < 0 || indice >= (int)g_jogos.size())
            return NULL;

        const biblioteca::Jogo &jogo = g_jogos[indice];
        std::string pasta = biblioteca::PastaArte(g_caminhoBanco, jogo.id);
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

        // O DDS sai do .assets inteiro, com cabeçalho, então o D3DX carrega direto da
        // memória: não há arquivo de imagem em disco para abrir.
        D3DTexture *textura = NULL;
        if (FAILED(D3DXCreateTextureFromFileInMemory(ATG::g_pd3dDevice,
                                                     &bytes[0], (UINT)bytes.size(),
                                                     &textura)))
            return NULL;

        return textura;
    }

    D3DTexture *NoCache(int indice, bool marcarUso)
    {
        for (int i = 0; i < CACHE_MAX; i++)
        {
            if (g_cache[i].textura != NULL && g_cache[i].indice == indice)
            {
                if (marcarUso)
                    g_cache[i].uso = ++g_relogio;
                return g_cache[i].textura;
            }
        }
        return NULL;
    }

    bool EmVoo(int indice)
    {
        for (size_t i = 0; i < g_emVoo.size(); i++)
            if (g_emVoo[i] == indice)
                return true;
        return false;
    }

    // Guarda no cache, descartando a entrada usada ha mais tempo quando cheio.
    void Guardar(int indice, D3DTexture *textura)
    {
        int alvo = -1;
        DWORD maisAntigo = 0xFFFFFFFF;

        for (int i = 0; i < CACHE_MAX; i++)
        {
            if (g_cache[i].textura == NULL)
            {
                alvo = i;
                break;
            }
            if (g_cache[i].uso < maisAntigo)
            {
                maisAntigo = g_cache[i].uso;
                alvo = i;
            }
        }

        if (g_cache[alvo].textura != NULL)
            g_cache[alvo].textura->Release();

        g_cache[alvo].indice  = indice;
        g_cache[alvo].textura = textura;
        g_cache[alvo].uso     = ++g_relogio;
    }

    // Pede o que falta para a tela e para uma linha de folga de cada lado. Sem estado
    // de janela: o cache responde quem ja tem, e o resto vira pedido.
    void PedirOQueFalta()
    {
        int base = (g_primeiraLinha - 1) * COLUNAS;
        if (base < 0)
            base = 0;
        int fim = (g_primeiraLinha + LINHAS + 1) * COLUNAS;
        if (fim > (int)g_jogos.size())
            fim = (int)g_jogos.size();

        for (int volta = 0; volta < 2; volta++)
        {
            for (int indice = base; indice < fim; indice++)
            {
                bool visivel = (indice >= g_primeiraLinha * COLUNAS) &&
                               (indice <  (g_primeiraLinha + LINHAS) * COLUNAS);
                if ((volta == 0) != visivel)
                    continue;
                if (NoCache(indice, false) != NULL || EmVoo(indice))
                    continue;

                std::string pasta = biblioteca::PastaArte(g_caminhoBanco, g_jogos[indice].id);
                if (pasta.empty())
                    continue;

                char arquivo[512];
                sprintf(arquivo, "%s\\%08X.assets", pasta.c_str(), g_jogos[indice].id);
                carregador::Pedir(indice, arquivo);
                g_emVoo.push_back(indice);
            }
        }
    }

    // Recolhe o que a thread de leitura terminou e cria a textura -- isto na thread de
    // desenho, para o D3D nao ser tocado por duas. De bytes em memoria e rapido: o DDS
    // vem em DXT5 e nao ha decodificacao a fazer.
    void RecolherCarregadas()
    {
        int indice;
        std::vector<unsigned char> bytes;

        // UMA por quadro. Mesmo a 3 ms, cinco de uma vez dariam um solavanco.
        if (!carregador::Retirar(&indice, bytes))
            return;

        for (size_t k = 0; k < g_emVoo.size(); k++)
        {
            if (g_emVoo[k] == indice)
            {
                g_emVoo.erase(g_emVoo.begin() + k);
                break;
            }
        }

        if (bytes.empty() || NoCache(indice, false) != NULL)
            return;      // falhou, ou chegou duplicado; nao sobrescreve nada

        // O formato vem do fourCC do proprio DDS, para o D3DX nao decidir converter.
        D3DFORMAT formato = D3DFMT_UNKNOWN;
        if (bytes.size() > 88)
        {
            if (memcmp(&bytes[84], "DXT5", 4) == 0)      formato = D3DFMT_DXT5;
            else if (memcmp(&bytes[84], "DXT1", 4) == 0) formato = D3DFMT_DXT1;
        }

        DWORD t0 = GetTickCount();
        D3DTexture *textura = NULL;

        // A versao Ex com estes parametros e o que o FreeStyle faz. A versao sem Ex usa
        // D3DX_DEFAULT em tudo: redimensiona 900x600 para 1024x1024 com filtragem e gera
        // a cadeia inteira de mipmaps, uns onze niveis. Era o que travava a cada linha.
        HRESULT hr = D3DXCreateTextureFromFileInMemoryEx(
            ATG::g_pd3dDevice, &bytes[0], (UINT)bytes.size(),
            D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2,
            1,                             // um mipmap so
            D3DUSAGE_CPU_CACHED_MEMORY, formato, D3DPOOL_DEFAULT,
            D3DX_FILTER_NONE, D3DX_FILTER_NONE,
            0, NULL, NULL, &textura);

        if (SUCCEEDED(hr) && textura != NULL)
            Guardar(indice, textura);

        if (g_medidas < 8)
        {
            diario::Escrever("  textura %d: %u bytes, %u ms, hr=0x%08X",
                             indice, (unsigned)bytes.size(), GetTickCount() - t0, hr);
            g_medidas++;
        }
    }

    void Desenhar()
    {
        ATG::D3DDevice *d = ATG::g_pd3dDevice;
        d->Clear(0, NULL, D3DCLEAR_TARGET, COR_FUNDO, 1.0f, 0);

        WCHAR texto[256];
        int base = g_primeiraLinha * COLUNAS;
        int total = (int)g_jogos.size();

        // Cabeçalho
        g_fonte.Begin();
        g_fonte.SetScaleFactors(1.5f, 1.5f);
        g_fonte.DrawText((FLOAT)MARGEM_X, 34.0f, COR_TEXTO, L"Todos os jogos", 0);
        g_fonte.SetScaleFactors(1.0f, 1.0f);
        swprintf_s(texto, 256, L"%d jogos", total);
        g_fonte.DrawText((FLOAT)MARGEM_X + 260.0f, 46.0f, COR_FRACO, texto, 0);
        g_fonte.End();

        // Grade
        for (int i = 0; i < POR_PAGINA; i++)
        {
            int indice = base + i;
            if (indice >= total)
                break;

            int coluna = i % COLUNAS;
            int linha  = i / COLUNAS;

            D3DRECT r;
            r.x1 = MARGEM_X + coluna * (CAPA_L + ESPACO_X);
            r.y1 = TOPO + linha * (CAPA_A + ESPACO_Y);
            r.x2 = r.x1 + CAPA_L;
            r.y2 = r.y1 + CAPA_A;

            D3DTexture *capa = NoCache(indice, true);

            if (capa != NULL)
            {
                // Só a frente do encarte: amostra de U 0,532 até 1,0.
                ATG::DebugDraw::DrawScreenSpaceTexturedRectPatch(
                    r,
                    XMFLOAT2(FRENTE_U0, 0.0f),
                    XMFLOAT2(1.0f,      0.0f),
                    XMFLOAT2(FRENTE_U0, 1.0f),
                    capa);
            }
            else
            {
                ATG::DebugDraw::DrawScreenSpaceRect(r, 1.0f, COR_FRACO);
            }

            if (indice == g_foco)
            {
                D3DRECT anel = r;
                anel.x1 -= 4; anel.y1 -= 4; anel.x2 += 4; anel.y2 += 4;
                ATG::DebugDraw::DrawScreenSpaceRect(anel, 3.0f, COR_ANEL);
            }

            // O nome, cortado pela LARGURA da capa. É a ATG que resolve o problema dos
            // "Call of Duty: Modern Wa..." indistinguíveis que o protótipo tinha.
            Larga(g_jogos[indice].nome, texto, 256);
            g_fonte.Begin();
            g_fonte.DrawText((FLOAT)r.x1, (FLOAT)r.y2 + 8.0f,
                             (indice == g_foco) ? COR_TEXTO : COR_APAGADO,
                             texto, ATGFONT_TRUNCATED, (FLOAT)CAPA_L);
            g_fonte.End();
        }

        // Rodapé: símbolo do botão e ação, com os glifos embutidos na fonte
        g_fonte.Begin();
        g_fonte.DrawText((FLOAT)MARGEM_X, 660.0f, COR_APAGADO,
                         GLYPH_A_BUTTON L" Jogar     " GLYPH_B_BUTTON L" Voltar", 0);

        int linhasTotais = (total + COLUNAS - 1) / COLUNAS;
        swprintf_s(texto, 256, L"%d de %d", g_foco + 1, total);
        g_fonte.DrawText(1180.0f, 660.0f, COR_FRACO, texto, ATGFONT_RIGHT);
        (void)linhasTotais;
        g_fonte.End();

        d->Present(NULL, NULL, NULL, NULL);
    }

    // Modal de arranque: escolher qual instalacao do FreeStyle usar. NAO e uma tela da
    // navegacao -- e um laco proprio, que roda uma vez e devolve. Quando a tela de
    // colecoes existir, com ida e volta de verdade, ai um conceito de tela se paga.
    std::string Escolher(const std::vector<biblioteca::Candidato> &candidatos)
    {
        int escolhido = 0;
        XINPUT_STATE anterior;
        ZeroMemory(&anterior, sizeof(anterior));

        for (;;)
        {
            XINPUT_STATE agora;
            ZeroMemory(&agora, sizeof(agora));
            XInputGetState(0, &agora);
            WORD novos = agora.Gamepad.wButtons & ~anterior.Gamepad.wButtons;
            anterior = agora;

            if (novos & XINPUT_GAMEPAD_DPAD_DOWN)
                escolhido = (escolhido + 1) % (int)candidatos.size();
            if (novos & XINPUT_GAMEPAD_DPAD_UP)
                escolhido = (escolhido + (int)candidatos.size() - 1) % (int)candidatos.size();
            if (novos & XINPUT_GAMEPAD_A)
                return candidatos[escolhido].caminho;

            ATG::D3DDevice *d = ATG::g_pd3dDevice;
            d->Clear(0, NULL, D3DCLEAR_TARGET, COR_FUNDO, 1.0f, 0);

            WCHAR texto[512];

            g_fonte.Begin();
            g_fonte.SetScaleFactors(1.5f, 1.5f);
            g_fonte.DrawText((FLOAT)MARGEM_X, 120.0f, COR_TEXTO, L"Qual biblioteca?", 0);
            g_fonte.SetScaleFactors(1.0f, 1.0f);
            g_fonte.DrawText((FLOAT)MARGEM_X, 180.0f, COR_APAGADO,
                             L"Achei mais de uma instalacao do FreeStyle neste console.", 0);
            g_fonte.End();

            for (int i = 0; i < (int)candidatos.size(); i++)
            {
                D3DRECT r;
                r.x1 = MARGEM_X;
                r.y1 = 240 + i * 62;
                r.x2 = 1180;
                r.y2 = r.y1 + 50;

                if (i == escolhido)
                    ATG::DebugDraw::DrawScreenSpaceRect(r, 2.0f, COR_ANEL);

                Larga(candidatos[i].rotulo, texto, 512);
                g_fonte.Begin();
                g_fonte.DrawText((FLOAT)r.x1 + 18.0f, (FLOAT)r.y1 + 14.0f,
                                 (i == escolhido) ? COR_TEXTO : COR_APAGADO, texto, 0);
                g_fonte.End();
            }

            g_fonte.Begin();
            g_fonte.DrawText((FLOAT)MARGEM_X, 660.0f, COR_APAGADO,
                             GLYPH_A_BUTTON L" Escolher", 0);
            g_fonte.DrawText(1180.0f, 660.0f, COR_FRACO,
                             L"fica gravado; apague collectionui.ini para trocar",
                             ATGFONT_RIGHT);
            g_fonte.End();

            d->Present(NULL, NULL, NULL, NULL);
        }
    }

    void Mover(int delta)
    {
        int total = (int)g_jogos.size();
        if (total == 0)
            return;

        int novo = g_foco + delta;
        if (novo < 0 || novo >= total)
            return;                       // não dá a volta: bate no fim e para
        g_foco = novo;

        // A grade segue o foco, rolando o mínimo necessário.
        int linhaDoFoco = g_foco / COLUNAS;
        if (linhaDoFoco < g_primeiraLinha)
            g_primeiraLinha = linhaDoFoco;
        else if (linhaDoFoco >= g_primeiraLinha + LINHAS)
            g_primeiraLinha = linhaDoFoco - LINHAS + 1;

    }
}

void __cdecl main()
{
    diario::Abrir("game:\\collectionui.log");
    diario::Escrever("CollectionUI — grade de capas");

    for (int i = 0; i < CACHE_MAX; i++)
    {
        g_cache[i].indice = -1;
        g_cache[i].textura = NULL;
        g_cache[i].uso = 0;
    }

    // --- D3D ---
    // No Xbox 360 não há indireção de COM: Direct3D::CreateDevice é um método ESTÁTICO
    // que encaminha para a função global Direct3D_CreateDevice, e o ponteiro devolvido
    // por Direct3DCreate9 nunca é dereferenciado.
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

    // Registrar a intenção ANTES de cada chamada arriscada: se o console cair, o log
    // diz onde. Logar só o sucesso faz a falha aparecer como silêncio.
    diario::Escrever("chamando SimpleShaders::Initialize (espera game:\\media\\effects\\simpleshaders.fxobj)");
    ATG::SimpleShaders::Initialize(NULL, NULL);
    diario::Escrever("SimpleShaders ok");

    diario::Escrever("carregando fonte game:\\media\\Arial_16.xpr");
    if (FAILED(g_fonte.Create("game:\\media\\Arial_16.xpr")))
        diario::Escrever("AVISO: fonte nao carregou — a tela sai sem texto");
    else
        diario::Escrever("fonte carregada");

    // --- biblioteca ---
    // Antes de procurar qualquer arquivo: um título só enxerga "game:" por padrão.
    // Sem montar, o HD simplesmente não existe para nós.
    diario::Escrever("montando dispositivos");
    dispositivos::MontarTodos();

    std::vector<biblioteca::Candidato> candidatos;
    biblioteca::ListarCandidatos(candidatos);

    // A escolha de antes, se ainda valer. Um caminho gravado que sumiu (instalacao
    // apagada, pendrive trocado) faz perguntar de novo, em vez de falhar calado.
    g_caminhoBanco = config::LerBanco();
    if (!g_caminhoBanco.empty() && !biblioteca::Existe(g_caminhoBanco))
    {
        diario::Escrever("o banco gravado nao existe mais: %s", g_caminhoBanco.c_str());
        g_caminhoBanco.clear();
    }

    if (g_caminhoBanco.empty() && candidatos.size() == 1)
    {
        // Uma so: nao ha escolha a fazer, nao se pergunta.
        g_caminhoBanco = candidatos[0].caminho;
        config::GravarBanco(g_caminhoBanco);
    }
    else if (g_caminhoBanco.empty() && candidatos.size() > 1)
    {
        diario::Escrever("mais de uma instalacao: perguntando");
        g_caminhoBanco = Escolher(candidatos);
        config::GravarBanco(g_caminhoBanco);
    }

    diario::Escrever("biblioteca escolhida: %s",
                     g_caminhoBanco.empty() ? "(nenhuma)" : g_caminhoBanco.c_str());

    if (!g_caminhoBanco.empty() &&
        biblioteca::Ler(g_caminhoBanco.c_str(), g_jogos))
    {
        diario::Escrever("biblioteca: %d jogos", (int)g_jogos.size());
        carregador::Iniciar();
    }
    else
    {
        diario::Escrever("ERRO: nao consegui ler a biblioteca");
    }

    diario::Escrever("entrando no laco de desenho");

    // --- laço ---
    XINPUT_STATE anterior;
    ZeroMemory(&anterior, sizeof(anterior));
    int   direcaoX = 0, direcaoY = 0;
    DWORD proximoPasso = 0;

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

        // O analogico nao tem "apertou agora": e posicao continua. Damos a ele o
        // comportamento de tecla segurada -- um passo imediato, pausa, depois repeticao.
        int dx = 0, dy = 0;
        if (agora.Gamepad.sThumbLX >  ZONA_MORTA) dx =  1;
        if (agora.Gamepad.sThumbLX < -ZONA_MORTA) dx = -1;
        if (agora.Gamepad.sThumbLY >  ZONA_MORTA) dy = -1;   // para cima
        if (agora.Gamepad.sThumbLY < -ZONA_MORTA) dy =  1;

        DWORD tAgora = GetTickCount();
        if (dx == 0 && dy == 0)
        {
            direcaoX = direcaoY = 0;
            proximoPasso = 0;
        }
        else if (dx != direcaoX || dy != direcaoY)
        {
            direcaoX = dx; direcaoY = dy;
            Mover(dx + dy * COLUNAS);
            proximoPasso = tAgora + ESPERA_INICIAL;
        }
        else if (tAgora >= proximoPasso)
        {
            Mover(dx + dy * COLUNAS);
            proximoPasso = tAgora + ESPERA_REPETE;
        }

        PedirOQueFalta();
        RecolherCarregadas();
        Desenhar();
    }
}
