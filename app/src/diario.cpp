#include "diario.h"
#include <xtl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

namespace
{
    HANDLE           g_arquivo = INVALID_HANDLE_VALUE;
    char             g_caminho[256] = "";
    CRITICAL_SECTION g_trava;
    bool             g_travaPronta = false;

    // Escreve e FORCA o dado ao disco.
    //
    // Era FILE* com fflush, e isso nao basta num console que morre de verdade: o
    // fflush entrega ao sistema, e o que ficou no cache do sistema de arquivos se
    // perde no crash. Tres rodadas de diagnostico foram construidas em cima de um fim
    // de log que podia nao ser o fim da execucao -- e era justamente a ultima linha,
    // a mais importante, a que tinha mais chance de sumir.
    void Despejar(const char *texto, int tamanho)
    {
        if (g_arquivo == INVALID_HANDLE_VALUE)
            return;

        DWORD escritos = 0;
        WriteFile(g_arquivo, texto, (DWORD)tamanho, &escritos, NULL);
        FlushFileBuffers(g_arquivo);
    }
}

namespace diario
{
    void Abrir(const char *caminho)
    {
        if (!g_travaPronta)
        {
            InitializeCriticalSection(&g_trava);
            g_travaPronta = true;
        }

        _snprintf(g_caminho, sizeof(g_caminho), "%s", caminho);
        g_caminho[sizeof(g_caminho) - 1] = '\0';

        g_arquivo = CreateFile(g_caminho, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    void Reabrir()
    {
        if (g_arquivo != INVALID_HANDLE_VALUE || g_caminho[0] == '\0')
            return;

        g_arquivo = CreateFile(g_caminho, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_arquivo != INVALID_HANDLE_VALUE)
            SetFilePointer(g_arquivo, 0, NULL, FILE_END);
    }

    // Chamada das DUAS threads: a de desenho e a do carregador. Sem a trava, duas
    // escritas se intercalam no meio da linha -- ou pior, no meio da estrutura do
    // arquivo.
    void Escrever(const char *formato, ...)
    {
        if (g_arquivo == INVALID_HANDLE_VALUE || !g_travaPronta)
            return;

        char linha[1024];
        va_list args;
        va_start(args, formato);
        int n = _vsnprintf(linha, sizeof(linha) - 2, formato, args);
        va_end(args);

        if (n < 0) n = (int)strlen(linha);        // truncou: _vsnprintf devolve -1
        if (n > (int)sizeof(linha) - 2) n = (int)sizeof(linha) - 2;
        linha[n++] = '\n';

        EnterCriticalSection(&g_trava);
        Despejar(linha, n);
        LeaveCriticalSection(&g_trava);
    }

    void Fechar()
    {
        if (g_arquivo == INVALID_HANDLE_VALUE)
            return;

        if (g_travaPronta) EnterCriticalSection(&g_trava);
        FlushFileBuffers(g_arquivo);
        CloseHandle(g_arquivo);
        g_arquivo = INVALID_HANDLE_VALUE;
        if (g_travaPronta) LeaveCriticalSection(&g_trava);
    }
}
