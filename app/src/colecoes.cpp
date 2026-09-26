#include "colecoes.h"
#include "diario.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace
{
    const char *ARQUIVO = "game:\\colecoes.txt";

    // Ponteiros, não valores: main.cpp guarda o ponteiro da coleção aberta ENTRE
    // QUADROS, e o endereço de um elemento de vector<Colecao> morre no primeiro
    // push_back. Guardando ponteiros, o endereço de cada coleção é estável.
    std::vector<colecoes::Colecao *> g_lista;

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
        for (size_t i = 0; i < g_lista.size(); i++)
            delete g_lista[i];
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

            Colecao *c = new Colecao();
            c->nome = linha;

            for (char *p = strtok(barra + 1, ","); p != NULL; p = strtok(NULL, ","))
            {
                // strtoul e base 16, não atoi. TitleId usa os 32 bits: o do Snes360 é
                // 0xFFED0707, que estoura int e voltaria negativo -- e a validação
                // antiga ("> 0") o descartaria em silêncio a cada releitura.
                unsigned int titleId = (unsigned int)strtoul(p, NULL, 16);
                if (titleId != 0)
                    c->ids.push_back(titleId);
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

        fprintf(f, "# CollectionUI: uma colecao por linha, nome|TitleIds em hexa, por virgula\n");
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            fprintf(f, "%s|", g_lista[i]->nome.c_str());
            for (size_t k = 0; k < g_lista[i]->ids.size(); k++)
                fprintf(f, "%s%08X", k ? "," : "", g_lista[i]->ids[k]);
            fputc('\n', f);
        }
        fclose(f);
    }

    std::vector<Colecao *> Ordenadas()
    {
        std::vector<Colecao *> saida = g_lista;

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

    std::string Sanear(const std::string &nome)
    {
        std::string s = nome;

        for (size_t i = 0; i < s.size(); i++)
            if (s[i] == '|' || s[i] == '\n' || s[i] == '\r')
                s[i] = ' ';

        // 28 BYTES, recuando até não partir uma sequência UTF-8: um nome cortado no
        // meio de um caractere faz o MultiByteToWideChar falhar, e a caixa da coleção
        // apareceria sem nome nenhum.
        if (s.size() > 28)
        {
            size_t corte = 28;
            while (corte > 0 && ((unsigned char)s[corte] & 0xC0) == 0x80)
                corte--;
            s = s.substr(0, corte);
        }
        return s;
    }

    // O ponteiro devolvido é estável: g_lista guarda ponteiros, então nem push_back
    // nem erase mexem no endereço das outras coleções.
    Colecao *Criar(const std::string &nome)
    {
        Colecao *c = new Colecao();
        c->nome = Sanear(nome);
        g_lista.push_back(c);
        Gravar();
        return c;
    }

    void Apagar(Colecao *c)
    {
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            if (g_lista[i] == c)
            {
                delete g_lista[i];
                g_lista.erase(g_lista.begin() + i);
                Gravar();
                return;
            }
        }
    }

    bool Tem(const Colecao *c, unsigned int titleId)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
            if (c->ids[i] == titleId)
                return true;
        return false;
    }

    void Alternar(Colecao *c, unsigned int titleId)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
        {
            if (c->ids[i] == titleId)
            {
                c->ids.erase(c->ids.begin() + i);
                return;
            }
        }
        c->ids.push_back(titleId);
    }

    void Remover(Colecao *c, unsigned int titleId)
    {
        for (size_t i = 0; i < c->ids.size(); i++)
        {
            if (c->ids[i] == titleId)
            {
                c->ids.erase(c->ids.begin() + i);
                Gravar();
                return;
            }
        }
    }
}
