#include "fichas.h"
#include "diario.h"

#include <xtl.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fichas
{
    namespace
    {
        std::map<unsigned int, Ficha> g_fichas;

        void Limpar(Ficha &f)
        {
            f.coop = 0; f.metacritic = 0; f.notaJogadores = 0.0f; f.ano = 0;
            f.horas[0] = f.horas[1] = f.horas[2] = 0.0f;
        }

        // Um campo "chave=valor". Chave desconhecida é ignorada: o gerador pode ganhar
        // campo novo antes do app.
        void Campo(Ficha &f, const char *campo)
        {
            const char *igual = strchr(campo, '=');
            if (igual == NULL) return;
            const size_t n = (size_t)(igual - campo);
            const char *v = igual + 1;
            if      (n == 4 && strncmp(campo, "coop", 4) == 0) f.coop = atoi(v);
            else if (n == 2 && strncmp(campo, "cx", 2) == 0)   f.coopExtras = v;
            else if (n == 2 && strncmp(campo, "ct", 2) == 0)   f.coopTexto = v;
            else if (n == 2 && strncmp(campo, "mc", 2) == 0)   f.metacritic = atoi(v);
            else if (n == 2 && strncmp(campo, "nj", 2) == 0)   f.notaJogadores = (float)atof(v);
            else if (n == 2 && strncmp(campo, "hm", 2) == 0)   f.horas[0] = (float)atof(v);
            else if (n == 2 && strncmp(campo, "hp", 2) == 0)   f.horas[1] = (float)atof(v);
            else if (n == 2 && strncmp(campo, "hc", 2) == 0)   f.horas[2] = (float)atof(v);
            else if (n == 3 && strncmp(campo, "ano", 3) == 0)  f.ano = atoi(v);
            else if (n == 3 && strncmp(campo, "gen", 3) == 0)  f.genero = v;
            else if (n == 3 && strncmp(campo, "dev", 3) == 0)  f.dev = v;
            else if (n == 3 && strncmp(campo, "pub", 3) == 0)  f.pub = v;
            else if (n == 4 && strncmp(campo, "desc", 4) == 0) f.descricao = v;
        }
    }

    int Carregar(const char *caminho)
    {
        g_fichas.clear();
        FILE *arq = fopen(caminho, "rb");
        if (arq == NULL)
        {
            diario::Escrever("fichas: sem %s (a faixa de detalhes mostra so o da FreeStyle)", caminho);
            return 0;
        }

        // Uma linha passa de 1 KB com a descrição inteira; 4 KB sobra. Linha maior que
        // isso seria cortada no meio de um campo, então é descartada inteira.
        static char linha[4096];
        int lidas = 0, longas = 0;
        while (fgets(linha, sizeof(linha), arq) != NULL)
        {
            size_t n = strlen(linha);
            if (n > 0 && linha[n - 1] != '\n' && !feof(arq))
            {
                longas++;
                int c;
                while ((c = fgetc(arq)) != EOF && c != '\n') {}
                continue;
            }
            while (n > 0 && (linha[n - 1] == '\n' || linha[n - 1] == '\r')) linha[--n] = '\0';
            if (n == 0 || linha[0] == '#') continue;

            char *tab = strchr(linha, '\t');
            if (tab == NULL) continue;
            *tab = '\0';
            const unsigned int tid = (unsigned int)strtoul(linha, NULL, 16);
            if (tid == 0) continue;

            Ficha f;
            Limpar(f);
            char *p = tab + 1;
            while (p != NULL && *p != '\0')
            {
                char *prox = strchr(p, '\t');
                if (prox != NULL) *prox++ = '\0';
                Campo(f, p);
                p = prox;
            }
            g_fichas[tid] = f;
            lidas++;
        }
        fclose(arq);
        diario::Escrever("fichas: %d lidas de %s%s", lidas, caminho, longas ? " (alguma linha longa demais foi pulada)" : "");
        return lidas;
    }

    const Ficha *Por(unsigned int titleId)
    {
        std::map<unsigned int, Ficha>::const_iterator i = g_fichas.find(titleId);
        return (i == g_fichas.end()) ? NULL : &i->second;
    }
}
