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

namespace
{
    // NORETURN como a de cima: sai para a dashboard e nunca volta.
    DWORD WINAPI ParaDashboard(LPVOID)
    {
        XLaunchNewImage(NULL, 0);
    }
}

namespace
{
    // ---- indies (Xbox Live Indie Games) -----------------------------------------
    //
    // Um indie NAO tem xex: o pacote traz o GameInfo.xml e os assemblies XNA, e e do
    // titulo 584E07D2 (o hospedeiro "Community Game"), tipo 00000002 (marketplace).
    // XContentLaunchImageFromFile(pacote, "default.xex") falhava por isso, enquanto a
    // FreeStyle abria normalmente.
    //
    // Quem abre e o Xna_TitleLauncher.xex da particao estendida do sistema, e o jogo
    // vai pelos dados de lancamento: 0xCAFEBABE, o nome-hash do pacote e o id do
    // aparelho onde ele esta (0x34 bytes). E o que o Dash Launch faz no instalador
    // (installer/dashLaunch.cpp, LaunchItem; a struct e a XLAUNCHDATA_CAFEBABE do
    // xkelib, _xamext.h), e o XeniOS reproduz o mesmo formato (indie_game.cc).
    //
    // O pacote PRECISA estar na pasta de conteudo do 584E07D2/00000002: o lancador o
    // procura por (aparelho, nome) la dentro.
    const char INDIE[] = "\\Content\\0000000000000000\\584E07D2\\00000002\\";

    struct DadosIndie
    {
        DWORD id;            // 0x00 = 0xCAFEBABE
        char  nome[0x2C];    // 0x04 = o nome-hash do arquivo do pacote
        DWORD aparelho;      // 0x30 = XCONTENTDEVICEID
    };

    // O XDK 2.0.21256 nao tem o enumerador de aparelhos (XContentCreateDeviceEnumerator
    // e do xkelib); tem o XContentGetDeviceData, que responde por id. Os ids sao
    // pequenos: sonda-se de 1 a 32 e fica o do TIPO pedido. Pendrive: so se houver um.
    DWORD AparelhoDoTipo(XCONTENTDEVICETYPE tipo)
    {
        DWORD achado = 0xFFFFFFFF;
        int quantos = 0;
        for (DWORD id = 1; id <= 32; id++)
        {
            XDEVICE_DATA d;
            ZeroMemory(&d, sizeof(d));
            if (XContentGetDeviceData(id, &d) != ERROR_SUCCESS) continue;
            diario::Escrever("indie: aparelho id=%u tipo=%u", (unsigned)id, (unsigned)d.DeviceType);
            if (d.DeviceType == tipo) { achado = id; quantos++; }
        }
        if (tipo != XCONTENTDEVICETYPE_HDD && quantos > 1)
        {
            diario::Escrever("indie: %d aparelhos do tipo %u, nao sei qual", quantos, (unsigned)tipo);
            return 0xFFFFFFFF;
        }
        return achado;
    }

    // A pasta do lancador e a versao do sistema (32000100 neste console, o numero que o
    // Dash Launch fixa); procura em vez de fixar, para nao quebrar noutra versao.
    bool AcharLancadorXna(char *saida, size_t tam)
    {
        if (!dispositivos::Montar("xsep:", "\\Device\\Harddisk0\\SystemExtPartition"))
        {
            diario::Escrever("indie: nao montou xsep:");
            return false;
        }
        _snprintf(saida, tam, "xsep:\\32000100\\Xna_TitleLauncher.xex");
        if (GetFileAttributes(saida) != 0xFFFFFFFF) return true;

        WIN32_FIND_DATA f;
        HANDLE h = FindFirstFile("xsep:\\*", &f);
        if (h == INVALID_HANDLE_VALUE) return false;
        bool ok = false;
        do
        {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
            _snprintf(saida, tam, "xsep:\\%s\\Xna_TitleLauncher.xex", f.cFileName);
            if (GetFileAttributes(saida) != 0xFFFFFFFF) { ok = true; break; }
        } while (FindNextFile(h, &f));
        FindClose(h);
        return ok;
    }
}

namespace lancador
{
    bool EhIndie(const biblioteca::Jogo &jogo)
    {
        return _strnicmp(jogo.caminho.c_str(), INDIE, sizeof(INDIE) - 1) == 0;
    }

    void Dashboard()
    {
        diario::Escrever("saindo para a dashboard");
        diario::Fechar();

        HANDLE h = CreateThread(NULL, 0, ParaDashboard, NULL, 0, NULL);
        if (h == NULL)
        {
            diario::Reabrir();
            diario::Escrever("ERRO: sem thread para sair para a dashboard");
            return;
        }
        WaitForSingleObject(h, INFINITE);   // dando certo, nunca termina
        CloseHandle(h);
        diario::Reabrir();
        diario::Escrever("ERRO: XLaunchNewImage(NULL) voltou");
    }

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
            erro = "File not found on any device";
            diario::Escrever("lancar: nao achei '%s' em dispositivo nenhum",
                             jogo.caminho.c_str());
            return false;
        }

        Pedido p;
        ZeroMemory(&p, sizeof(p));

        if (EhIndie(jogo))
        {
            const char *nome = jogo.caminho.c_str() + sizeof(INDIE) - 1;
            const XCONTENTDEVICETYPE tipo = (_strnicmp(caminho.c_str(), "Hdd:", 4) == 0)
                                          ? XCONTENTDEVICETYPE_HDD : XCONTENTDEVICETYPE_MU;
            char lancadorXna[MAX_PATH];
            DadosIndie d;
            ZeroMemory(&d, sizeof(d));
            d.id = 0xCAFEBABE;
            d.aparelho = AparelhoDoTipo(tipo);
            if (strlen(nome) >= sizeof(d.nome) || strchr(nome, '\\') != NULL)
            {
                erro = "The indie package has an unexpected name";
                diario::Escrever("indie: nome do pacote fora do formato: %s", nome);
                return false;
            }
            strcpy(d.nome, nome);
            if (d.aparelho == 0xFFFFFFFF)
            {
                erro = "Couldn't find the indie's storage device";
                return false;
            }
            if (!AcharLancadorXna(lancadorXna, sizeof(lancadorXna)))
            {
                erro = "The system's Xna_TitleLauncher.xex is missing";
                diario::Escrever("indie: Xna_TitleLauncher.xex nao encontrado em xsep:");
                return false;
            }
            DWORD r = XSetLaunchData(&d, sizeof(d));
            diario::Escrever("indie: %s aparelho=%u pacote=%s XSetLaunchData=0x%08X",
                             lancadorXna, (unsigned)d.aparelho, d.nome, (unsigned)r);
            if (r != ERROR_SUCCESS)
            {
                erro = "The system refused the indie's launch data";
                return false;
            }
            // Daqui em diante e o caminho do xex solto (tipo 1): XLaunchNewImage com o
            // lancador, flags 0, que MANTEM os dados de lancamento.
            _snprintf(p.caminho, sizeof(p.caminho), "%s", lancadorXna);
            p.caminho[sizeof(p.caminho) - 1] = '\0';
            p.tipo = 1;
            diario::Escrever("lancando indie: %s", jogo.caminho.c_str());
            diario::Fechar();

            HANDLE h = CreateThread(NULL, 0, Disparar, &p, 0, NULL);
            if (h == NULL) { diario::Reabrir(); erro = "Couldn't create the launch thread"; return false; }
            WaitForSingleObject(h, INFINITE);   // dando certo, nunca termina
            CloseHandle(h);
            diario::Reabrir();
            diario::Escrever("FALHOU: XLaunchNewImage do lancador XNA voltou: %s", lancadorXna);
            erro = "The indie game didn't start";
            return false;
        }

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
            erro = "Couldn't create the launch thread";
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
            _snprintf(texto, sizeof(texto), "The game didn't start (error 0x%08X)",
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
            _snprintf(texto, sizeof(texto), "The game didn't start");
            diario::Escrever("FALHOU: XLaunchNewImage voltou, o que nao deveria: %s",
                             caminho.c_str());
        }
        texto[sizeof(texto) - 1] = '\0';
        erro = texto;
        return false;
    }
}
