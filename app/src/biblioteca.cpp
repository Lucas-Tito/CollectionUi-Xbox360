#include "biblioteca.h"
#include "diario.h"
#include "sqlite3.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>

namespace
{
    // So existem depois de dispositivos::MontarTodos().
    //
    // "game:" NAO entra: no Xbox 360 ele e a pasta de onde o executavel foi lancado,
    // nao a raiz do dispositivo. Varrer ali procuraria uma instalacao do FreeStyle
    // dentro da nossa propria pasta. O drive em que o app esta ja e coberto por Usb0:.
    const char *DISPOSITIVOS[] = { "Hdd:\\", "Usb0:\\", "Usb1:\\", "Usb2:\\" };
    const int   QUANTOS_DISPOSITIVOS = sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0]);

    bool Existe(const char *caminho)
    {
        return GetFileAttributes(caminho) != 0xFFFFFFFF;
    }

    // Teto de sanidade por campo. O maior texto real desta biblioteca nao passa de
    // uma centena de bytes; mil e folga larga.
    //
    // O teto existe porque pagina corrompida devolve ponteiro para regiao SEM
    // terminador, e o construtor de std::string a partir de const char* sai lendo
    // memoria ate achar um zero -- ou ate morrer. Construimos pelo TAMANHO que o
    // proprio sqlite informa, nunca pelo terminador.
    const int TEXTO_MAX = 1024;

    bool Integro(sqlite3 *bd)
    {
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(bd, "pragma quick_check", -1, &st, NULL) != SQLITE_OK)
            return false;

        bool ok = false;
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            const unsigned char *r = sqlite3_column_text(st, 0);
            ok = (r != NULL && strcmp((const char *)r, "ok") == 0);
            if (!ok)
                diario::Escrever("quick_check: %s", r ? (const char *)r : "(nulo)");
        }
        sqlite3_finalize(st);
        return ok;
    }

    std::string Texto(sqlite3_stmt *stmt, int coluna)
    {
        const unsigned char *t = sqlite3_column_text(stmt, coluna);
        if (t == NULL)
            return std::string();

        int n = sqlite3_column_bytes(stmt, coluna);
        if (n < 0)         n = 0;
        if (n > TEXTO_MAX) n = TEXTO_MAX;

        return std::string((const char *)t, (size_t)n);
    }

    // Ordena ignorando o artigo inicial, para "The Darkness" cair no D e nao no T.
    // Mesma regra do prototipo, decidida em docs/interface.md.
    const char *SemArtigo(const char *s)
    {
        static const char *ARTIGOS[] = { "the ", "a ", "an ", "o ", "os ", "as ", "um ", "uma " };
        for (int i = 0; i < 8; i++)
        {
            size_t n = strlen(ARTIGOS[i]);
            if (_strnicmp(s, ARTIGOS[i], (int)n) == 0)
                return s + n;
        }
        return s;
    }

    bool AntesDe(const biblioteca::Jogo &a, const biblioteca::Jogo &b)
    {
        return _stricmp(SemArtigo(a.nome.c_str()), SemArtigo(b.nome.c_str())) < 0;
    }
}

namespace biblioteca
{
    bool Existe(const std::string &caminho)
    {
        return ::GetFileAttributes(caminho.c_str()) != 0xFFFFFFFF;
    }

    void ListarCandidatos(std::vector<Candidato> &saida)
    {
        saida.clear();
        diario::Escrever("procurando instalacoes do FreeStyle:");

        for (int d = 0; d < QUANTOS_DISPOSITIVOS; d++)
        {
            std::string busca = std::string(DISPOSITIVOS[d]) + "*";

            WIN32_FIND_DATA achado;
            HANDLE h = FindFirstFile(busca.c_str(), &achado);
            if (h == INVALID_HANDLE_VALUE)
            {
                diario::Escrever("  %-8s nao abriu (erro %u)", DISPOSITIVOS[d], GetLastError());
                continue;
            }

            do
            {
                if ((achado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    continue;
                if (achado.cFileName[0] == '.')
                    continue;

                std::string raiz = std::string(DISPOSITIVOS[d]) + achado.cFileName;
                std::string banco = raiz + "\\Data\\Databases\\content.db";

                if (Existe(banco))
                {
                    Candidato c;
                    c.caminho = banco;
                    c.rotulo  = raiz;
                    saida.push_back(c);
                    diario::Escrever("  achei: %s", raiz.c_str());
                }
            }
            while (FindNextFile(h, &achado));

            FindClose(h);
        }

        diario::Escrever("instalacoes encontradas: %d", (int)saida.size());
    }

    bool Ler(const char *caminhoBanco, std::vector<Jogo> &saida)
    {
        saida.clear();

        sqlite3 *bd = NULL;
        int r = sqlite3_open_v2(caminhoBanco, &bd, SQLITE_OPEN_READONLY, "xbox");
        if (r != SQLITE_OK)
        {
            diario::Escrever("ERRO: sqlite3_open_v2 = %d (%s)", r, sqlite3_errmsg(bd));
            if (bd != NULL) sqlite3_close(bd);
            return false;
        }

        // O banco esta inteiro? Vale perguntar porque o nosso VFS NAO TEM TRAVA: em
        // vfs_xbox.c, xbTravar e xbChecarTrava sao no-op que respondem sempre "consegui"
        // e "ninguem mais esta mexendo". Com a FreeStyle reescrevendo o content.db num
        // scan, nada impede de lermos pagina pela metade -- e pagina de b-tree
        // corrompida vira deslocamento inventado, que vira ponteiro inventado.
        //
        // Nao e garantia: o banco pode passar aqui e ser reescrito na linha seguinte.
        // E a primeira das tres camadas, nao a unica. Custa pouco: este banco tem
        // 157 KB.
        if (!Integro(bd))
        {
            diario::Escrever("ERRO: banco inconsistente -- a FreeStyle esta varrendo?");
            sqlite3_close(bd);
            return false;
        }

        const char *SQL =
            "select ContentItemId, ContentItemTitleId, ContentItemName, ContentItemGenre,"
            " ContentItemDeveloper, ContentItemPublisher, ContentItemRating,"
            " ContentItemReleaseDate, ContentItemPath, ContentItemFileType,"
            " ContentItemDiscsInSet, ContentItemContentType from ContentItems";

        sqlite3_stmt *stmt = NULL;
        r = sqlite3_prepare_v2(bd, SQL, -1, &stmt, NULL);
        if (r != SQLITE_OK)
        {
            diario::Escrever("ERRO: prepare = %d (%s)", r, sqlite3_errmsg(bd));
            sqlite3_close(bd);
            return false;
        }

        int passo;
        while ((passo = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            Jogo j;
            j.id             = sqlite3_column_int(stmt, 0);
            j.titleId        = (unsigned int)sqlite3_column_int64(stmt, 1);
            j.nome           = Texto(stmt, 2);
            j.genero         = Texto(stmt, 3);
            j.desenvolvedora = Texto(stmt, 4);
            j.publicadora    = Texto(stmt, 5);
            j.nota           = Texto(stmt, 6);
            j.lancamento     = Texto(stmt, 7);
            j.caminho        = Texto(stmt, 8);
            j.tipoArquivo    = sqlite3_column_int(stmt, 9);
            j.discos         = sqlite3_column_int(stmt, 10);
            j.contentType    = sqlite3_column_int(stmt, 11);

            std::string pasta = PastaArte(caminhoBanco, j.id);
            if (!pasta.empty())
            {
                char arq[512];
                _snprintf(arq, sizeof(arq), "%s\\%08X.assets", pasta.c_str(), j.id);
                arq[sizeof(arq) - 1] = '\0';
                j.capa = arq;
            }

            saida.push_back(j);
        }

        // Parar por corrupcao e parar por fim de tabela eram a mesma coisa para este
        // laco. Nao sao: metade da biblioteca e pior que biblioteca nenhuma, porque o
        // usuario nao tem como saber que falta coisa.
        if (passo != SQLITE_DONE)
        {
            diario::Escrever("ERRO: leitura parou em sqlite3_step = %d (%s), %d lidos",
                             passo, sqlite3_errmsg(bd), (int)saida.size());
            sqlite3_finalize(stmt);
            sqlite3_close(bd);
            saida.clear();
            return false;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(bd);

        Ordenar(saida);
        return true;
    }

    // Separada do Ler porque as ROMs de emulador entram DEPOIS, e a tela conta com a
    // lista ordenada: e dela que saem o indice alfabetico e o salto por letra.
    void Ordenar(std::vector<Jogo> &lista)
    {
        // Insercao simples sobre PONTEIROS, nao sobre os itens. O comentario antigo
        // dizia "sao ~120 itens, nao vale trazer <algorithm>" -- a conta mudou: a
        // biblioteca desta casa foi para 418, e insercao e quadratica. Com o item
        // inteiro, cada troca copiava um Jogo de NOVE std::string, o que dava centenas
        // de milhares de alocacoes no arranque. Trocando ponteiro, a troca e um
        // registrador, e so no fim os itens sao movidos uma vez cada.
        if (lista.size() < 2)
            return;

        std::vector<Jogo *> p;
        p.reserve(lista.size());
        for (size_t i = 0; i < lista.size(); i++)
            p.push_back(&lista[i]);

        for (size_t i = 1; i < p.size(); i++)
        {
            Jogo *atual = p[i];
            size_t k = i;
            while (k > 0 && AntesDe(*atual, *p[k - 1]))
            {
                p[k] = p[k - 1];
                k--;
            }
            p[k] = atual;
        }

        std::vector<Jogo> saida;
        saida.reserve(lista.size());
        for (size_t i = 0; i < p.size(); i++)
            saida.push_back(*p[i]);

        lista.swap(saida);
    }

    // Intercala uma lista JA ORDENADA na outra, numa passada. A alternativa --
    // acrescentar no fim e reordenar tudo -- e insercao simples sobre uma cauda
    // desordenada, isto e, quadratica no numero de ROMs. E cada troca copia um Jogo,
    // que tem nove std::string dentro. Com um romset de arcade de uns milhares de zips
    // isso vira dezenas de segundos de tela preta, parecendo console travado.
    void Juntar(std::vector<Jogo> &destino, std::vector<Jogo> &extras)
    {
        Ordenar(extras);

        std::vector<Jogo> saida;
        saida.reserve(destino.size() + extras.size());

        size_t a = 0, b = 0;
        while (a < destino.size() && b < extras.size())
        {
            if (AntesDe(extras[b], destino[a])) saida.push_back(extras[b++]);
            else                                saida.push_back(destino[a++]);
        }
        while (a < destino.size()) saida.push_back(destino[a++]);
        while (b < extras.size())  saida.push_back(extras[b++]);

        destino.swap(saida);
    }

    std::string PastaArte(const std::string &caminhoBanco, int id)
    {
        // <raiz>\Data\Databases\content.db  ->  <raiz>\Data\GameData\<ID em hex>
        size_t corte = caminhoBanco.find("\\Data\\Databases\\");
        if (corte == std::string::npos)
            return std::string();

        char hex[16];
        sprintf(hex, "%08X", id);

        return caminhoBanco.substr(0, corte) + "\\Data\\GameData\\" + hex;
    }
}
