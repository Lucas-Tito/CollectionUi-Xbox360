#include "colecoes.h"
#include "diario.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace
{
    const char *ARQUIVO = "game:\\colecoes.txt";
    std::vector<colecoes::Colecao> g_lista;

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
}

namespace colecoes
{
    void Carregar()
    {
        g_lista.clear();

        FILE *f = fopen(ARQUIVO, "r");
        if (f == NULL)
        {
            diario::Escrever("sem colecoes gravadas (%s)", ARQUIVO);
            return;
        }

        char linha[4096];
        while (fgets(linha, sizeof(linha), f) != NULL)
        {
            char *fim = linha + strlen(linha);
            while (fim > linha && (fim[-1] == '\n' || fim[-1] == '\r'))
                *--fim = '\0';
            if (linha[0] == '\0' || linha[0] == '#')
                continue;

            char *barra = strchr(linha, '|');
            if (barra == NULL)
                continue;
            *barra = '\0';

            Colecao c;
            c.nome = linha;

            for (char *p = strtok(barra + 1, ","); p != NULL; p = strtok(NULL, ","))
            {
                int id = atoi(p);
                if (id > 0)
                    c.ids.push_back(id);
            }
            g_lista.push_back(c);
        }
        fclose(f);
        diario::Escrever("colecoes carregadas: %d", (int)g_lista.size());
    }

    void Gravar()
    {
        FILE *f = fopen(ARQUIVO, "w");
        if (f == NULL)
        {
            diario::Escrever("AVISO: nao consegui gravar %s", ARQUIVO);
            return;
        }

        fprintf(f, "# CollectionUI: uma colecao por linha, nome|ids separados por virgula\n");
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            fprintf(f, "%s|", g_lista[i].nome.c_str());
            for (size_t k = 0; k < g_lista[i].ids.size(); k++)
                fprintf(f, "%s%d", k ? "," : "", g_lista[i].ids[k]);
            fputc('\n', f);
        }
        fclose(f);
    }

    std::vector<Colecao *> Ordenadas()
    {
        std::vector<Colecao *> saida;
        for (size_t i = 0; i < g_lista.size(); i++)
            saida.push_back(&g_lista[i]);

        // Insercao: sao poucas, e evita trazer <algorithm> so para isto.
        for (size_t i = 1; i < saida.size(); i++)
        {
            Colecao *atual = saida[i];
            size_t k = i;
            while (k > 0 && _stricmp(SemArtigo(atual->nome.c_str()),
                                     SemArtigo(saida[k-1]->nome.c_str())) < 0)
            {
                saida[k] = saida[k-1];
                k--;
            }
            saida[k] = atual;
        }
        return saida;
    }

    Colecao *Criar(const std::string &nome)
    {
        Colecao c;
        c.nome = nome.substr(0, 28);
        g_lista.push_back(c);
        Gravar();
        // O vector pode ter realocado: devolve pelo nome, nao por ponteiro antigo.
        return &g_lista[g_lista.size() - 1];
    }

    void Apagar(Colecao *c)
    {
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            if (&g_lista[i] == c)
            {
                g_lista.erase(g_lista.begin() + i);
                Gravar();
                return;
            }
        }
    }

    bool Tem(const Colecao *c, int id)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
            if (c->ids[i] == id)
                return true;
        return false;
    }

    void Alternar(Colecao *c, int id)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
        {
            if (c->ids[i] == id)
            {
                c->ids.erase(c->ids.begin() + i);
                return;
            }
        }
        c->ids.push_back(id);
    }

    void Remover(Colecao *c, int id)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
        {
            if (c->ids[i] == id)
            {
                c->ids.erase(c->ids.begin() + i);
                Gravar();
                return;
            }
        }
    }
}
