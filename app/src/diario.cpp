#include "diario.h"
#include <stdio.h>
#include <stdarg.h>

namespace
{
    FILE *g_arquivo = NULL;
}

namespace diario
{
    void Abrir(const char *caminho)
    {
        g_arquivo = fopen(caminho, "w");
    }

    void Escrever(const char *formato, ...)
    {
        if (g_arquivo == NULL)
            return;

        va_list args;
        va_start(args, formato);
        vfprintf(g_arquivo, formato, args);
        va_end(args);

        fputc('\n', g_arquivo);
        fflush(g_arquivo);   // sem isto, um travamento leva junto o que interessa saber
    }

    void Fechar()
    {
        if (g_arquivo != NULL)
        {
            fclose(g_arquivo);
            g_arquivo = NULL;
        }
    }
}
