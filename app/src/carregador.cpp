#include "carregador.h"
#include "diario.h"
#include "fsda.h"
#include <xtl.h>
#include <string.h>

namespace
{
    struct Pedido
    {
        int         indice;
        std::string arquivo;
    };

    struct Resultado
    {
        int                        indice;
        std::vector<unsigned char> bytes;
    };

    CRITICAL_SECTION       g_trava;
    std::vector<Pedido>    g_fila;
    std::vector<Resultado> g_prontos;
    HANDLE                 g_thread = NULL;
    HANDLE                 g_temPedido = NULL;
    volatile bool          g_parar = false;

    bool LerCapa(const Pedido &p, std::vector<unsigned char> &saida)
    {
        std::vector<fsda::Imagem> imagens;
        if (!fsda::Ler(p.arquivo.c_str(), imagens))
            return false;

        const fsda::Imagem *capa = fsda::Achar(imagens, fsda::TIPO_CAPA);
        if (capa == NULL)
            return false;

        return fsda::LerBytes(p.arquivo.c_str(), *capa, saida);
    }

    DWORD WINAPI Trabalhar(LPVOID)
    {
        for (;;)
        {
            WaitForSingleObject(g_temPedido, 200);
            if (g_parar)
                break;

            for (;;)
            {
                Pedido p;
                bool tem = false;

                EnterCriticalSection(&g_trava);
                if (!g_fila.empty())
                {
                    p = g_fila.front();
                    g_fila.erase(g_fila.begin());
                    tem = true;
                }
                LeaveCriticalSection(&g_trava);

                if (!tem)
                    break;

                Resultado r;
                r.indice = p.indice;
                if (!LerCapa(p, r.bytes))
                    r.bytes.clear();       // indice sem bytes = falhou, e a tela mostra o vazio

                EnterCriticalSection(&g_trava);
                g_prontos.push_back(r);
                LeaveCriticalSection(&g_trava);

                if (g_parar)
                    break;
            }
        }
        return 0;
    }
}

namespace carregador
{
    void Iniciar()
    {
        InitializeCriticalSection(&g_trava);
        g_temPedido = CreateEvent(NULL, FALSE, FALSE, NULL);
        g_parar = false;

        g_thread = CreateThread(NULL, 0, Trabalhar, NULL, CREATE_SUSPENDED, NULL);
        if (g_thread == NULL)
        {
            diario::Escrever("ERRO: nao criei a thread de carregamento");
            return;
        }

        // O ponto que faz a diferenca: sem fixar, a thread disputa o mesmo nucleo do
        // laco de desenho e o engasgo continua. O Xenon tem 3 nucleos de 2 threads;
        // 4 e a primeira thread do terceiro nucleo, longe da nossa.
        XSetThreadProcessor(g_thread, 4);
        ResumeThread(g_thread);

        diario::Escrever("thread de carregamento no processador 4");
    }

    void Parar()
    {
        g_parar = true;
        if (g_temPedido != NULL)
            SetEvent(g_temPedido);
        if (g_thread != NULL)
        {
            WaitForSingleObject(g_thread, 1000);
            CloseHandle(g_thread);
            g_thread = NULL;
        }
    }

    void Pedir(int indice, const std::string &arquivoAssets)
    {
        Pedido p;
        p.indice  = indice;
        p.arquivo = arquivoAssets;

        EnterCriticalSection(&g_trava);
        g_fila.push_back(p);
        LeaveCriticalSection(&g_trava);

        SetEvent(g_temPedido);
    }

    void DescartarPendentes()
    {
        EnterCriticalSection(&g_trava);
        g_fila.clear();
        LeaveCriticalSection(&g_trava);
    }

    bool Retirar(int *indice, std::vector<unsigned char> &bytes)
    {
        bool tem = false;

        EnterCriticalSection(&g_trava);
        if (!g_prontos.empty())
        {
            *indice = g_prontos.front().indice;
            bytes.swap(g_prontos.front().bytes);
            g_prontos.erase(g_prontos.begin());
            tem = true;
        }
        LeaveCriticalSection(&g_trava);

        return tem;
    }
}
