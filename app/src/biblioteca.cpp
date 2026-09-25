#include "biblioteca.h"
#include "diario.h"
#include "sqlite3.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>

namespace
{
    const char *DISPOSITIVOS[] = { "Hdd1:\\", "Hdd:\\", "Usb0:\\", "Usb1:\\", "Usb2:\\" };
    const int   QUANTOS_DISPOSITIVOS = sizeof(DISPOSITIVOS) / sizeof(DISPOSITIVOS[0]);

    bool Existe(const char *caminho)
    {
        return GetFileAttributes(caminho) != 0xFFFFFFFF;
    }

    std::string Texto(sqlite3_stmt *stmt, int coluna)
    {
        const unsigned char *t = sqlite3_column_text(stmt, coluna);
        return (t == NULL) ? std::string() : std::string((const char *)t);
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
    bool AcharBanco(std::string &caminhoSaida)
    {
        for (int d = 0; d < QUANTOS_DISPOSITIVOS; d++)
        {
            std::string busca = std::string(DISPOSITIVOS[d]) + "*";

            WIN32_FIND_DATA achado;
            HANDLE h = FindFirstFile(busca.c_str(), &achado);
            if (h == INVALID_HANDLE_VALUE)
                continue;

            do
            {
                if ((achado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    continue;
                if (achado.cFileName[0] == '.')
                    continue;

                std::string candidato = std::string(DISPOSITIVOS[d]) + achado.cFileName +
                                        "\\Data\\Databases\\content.db";
                if (Existe(candidato.c_str()))
                {
                    diario::Escrever("banco encontrado: %s", candidato.c_str());
                    caminhoSaida = candidato;
                    FindClose(h);
                    return true;
                }
            }
            while (FindNextFile(h, &achado));

            FindClose(h);
        }
        return false;
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

        const char *SQL =
            "select ContentItemId, ContentItemTitleId, ContentItemName, ContentItemGenre,"
            " ContentItemDeveloper, ContentItemPublisher, ContentItemRating,"
            " ContentItemReleaseDate, ContentItemPath, ContentItemFileType,"
            " ContentItemDiscsInSet from ContentItems";

        sqlite3_stmt *stmt = NULL;
        r = sqlite3_prepare_v2(bd, SQL, -1, &stmt, NULL);
        if (r != SQLITE_OK)
        {
            diario::Escrever("ERRO: prepare = %d (%s)", r, sqlite3_errmsg(bd));
            sqlite3_close(bd);
            return false;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW)
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
            saida.push_back(j);
        }

        sqlite3_finalize(stmt);
        sqlite3_close(bd);

        // Insercao simples: sao ~120 itens, nao vale trazer <algorithm> para isto.
        for (size_t i = 1; i < saida.size(); i++)
        {
            Jogo atual = saida[i];
            size_t k = i;
            while (k > 0 && AntesDe(atual, saida[k - 1]))
            {
                saida[k] = saida[k - 1];
                k--;
            }
            saida[k] = atual;
        }

        return true;
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
