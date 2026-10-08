#include "dispositivos.h"
#include "diario.h"
#include <xtl.h>
#include <stdio.h>

// Exports do kernel (xboxkrnl). Nao estao em header nenhum do XDK: sao API interna
// que o homebrew usa para montar unidade. As declaracoes seguem o uso consagrado.
typedef struct _STRING
{
    unsigned short Length;
    unsigned short MaximumLength;
    char          *Buffer;
} STRING, *PSTRING;

extern "C"
{
    void    RtlInitAnsiString(PSTRING destino, const char *origem);
    HRESULT ObCreateSymbolicLink(PSTRING apelido, PSTRING dispositivo);
    HRESULT ObDeleteSymbolicLink(PSTRING apelido);
}

namespace
{
    struct Mapa
    {
        const char *apelido;
        const char *dispositivo;
    };

    // O mapa e o mesmo que o X-Store usa, e bate com o que o console expoe.
    const Mapa MAPA[] = {
        { "Hdd:",  "\\Device\\Harddisk0\\Partition1" },
        { "Usb0:", "\\Device\\Mass0"                 },
        { "Usb1:", "\\Device\\Mass1"                 },
        { "Usb2:", "\\Device\\Mass2"                 },
    };
    const int QUANTOS = sizeof(MAPA) / sizeof(MAPA[0]);

    const char *APELIDOS[QUANTOS + 1];
    int         QUANTOS_VIVOS = 0;

    HRESULT Montar(const char *apelido, const char *dispositivo)
    {
        char alvo[64];
        sprintf(alvo, "\\??\\%s", apelido);

        STRING sApelido, sDispositivo;
        RtlInitAnsiString(&sDispositivo, dispositivo);
        RtlInitAnsiString(&sApelido, alvo);

        ObDeleteSymbolicLink(&sApelido);   // pode nao existir; o erro nao importa
        return ObCreateSymbolicLink(&sApelido, &sDispositivo);
    }
}

namespace dispositivos
{
    void MontarTodos()
    {
        QUANTOS_VIVOS = 0;
        for (int i = 0; i < QUANTOS; i++)
        {
            HRESULT hr = Montar(MAPA[i].apelido, MAPA[i].dispositivo);

            // So entra na lista o que MONTOU. Antes entrava sempre e o hr ia apenas
            // para o log, entao um console sem pendrive seguia sondando Usb0:, Usb1: e
            // Usb2: em toda busca de caminho. Nao custa nada para jogo que esta no HD
            // (o Resolver acerta no primeiro), mas cada linha morta do content.db --
            // jogo apagado do disco que ficou no banco -- pagava tres chamadas a toa.
            if (SUCCEEDED(hr))
                APELIDOS[QUANTOS_VIVOS++] = MAPA[i].apelido;

            diario::Escrever("  montar %-6s -> %-32s hr = 0x%08X%s",
                             MAPA[i].apelido, MAPA[i].dispositivo, hr,
                             SUCCEEDED(hr) ? "" : "  (pode ser normal: dispositivo ausente)");
        }
        APELIDOS[QUANTOS_VIVOS] = NULL;
    }

    const char *const *Apelidos(int *quantos)
    {
        if (quantos != NULL)
            *quantos = QUANTOS_VIVOS;
        return APELIDOS;
    }
}
