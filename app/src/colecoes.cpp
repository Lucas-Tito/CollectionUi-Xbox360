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
    Colecao *PorId(int id)
    {
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id == id)
                return g_lista[i];
        return NULL;
    }

    // Tira das unioes os ids que nao existem mais, e APAGA a uniao que ficou sem
    // nenhuma origem: uniao sem origem nao e colecao vazia, e colecao que perdeu o
    // sentido. Repete ate estabilizar, porque apagar uma pode esvaziar outra -- nao
    // acontece hoje (uniao nao contem uniao), mas custa nada ficar certo.
    void LimparUnioes()
    {
        bool mexeu = true;
        while (mexeu)
        {
            mexeu = false;
            for (size_t i = 0; i < g_lista.size(); i++)
            {
                Colecao *c = g_lista[i];
                if (!c->uniao) continue;

                for (size_t k = 0; k < c->origens.size(); )
                {
                    // Fora id morto E origem que e uniao: a grade de escolha nao
                    // mostra uniao, entao uma dessas so entra por arquivo editado a
                    // mao -- e ficaria contada no rodape sem haver como desmarcar.
                    const Colecao *o = PorId(c->origens[k]);
                    if (o == NULL || o->uniao)
                    {
                        c->origens.erase(c->origens.begin() + k);
                        mexeu = true;
                    }
                    else k++;
                }

                if (c->origens.empty())
                {
                    diario::Escrever("uniao '%s' ficou sem origem e foi apagada",
                                     c->nome.c_str());
                    delete c;
                    g_lista.erase(g_lista.begin() + i);
                    mexeu = true;
                    break;
                }
            }
        }
    }

    // Lê uma linha de QUALQUER tamanho.
    //
    // Com char[4096] uma coleção grande se destruía sozinha: 600 TitleIds em hexa dão
    // 5.415 bytes, o fgets cortava no meio do 455º token, o id partido virava um número
    // de lixo e os 145 restantes voltavam como uma linha sem '|', descartada em
    // silêncio. E como todo Gravar reescreve o arquivo inteiro a partir do que foi
    // carregado, a perda de leitura virava perda definitiva no disco.
    bool LerLinha(FILE *f, std::string &saida)
    {
        char pedaco[512];
        saida.clear();

        while (fgets(pedaco, sizeof(pedaco), f) != NULL)
        {
            saida += pedaco;
            if (saida[saida.size() - 1] == '\n')
                return true;
        }
        return !saida.empty();      // última linha sem quebra no fim
    }

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

        std::string linha;
        while (LerLinha(f, linha))
        {
            while (!linha.empty() &&
                   (linha[linha.size() - 1] == '\n' || linha[linha.size() - 1] == '\r'))
                linha.erase(linha.size() - 1);

            // O que separa coleção de comentário é a BARRA, não o '#'. Tratar '#' como
            // comentário fazia uma coleção chamada "#1 favoritos" sumir inteira na
            // releitura -- e o teclado do sistema deixa digitar '#'. Por isso o
            // cabeçalho que Gravar escreve não contém barra nenhuma.
            size_t barra = linha.find('|');
            if (barra == std::string::npos)
                continue;

            Colecao *c = new Colecao();
            c->id = 0;
            c->uniao = false;

            // Formato NOVO:   id|tipo|nome|conteudo
            // Formato ANTIGO: nome|TitleIds
            //
            // Distinguem-se pelo comeco: o novo abre com numero seguido de barra. Linha
            // antiga ganha id na leitura e e regravada no formato novo no proximo
            // salvamento -- a migracao nao pede nada do usuario.
            //
            // So abrir com numero NAO basta para dizer que a linha e do formato novo:
            // "1942|FFED0707,..." e do antigo, e uma colecao chamada 1942 nao e
            // esquisitice nenhuma num app de jogos. O segundo campo e que decide --
            // no formato antigo ali vao TitleIds em hexa, nunca a palavra jogos nem
            // uniao.
            std::string ids;
            size_t b2 = linha.find('|', barra + 1);
            std::string tipo = (b2 == std::string::npos)
                             ? std::string()
                             : linha.substr(barra + 1, b2 - barra - 1);
            bool novo = (barra > 0
                         && linha.find_first_not_of("0123456789") == barra
                         && (tipo == "jogos" || tipo == "uniao"));

            if (novo)
            {
                c->id = atoi(linha.substr(0, barra).c_str());
                c->uniao = (tipo == "uniao");

                size_t b3 = linha.find('|', b2 + 1);
                if (b3 == std::string::npos)
                {
                    // Linha cortada no meio (queda de energia durante o Gravar). Salva
                    // o nome e perde o conteudo -- que o usuario ve e refaz. NUNCA
                    // descartar a linha: Gravar reescreve o arquivo inteiro a partir
                    // do que foi lido, entao descartar aqui apaga do disco em silencio.
                    c->nome = linha.substr(b2 + 1);
                }
                else
                {
                    c->nome = linha.substr(b2 + 1, b3 - b2 - 1);
                    ids     = linha.substr(b3 + 1);
                }
            }
            else
            {
                c->nome = linha.substr(0, barra);
                ids     = linha.substr(barra + 1);
            }

            std::vector<char> mut(ids.begin(), ids.end());
            mut.push_back('\0');

            for (char *p = strtok(&mut[0], ","); p != NULL; p = strtok(NULL, ","))
            {
                // strtoul e base 16, não atoi. TitleId usa os 32 bits: o do Snes360 é
                // 0xFFED0707, que estoura int e voltaria negativo -- e a validação
                // antiga ("> 0") o descartaria em silêncio a cada releitura.
                if (c->uniao)
                {
                    // Origem e id de colecao, em DECIMAL.
                    int origem = atoi(p);
                    if (origem > 0)
                        c->origens.push_back(origem);
                }
                else
                {
                    unsigned int titleId = (unsigned int)strtoul(p, NULL, 16);
                    if (titleId != 0)
                        c->ids.push_back(titleId);
                }
            }
            g_lista.push_back(c);
        }
        fclose(f);

        // Quem veio do formato antigo ganha id agora, acima do maior ja usado.
        int maior = 0;
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id > maior) maior = g_lista[i]->id;
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id == 0) g_lista[i]->id = ++maior;

        LimparUnioes();

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

        // Sem barra nestas linhas, de proposito: e o que as distingue de uma colecao.
        fprintf(f, "# CollectionUI: uma colecao por linha, no formato\n");
        fprintf(f, "#   id, tipo, nome, conteudo -- separados por barra vertical\n");
        fprintf(f, "#   tipo jogos: TitleIds em hexa. tipo uniao: ids de colecao\n");

        for (size_t i = 0; i < g_lista.size(); i++)
        {
            const Colecao *c = g_lista[i];
            fprintf(f, "%d|%s|%s|", c->id, c->uniao ? "uniao" : "jogos", c->nome.c_str());

            if (c->uniao)
                for (size_t k = 0; k < c->origens.size(); k++)
                    fprintf(f, "%s%d", k ? "," : "", c->origens[k]);
            else
                for (size_t k = 0; k < c->ids.size(); k++)
                    fprintf(f, "%s%08X", k ? "," : "", c->ids[k]);

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
    int ProximoId()
    {
        int maior = 0;
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id > maior) maior = g_lista[i]->id;
        return maior + 1;
    }

    Colecao *Criar(const std::string &nome)
    {
        Colecao *c = new Colecao();
        c->id = ProximoId();
        c->uniao = false;
        c->nome = Sanear(nome);
        g_lista.push_back(c);
        Gravar();
        return c;
    }

    Colecao *CriarUniao(const std::string &nome)
    {
        Colecao *c = new Colecao();
        c->id = ProximoId();
        c->uniao = true;
        c->nome = Sanear(nome);
        g_lista.push_back(c);
        // NAO grava aqui: uniao sem origem seria apagada na proxima leitura. Quem chama
        // grava depois de escolher as origens.
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
                // Antes de gravar: senao o id apagado fica pendurado em alguma uniao
                // no disco, e o proximo ProximoId() pode devolver esse mesmo id a uma
                // colecao nova -- que entraria na uniao sem ninguem ter pedido.
                LimparUnioes();
                Gravar();
                return;
            }
        }
    }

    bool Tem(const Colecao *c, unsigned int titleId)
    {
        if (c->uniao)
        {
            // Pergunta a cada origem. A chamada de baixo NAO volta a entrar neste ramo:
            // so se desce em origem que nao e uniao, e esse teste e o guarda de ciclo --
            // um colecoes.txt editado a mao com uma uniao apontando pra si mesma para
            // aqui em vez de recorrer sem fim.
            //
            // Responde por pergunta em vez de devolver a lista toda porque os tres
            // chamadores varrem a biblioteca e so querem saber "este jogo entra?".
            // Montar o vetor resolvido a cada um deles seria alocacao dentro do laco
            // de desenho.
            for (size_t i = 0; i < c->origens.size(); i++)
            {
                const Colecao *o = PorId(c->origens[i]);
                if (o != NULL && !o->uniao && Tem(o, titleId))
                    return true;
            }
            return false;
        }

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
