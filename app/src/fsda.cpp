#include "fsda.h"
#include <xtl.h>
#include <string.h>
#include <stdio.h>

namespace
{
    // O container e big-endian e o Xbox 360 tambem, entao um cast direto funcionaria.
    // Montamos byte a byte mesmo assim: custa nada e o codigo continua correto se um dia
    // for lido no PC -- que foi exatamente como o formato foi descoberto.
    unsigned int LerBE32(const unsigned char *p)
    {
        return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
               ((unsigned int)p[2] << 8)  |  (unsigned int)p[3];
    }

    // ATENCAO: o DDS embutido e LITTLE-endian, ao contrario do container que o cerca.
    // E formato da Microsoft para PC, guardado como veio. Num processador big-endian
    // como o do 360, ler largura e altura sem inverter devolve numero sem sentido.
    unsigned int LerLE32(const unsigned char *p)
    {
        return ((unsigned int)p[3] << 24) | ((unsigned int)p[2] << 16) |
               ((unsigned int)p[1] << 8)  |  (unsigned int)p[0];
    }

    bool LerDe(HANDLE h, unsigned int posicao, void *destino, unsigned int quantos)
    {
        DWORD lidos = 0;
        if (SetFilePointer(h, (LONG)posicao, NULL, FILE_BEGIN) == 0xFFFFFFFF)
            return false;
        if (!ReadFile(h, destino, quantos, &lidos, NULL))
            return false;
        return lidos == quantos;
    }
}

namespace fsda
{
    bool Ler(const char *caminho, std::vector<Imagem> &saida)
    {
        saida.clear();

        HANDLE h = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        unsigned char cabecalho[24];
        if (!LerDe(h, 0, cabecalho, sizeof(cabecalho)) || memcmp(cabecalho, "FSDA", 4) != 0)
        {
            CloseHandle(h);
            return false;
        }

        unsigned int quantas = LerBE32(cabecalho + 16);
        if (quantas > 64)           // sanidade: o maior observado foi 6
            quantas = 64;

        for (unsigned int i = 0; i < quantas; i++)
        {
            unsigned char entrada[16];
            if (!LerDe(h, 24 + i * 16, entrada, sizeof(entrada)))
                break;

            Imagem img;
            memset(&img, 0, sizeof(img));
            img.tipo    = LerBE32(entrada);
            img.offset  = LerBE32(entrada + 4);
            img.tamanho = LerBE32(entrada + 8);

            if (img.offset == 0 || img.tamanho == 0)
                continue;

            // O cabecalho DDS tem 128 bytes: magic, dwSize, dwFlags, altura, largura...
            unsigned char dds[88];
            if (!LerDe(h, img.offset, dds, sizeof(dds)) || memcmp(dds, "DDS ", 4) != 0)
                continue;

            img.altura  = LerLE32(dds + 12);
            img.largura = LerLE32(dds + 16);
            memcpy(img.formato, dds + 84, 4);
            img.formato[4] = '\0';

            // O "tamanho" da tabela FSDA e largura*altura*4 + 128: o tamanho que a
            // imagem teria DESCOMPRIMIDA, nao o do DXT que esta gravado. Medido nos 120
            // .assets: a capa de 900x600 aparece como 2.160.128 e o DDS real tem
            // 540.128. Ler o campo como veio custa 4x de disco e 4x de RAM por capa.
            if (memcmp(img.formato, "DXT", 3) == 0 && img.largura > 0 && img.altura > 0)
            {
                unsigned int blocos   = ((img.largura + 3) / 4) * ((img.altura + 3) / 4);
                unsigned int porBloco = (memcmp(img.formato, "DXT1", 4) == 0) ? 8 : 16;
                unsigned int real     = 128 + blocos * porBloco;
                if (real < img.tamanho)
                    img.tamanho = real;
            }

            // Teto de sanidade: o tamanho vem do arquivo, e um .assets corrompido com
            // 0xFFFFFFFF faria um resize de 4 GB num console de 512 MB -- bad_alloc
            // dentro da thread de leitura, sem ninguém para capturar. A maior capa
            // real tem uns 540 KB.
            if (img.tamanho > 8u * 1024u * 1024u)
                continue;

            saida.push_back(img);
        }

        CloseHandle(h);
        return !saida.empty();
    }

    bool LerBytes(const char *caminho, const Imagem &imagem, std::vector<unsigned char> &saida)
    {
        saida.clear();
        if (imagem.tamanho == 0)
            return false;

        HANDLE h = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        saida.resize(imagem.tamanho);
        bool ok = LerDe(h, imagem.offset, &saida[0], imagem.tamanho);
        CloseHandle(h);

        if (!ok)
            saida.clear();
        return ok;
    }

    const Imagem *Achar(const std::vector<Imagem> &imagens, unsigned int tipo)
    {
        for (size_t i = 0; i < imagens.size(); i++)
        {
            if (imagens[i].tipo == tipo)
                return &imagens[i];
        }
        return NULL;
    }
}
