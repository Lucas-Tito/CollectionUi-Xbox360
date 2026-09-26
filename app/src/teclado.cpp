#include "teclado.h"
#include "diario.h"
#include <xtl.h>
#include <string.h>

// XShowKeyboardUI vem declarado em include/xbox/xbox.h, puxado pelo xtl.h. Não é
// preciso declarar à mão -- diferente do ObCreateSymbolicLink de dispositivos.cpp,
// esse sim ausente de todo header.

namespace
{
    const DWORD MAX = 64;

    // No módulo, NÃO na pilha: o sistema escreve nestes buffers depois que a função
    // que os criou já voltou. Ver o cabeçalho.
    WCHAR       g_titulo[128];
    WCHAR       g_descricao[256];
    WCHAR       g_inicial[MAX];
    WCHAR       g_saida[MAX];
    XOVERLAPPED g_ov;
    bool        g_aberto = false;

    void Larga(const char *origem, WCHAR *destino, int capacidade)
    {
        if (origem == NULL || MultiByteToWideChar(CP_UTF8, 0, origem, -1, destino, capacidade) <= 0)
            destino[0] = L'\0';
    }
}

namespace teclado
{
    bool Abrir(const char *titulo, const char *descricao, const char *textoInicial)
    {
        if (g_aberto)
            return false;

        Larga(titulo, g_titulo, 128);
        Larga(descricao, g_descricao, 256);
        Larga(textoInicial, g_inicial, MAX);
        ZeroMemory(g_saida, sizeof(g_saida));
        ZeroMemory(&g_ov, sizeof(g_ov));

        // XUSER_INDEX_ANY, não 0: este console não tem tela de login e pode não ter
        // perfil nenhum conectado.
        DWORD r = XShowKeyboardUI(XUSER_INDEX_ANY, VKBD_LATIN_FULL | VKBD_HIGHLIGHT_TEXT,
                                  g_inicial, g_titulo, g_descricao, g_saida, MAX, &g_ov);
        if (r != ERROR_IO_PENDING)
        {
            diario::Escrever("teclado: nao abriu (erro %u)", r);
            return false;
        }

        g_aberto = true;
        return true;
    }

    bool Aberto()
    {
        return g_aberto;
    }

    bool Terminou(bool *confirmou, std::string &saida)
    {
        if (!g_aberto || !XHasOverlappedIoCompleted(&g_ov))
            return false;

        g_aberto = false;
        saida.clear();
        *confirmou = false;

        // ATENÇÃO: XGetOverlappedResult devolve DWORD, um código de erro -- não BOOL.
        // ERROR_SUCCESS é ZERO. Guardar em BOOL inverte o teste e faz todo teclado
        // parecer cancelado, o que já impediu criar qualquer coleção uma vez.
        // Aqui bWait é FALSE: a operação JÁ terminou, e esperar seria voltar ao erro
        // que travava o console.
        DWORD estado = XGetOverlappedResult(&g_ov, NULL, FALSE);
        if (estado != ERROR_SUCCESS)        // ERROR_CANCELLED: o usuário desistiu
            return true;

        char estreito[MAX * 4];
        if (WideCharToMultiByte(CP_UTF8, 0, g_saida, -1, estreito, sizeof(estreito),
                                NULL, NULL) <= 0)
            return true;

        saida = estreito;
        *confirmou = !saida.empty();
        return true;
    }
}
