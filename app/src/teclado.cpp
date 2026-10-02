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

    // Ver o Larga de main.cpp: literal estreito do fonte vem em ANSI, nao em UTF-8, e
    // CP_UTF8 parava no primeiro acento -- o titulo "Nova colecao" chegava ao teclado
    // do sistema cortado em "Nova cole".
    bool EhUtf8(const char *s)
    {
        const unsigned char *p = (const unsigned char *)s;
        while (*p)
        {
            int extras;
            if (*p < 0x80)                extras = 0;
            else if ((*p & 0xE0) == 0xC0) extras = 1;
            else if ((*p & 0xF0) == 0xE0) extras = 2;
            else if ((*p & 0xF8) == 0xF0) extras = 3;
            else return false;

            p++;
            while (extras-- > 0)
                if ((*p++ & 0xC0) != 0x80) return false;
        }
        return true;
    }

    void Larga(const char *origem, WCHAR *destino, int capacidade)
    {
        if (origem == NULL)
        {
            destino[0] = L'\0';
            return;
        }
        if (!EhUtf8(origem))
        {
            int i = 0;
            for (; origem[i] != '\0' && i < capacidade - 1; i++)
                destino[i] = (WCHAR)(unsigned char)origem[i];
            destino[i] = L'\0';
            return;
        }
        if (MultiByteToWideChar(CP_UTF8, 0, origem, -1, destino, capacidade) <= 0)
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
