// CollectionUI — teste de fogo do toolchain.
//
// Prova três coisas de uma vez, e cada uma falha de um jeito visível:
//   1. o .xex roda no console               -> a tela deixa de ser a do dashboard
//   2. o D3D9 linka e o dispositivo inicia  -> a tela pinta
//   3. a GPU está apresentando quadros      -> a cor muda, em vez de congelar
//
// Escreve também um log em game:\collectionui.log, que se lê por FTP. O canal visual
// diz "funcionou"; o escrito diz "por que não". Sem XBDM e sem plugin no console.

#include <xtl.h>
#include <stdio.h>

static FILE *g_log = NULL;

static void LogOpen()
{
    g_log = fopen("game:\\collectionui.log", "w");   // "w": cada execução começa limpo
}

static void Log(const char *fmt, ...)
{
    if (g_log == NULL)
        return;

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);

    fputc('\n', g_log);
    fflush(g_log);   // sem isto, um travamento leva junto tudo que interessa saber
}

void __cdecl main()
{
    LogOpen();
    Log("CollectionUI v0 — teste de toolchain");

    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (d3d == NULL)
    {
        Log("ERRO: Direct3DCreate9 devolveu NULL");
        for (;;) {}   // não retorna: no 360, sair de main tira o título do ar
    }
    Log("Direct3DCreate9: ok");

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth        = 1280;
    pp.BackBufferHeight       = 720;
    pp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    pp.BackBufferCount        = 1;
    pp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    pp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = FALSE;
    pp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    IDirect3DDevice9 *device = NULL;
    HRESULT hr = d3d->CreateDevice(
        0, D3DDEVTYPE_HAL, NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp, &device);

    if (FAILED(hr) || device == NULL)
    {
        Log("ERRO: CreateDevice falhou, hr = 0x%08X", hr);
        for (;;) {}
    }
    Log("CreateDevice: ok, %dx%d", pp.BackBufferWidth, pp.BackBufferHeight);
    Log("entrando no laço de apresentação");

    // Cor que percorre o círculo de matiz. Se a tela ficar parada numa cor só,
    // o Present não está acontecendo; se piscar, está.
    unsigned int frame = 0;
    for (;;)
    {
        unsigned int fase = (frame / 2) % 768;
        unsigned int r, g, b;

        if (fase < 256)        { r = 255 - fase; g = fase;       b = 0;          }
        else if (fase < 512)   { r = 0;          g = 511 - fase; b = fase - 256; }
        else                   { r = fase - 512; g = 0;          b = 767 - fase; }

        device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(r, g, b), 1.0f, 0);
        device->Present(NULL, NULL, NULL, NULL);

        // Os primeiros quadros provam que o laço começou; depois, um sinal de vida
        // a cada ~10 s, para o log não crescer sem limite.
        if (frame < 5 || frame % 600 == 0)
            Log("quadro %u — rgb(%u, %u, %u)", frame, r, g, b);

        frame++;
    }
}
