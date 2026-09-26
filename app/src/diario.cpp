#include "diario.h"
#include <stdio.h>
#include <stdarg.h>

namespace
{
    FILE *g_arquivo = NULL;
    char  g_caminho[256] = "";
}

namespace diario
{
    void Abrir(const char *caminho)
    {
        _snprintf(g_caminho, sizeof(g_caminho), "%s", caminho);
        g_caminho[sizeof(g_caminho) - 1] = '\0';
        g_arquivo = fopen(g_caminho, "w");
    }

    void Reabrir()
    {
        if (g_arquivo == NULL && g_caminho[0] != '\0')
            g_arquivo = fopen(g_caminho, "a");
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
