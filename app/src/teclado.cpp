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

    void Larga(const char *origem, WCHAR *destino, int capacidade)
    {
        if (origem == NULL || MultiByteToWideChar(CP_UTF8, 0, origem, -1, destino, capacidade) <= 0)
            destino[0] = L'\0';
    }
}

namespace teclado
{
    bool Pedir(const char *titulo, const char *descricao,
               const char *textoInicial, std::string &saida)
    {
        WCHAR wTitulo[128], wDescricao[256], wInicial[MAX], wSaida[MAX];
        Larga(titulo, wTitulo, 128);
        Larga(descricao, wDescricao, 256);
        Larga(textoInicial, wInicial, MAX);
        ZeroMemory(wSaida, sizeof(wSaida));

        XOVERLAPPED ov;
        ZeroMemory(&ov, sizeof(ov));
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (ov.hEvent == NULL)
            return false;

        DWORD r = XShowKeyboardUI(0, VKBD_LATIN_FULL | VKBD_HIGHLIGHT_TEXT,
                                  wInicial, wTitulo, wDescricao, wSaida, MAX, &ov);
        if (r != ERROR_IO_PENDING)
        {
            diario::Escrever("teclado: nao abriu (erro %u)", r);
            CloseHandle(ov.hEvent);
            return false;
        }

        // Bloqueia até o usuário terminar. O teclado do sistema desenha por cima.
        WaitForSingleObject(ov.hEvent, INFINITE);

        // ATENÇÃO: XGetOverlappedResult devolve DWORD, um código de erro -- não BOOL.
        // ERROR_SUCCESS é ZERO. Guardar em BOOL inverte o teste e faz todo teclado
        // parecer cancelado, o que impedia criar qualquer coleção.
        DWORD estado = XGetOverlappedResult(&ov, NULL, TRUE);
        CloseHandle(ov.hEvent);

        if (estado != ERROR_SUCCESS)        // ERROR_CANCELLED: o usuário desistiu
            return false;

        char estreito[MAX * 4];
        if (WideCharToMultiByte(CP_UTF8, 0, wSaida, -1, estreito, sizeof(estreito),
                                NULL, NULL) <= 0)
            return false;

        saida = estreito;
        return !saida.empty();
    }
}
