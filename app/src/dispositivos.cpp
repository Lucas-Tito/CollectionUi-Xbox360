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
        for (int i = 0; i < QUANTOS; i++)
        {
            HRESULT hr = Montar(MAPA[i].apelido, MAPA[i].dispositivo);
            APELIDOS[i] = MAPA[i].apelido;
            diario::Escrever("  montar %-6s -> %-32s hr = 0x%08X%s",
                             MAPA[i].apelido, MAPA[i].dispositivo, hr,
                             SUCCEEDED(hr) ? "" : "  (pode ser normal: dispositivo ausente)");
        }
        APELIDOS[QUANTOS] = NULL;
    }

    const char *const *Apelidos(int *quantos)
    {
        if (quantos != NULL)
            *quantos = QUANTOS;
        return APELIDOS;
    }
}
