#include "lancador.h"
#include "dispositivos.h"
#include "diario.h"
#include <xtl.h>
#include <stdio.h>

namespace
{
    struct Pedido
    {
        char  caminho[512];
        char  imagem[32];
        int   tipo;
        DWORD codigo;        // da segunda tentativa, ou da unica
        DWORD codigo1;       // da primeira, no caminho curto
        bool  tentouLongo;
    };

    // Lancar de uma thread separada nao e zelo: a doc do XDK diz que estas funcoes
    // "cannot be called from a thread that owns the D3D device; it must be called from
    // a separate thread, or the D3D device must be destroyed first".
    DWORD WINAPI Disparar(LPVOID bruto)
    {
        Pedido *p = (Pedido *)bruto;

        if (p->tipo == 3)
        {
            // Devolve zero no sucesso -- mas no sucesso o console reinicia e ninguem
            // le este retorno. Na pratica so voltamos daqui com erro.
            p->codigo1 = XContentLaunchImageFromFile(p->caminho, p->imagem);
            p->codigo  = p->codigo1;
            if (p->codigo == 0)
                return 0;

            // Erro de "nao achei o caminho" com o arquivo existindo (a sondagem acha
            // antes de chegar aqui) aponta para a FORMA do caminho. "Hdd:" e um apelido
            // curto, e quem o expande depende de QUEM resolve: a XAPI dentro do nosso
            // titulo prefixa "\??\" sozinha -- por isso o GetFileAttributes acha --,
            // mas esta chamada atravessa para o xam, que resolve noutro contexto. Um
            // caminho absoluto do namespace de objetos nao depende de prefixo nenhum.
            //
            // E a MESMA forma que o FreeStyle usa para montar container, e o apelido
            // dele tambem e caseiro: "\??\Hdd1:\...".
            //
            // NADA de escrever no log entre as duas chamadas: a doc proibe I/O com o
            // disco antes de lancar. Os dois codigos vao no Pedido e sao registrados
            // depois, por quem esperou a thread.
            if (p->codigo == ERROR_FILE_NOT_FOUND ||
                p->codigo == ERROR_PATH_NOT_FOUND ||
                p->codigo == ERROR_INVALID_NAME)
            {
                char outro[512];
                _snprintf(outro, sizeof(outro), "\\??\\%s", p->caminho);
                outro[sizeof(outro) - 1] = '\0';

                p->tentouLongo = true;
                p->codigo = XContentLaunchImageFromFile(outro, p->imagem);
            }
            return 0;
        }

        // ATENCAO: XLaunchNewImage e DECLSPEC_NORETURN, e isto nao e teoria -- o
        // proprio cl.exe recusou o "return 0;" que estava aqui com C4702, unreachable
        // code. Em Release o otimizador APAGA o que vier depois desta linha, entao
        // tratamento de erro escrito abaixo dela e ficcao. Todo diagnostico vem antes.
        XLaunchNewImage(p->caminho, 0);
    }
}

namespace lancador
{
    std::string Resolver(const std::string &caminhoRelativo)
    {
        int quantos = 0;
        const char *const *apelidos = dispositivos::Apelidos(&quantos);

        for (int i = 0; i < quantos; i++)
        {
            // INVALID_FILE_ATTRIBUTES nao existe nos headers do Xbox; o resto do
            // codigo ja compara com 0xFFFFFFFF na mao, em biblioteca::Existe.
            std::string tentativa = std::string(apelidos[i]) + caminhoRelativo;
            if (biblioteca::Existe(tentativa))
                return tentativa;
        }
        return std::string();
    }

    bool Lancar(const biblioteca::Jogo &jogo, std::string &erro)
    {
        std::string caminho = Resolver(jogo.caminho);
        if (caminho.empty())
        {
            erro = "Arquivo nao encontrado em nenhum dispositivo";
            diario::Escrever("lancar: nao achei '%s' em dispositivo nenhum",
                             jogo.caminho.c_str());
            return false;
        }

        Pedido p;
        ZeroMemory(&p, sizeof(p));

        // Atributos crus do caminho ANTES de lancar: biblioteca::Existe devolve true
        // para DIRETORIO tambem, entao "o arquivo existe" nao prova que e arquivo.
        DWORD atributos = GetFileAttributes(caminho.c_str());
        _snprintf(p.caminho, sizeof(p.caminho), "%s", caminho.c_str());
        p.caminho[sizeof(p.caminho) - 1] = '\0';
        p.tipo   = jogo.tipoArquivo;
        p.codigo = 0;

        // Dentro do container, o executavel e default.xbe se o pacote for de Xbox
        // original (XCONTENTTYPE_XBOXTITLE), default.xex no resto.
        strcpy(p.imagem, (jogo.contentType == 0x5000) ? "default.xbe" : "default.xex");

        // O ultimo registro tem de sair ANTES: dando certo, o console reinicia e
        // nenhuma linha escrita depois chega ao arquivo.
        diario::Escrever("lancando tipo=%d contentType=0x%X attr=0x%08X: %s (%s)",
                         jogo.tipoArquivo, jogo.contentType, (unsigned)atributos,
                         caminho.c_str(),
                         (jogo.tipoArquivo == 3) ? p.imagem : "direto");
        diario::Fechar();

        HANDLE h = CreateThread(NULL, 0, Disparar, &p, 0, NULL);
        if (h == NULL)
        {
            diario::Reabrir();
            erro = "Nao consegui criar a thread de lancamento";
            return false;
        }

        // Dando certo, esta espera nunca termina -- o console ja foi embora.
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);

        // Chegar aqui e falha, por definicao.
        diario::Reabrir();

        char texto[128];
        if (jogo.tipoArquivo == 3)
        {
            _snprintf(texto, sizeof(texto), "O jogo nao abriu (erro 0x%08X)",
                      (unsigned)p.codigo);
            if (p.tentouLongo)
                diario::Escrever("FALHOU container: caminho curto 0x%08X, \\??\\ 0x%08X, em %s",
                                 (unsigned)p.codigo1, (unsigned)p.codigo, caminho.c_str());
            else
                diario::Escrever("FALHOU container: 0x%08X em %s",
                                 (unsigned)p.codigo, caminho.c_str());
        }
        else
        {
            _snprintf(texto, sizeof(texto), "O jogo nao abriu");
            diario::Escrever("FALHOU: XLaunchNewImage voltou, o que nao deveria: %s",
                             caminho.c_str());
        }
        texto[sizeof(texto) - 1] = '\0';
        erro = texto;
        return false;
    }
}
