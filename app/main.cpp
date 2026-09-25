// CollectionUI — fase 2: ler a biblioteca do console.
//
// Abre o content.db do FreeStyle com SQLite, percorre os .assets de cada jogo e escreve
// tudo no log. Os totais servem de conferencia cruzada: a mesma leitura foi feita no PC
// em Python (prototipo/extrair.py) e deu 120 jogos, capa grande em 120 e fundo em 51.
// Se o console disser outro numero, uma das duas implementacoes esta errada.
//
// A tela nao tem interface ainda, so cor: VERDE se a biblioteca carregou, VERMELHO se nao.
// Da para saber do sofa se vale ir buscar o log.

#include <xtl.h>
#include "diario.h"
#include "biblioteca.h"
#include "fsda.h"

namespace
{
    void Apresentar(unsigned int cor)
    {
        IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (d3d == NULL)
        {
            diario::Escrever("ERRO: Direct3DCreate9 devolveu NULL");
            for (;;) {}
        }

        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof(pp));
        pp.BackBufferWidth      = 1280;
        pp.BackBufferHeight     = 720;
        pp.BackBufferFormat     = D3DFMT_X8R8G8B8;
        pp.BackBufferCount      = 1;
        pp.SwapEffect           = D3DSWAPEFFECT_DISCARD;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

        IDirect3DDevice9 *device = NULL;
        HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                       D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
        if (FAILED(hr) || device == NULL)
        {
            diario::Escrever("ERRO: CreateDevice falhou, hr = 0x%08X", hr);
            for (;;) {}
        }

        for (;;)
        {
            device->Clear(0, NULL, D3DCLEAR_TARGET, cor, 1.0f, 0);
            device->Present(NULL, NULL, NULL, NULL);
        }
    }
}

void __cdecl main()
{
    diario::Abrir("game:\\collectionui.log");
    diario::Escrever("CollectionUI — fase 2: leitura da biblioteca");

    std::string caminhoBanco;
    if (!biblioteca::AcharBanco(caminhoBanco))
    {
        diario::Escrever("ERRO: nao achei content.db em nenhum dispositivo");
        Apresentar(D3DCOLOR_XRGB(200, 40, 40));
    }

    std::vector<biblioteca::Jogo> jogos;
    if (!biblioteca::Ler(caminhoBanco.c_str(), jogos) || jogos.empty())
    {
        diario::Escrever("ERRO: o banco abriu mas nao devolveu jogos");
        Apresentar(D3DCOLOR_XRGB(200, 40, 40));
    }

    diario::Escrever("jogos na biblioteca: %d", (int)jogos.size());
    diario::Escrever("");
    diario::Escrever("--- os 10 primeiros, em ordem alfabetica ---");

    for (size_t i = 0; i < jogos.size() && i < 10; i++)
    {
        const biblioteca::Jogo &j = jogos[i];
        diario::Escrever("%2d. %-42s  %08X  tipo %d  %s",
                         (int)i + 1, j.nome.c_str(), j.titleId, j.tipoArquivo,
                         j.genero.empty() ? "-" : j.genero.c_str());
    }

    diario::Escrever("");
    diario::Escrever("--- arte: percorrendo os %d .assets ---", (int)jogos.size());

    int comArte = 0, comCapa = 0, comFundo = 0, comIcone = 0, comBanner = 0;
    int maiorLargura = 0, maiorAltura = 0;

    for (size_t i = 0; i < jogos.size(); i++)
    {
        std::string pasta = biblioteca::PastaArte(caminhoBanco, jogos[i].id);
        if (pasta.empty())
            continue;

        char arquivo[512];
        sprintf(arquivo, "%s\\%08X.assets", pasta.c_str(), jogos[i].id);

        std::vector<fsda::Imagem> imagens;
        if (!fsda::Ler(arquivo, imagens))
            continue;

        comArte++;

        const fsda::Imagem *capa = fsda::Achar(imagens, fsda::TIPO_CAPA);
        if (capa != NULL)
        {
            comCapa++;
            if ((int)capa->largura > maiorLargura) maiorLargura = (int)capa->largura;
            if ((int)capa->altura  > maiorAltura)  maiorAltura  = (int)capa->altura;

            if (comCapa <= 3)
                diario::Escrever("   %-36s capa %ux%u %s (offset %u, %u bytes)",
                                 jogos[i].nome.c_str(), capa->largura, capa->altura,
                                 capa->formato, capa->offset, capa->tamanho);
        }
        if (fsda::Achar(imagens, fsda::TIPO_FUNDO)  != NULL) comFundo++;
        if (fsda::Achar(imagens, fsda::TIPO_ICONE)  != NULL) comIcone++;
        if (fsda::Achar(imagens, fsda::TIPO_BANNER) != NULL) comBanner++;
    }

    diario::Escrever("");
    diario::Escrever("--- totais (o PC mediu: 120 jogos, capa 120, fundo 51, icone 109, banner 29) ---");
    diario::Escrever("  .assets lidos : %d de %d", comArte, (int)jogos.size());
    diario::Escrever("  com capa      : %d   (maior: %dx%d)", comCapa, maiorLargura, maiorAltura);
    diario::Escrever("  com fundo     : %d", comFundo);
    diario::Escrever("  com icone     : %d", comIcone);
    diario::Escrever("  com banner    : %d", comBanner);
    diario::Escrever("");
    diario::Escrever("leitura concluida. tela verde.");

    Apresentar(D3DCOLOR_XRGB(40, 150, 60));
}
