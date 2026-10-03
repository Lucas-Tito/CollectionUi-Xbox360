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

    // Le o arquivo inteiro. E o caminho da capa de ROM: um .jpg solto, que o
    // D3DXCreateTextureFromFileInMemoryEx decodifica direto, sem container no meio.
    bool LerInteiro(const char *caminho, std::vector<unsigned char> &saida)
    {
        HANDLE h = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        DWORD tam = GetFileSize(h, NULL);
        // 8 MB e folga enorme para uma capa de 146x208; o teto existe porque um
        // caminho errado pode cair num .bin de ROM de 600 MB, e ai o console morre
        // na alocacao, nao numa mensagem de erro.
        if (tam == 0xFFFFFFFF || tam == 0 || tam > 8u * 1024u * 1024u)
        {
            CloseHandle(h);
            return false;
        }

        saida.resize(tam);
        DWORD lidos = 0;
        bool ok = (ReadFile(h, &saida[0], tam, &lidos, NULL) != FALSE) && lidos == tam;
        CloseHandle(h);

        if (!ok) saida.clear();
        return ok;
    }

    bool LerCapa(const Pedido &p, std::vector<unsigned char> &saida)
    {
        // Decide pelo CONTEUDO, nao pela extensao: o container da FreeStyle abre com
        // "FSDA", e qualquer outra coisa e imagem solta. Um .assets renomeado continua
        // funcionando, e um .jpg com nome errado nao e interpretado como container.
        std::vector<fsda::Imagem> imagens;
        if (!fsda::Ler(p.arquivo.c_str(), imagens))
            return LerInteiro(p.arquivo.c_str(), saida);

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

                // Rastro dos DOIS lados: so a thread de desenho registrava, entao um
                // crash aqui dentro deixava o log identico ao de um crash la fora.
                // "lendo" sem o "lido" correspondente aponta para esta thread.
                diario::Detalhe("lendo %d: %s", p.indice, p.arquivo.c_str());

                Resultado r;
                r.indice = p.indice;
                if (!LerCapa(p, r.bytes))
                    r.bytes.clear();       // indice sem bytes = falhou, e a tela mostra o vazio

                diario::Detalhe("lido %d: %u bytes", p.indice, (unsigned)r.bytes.size());

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
        // Idempotente na trava e no evento: Parar() acontece antes de lancar um jogo,
        // e se o lancamento falhar Iniciar() e chamado de novo. Reinicializar uma
        // CRITICAL_SECTION viva e vazar um HANDLE de evento a cada tentativa.
        if (g_temPedido == NULL)
        {
            InitializeCriticalSection(&g_trava);
            g_temPedido = CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        g_parar = false;

        g_thread = CreateThread(NULL, 0, Trabalhar, NULL, CREATE_SUSPENDED, NULL);
        if (g_thread == NULL)
        {
            diario::Escrever("ERRO: nao criei a thread de carregamento");
            return;
        }

        // O ponto que faz a diferenca: sem fixar, a thread disputa o mesmo nucleo do
        // laco de desenho e o engasgo continua. O Xenon tem 3 nucleos de 2 threads:
        // 0-1, 2-3, 4-5. O desenho roda na 0, entao a 2 e outro nucleo.
        //
        // ERA 4, e isso derrubava o app. O XAudio2 roda em XAUDIO2_DEFAULT_PROCESSOR,
        // que em xaudio2.h:181 e (XboxThread4|XboxThread5) -- a MESMA thread. Enquanto
        // ficavamos nas colecoes nada acontecia, porque aqui a thread dorme; entrar
        // numa colecao solta a leitura de .assets justo em cima do motor de audio, e
        // era ali, e so ali, que vinha o crash e o som estourado.
        XSetThreadProcessor(g_thread, 2);
        ResumeThread(g_thread);

        diario::Escrever("thread de carregamento no processador %d", 2);
    }

    void Parar()
    {
        g_parar = true;
        if (g_temPedido != NULL)
            SetEvent(g_temPedido);
        if (g_thread != NULL)
        {
            // INFINITE, nao 1000: o unico chamador de Parar() e o lancamento de jogo,
            // e a doc do XDK proibe lancar com I/O de disco pendente. Desistir da
            // espera fechava o HANDLE com a thread viva -- e, se o lancamento
            // falhasse, Iniciar() punha g_parar em falso e a thread velha voltava a
            // consumir a fila ao lado da nova. O worker so demora o tempo de um
            // fsda::LerBytes, que e limitado.
            WaitForSingleObject(g_thread, INFINITE);
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
