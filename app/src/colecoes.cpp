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

    // Linhas de um tipo que ESTA versão não conhece, guardadas inteiras e devolvidas
    // ao arquivo como vieram.
    //
    // Sem isto, uma versão futura que invente um quarto tipo perderia a coleção ao ser
    // lida por esta: a linha cairia na migração do formato antigo, viraria uma coleção
    // chamada com o número do id e com TitleIds de lixo, e o Gravar -- que reescreve o
    // arquivo INTEIRO a partir do que foi lido -- gravaria o estrago por cima do
    // original. Guardar o texto cru custa quase nada e corta essa classe de perda de
    // uma vez.
    //
    // É só meia solução, e a metade que falta não tem conserto: as versões JÁ
    // LANÇADAS não fazem isto. Quem criar um cruzamento e depois voltar para a v1.3.0
    // perde o cruzamento do mesmo jeito.
    std::vector<std::string> g_desconhecidas;

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

    // O id de uma linha preservada, para o sorteio não colidir com ele. A linha é
    // opaca para nós, mas o id dela não é: ele abre a linha, em decimal.
    bool IdPreservado(int id)
    {
        for (size_t i = 0; i < g_desconhecidas.size(); i++)
            if (atoi(g_desconhecidas[i].c_str()) == id)
                return true;
        return false;
    }
}

namespace colecoes
{
    // Definida depois do Carregar, que a usa: é ela que sorteia id para a linha do
    // formato antigo e para a que vier com id repetido.
    int ProximoId();

    bool Composta(const Colecao *c)
    {
        return c != NULL && c->tipo != TIPO_JOGOS;
    }

    bool PodeSerOrigem(const Colecao *candidata, Tipo tipoDoAlvo)
    {
        if (candidata == NULL) return false;

        // Junção só aceita coleção de jogos; cruzamento aceita jogos ou junção.
        // Coleção de jogos não tem origem nenhuma.
        //
        // Este teste é também o GUARDA DE CICLO do Tem(): como nenhum tipo aceita o
        // próprio tipo, a recursão desce no máximo dois degraus e para. Um
        // colecoes.txt editado à mão com um cruzamento apontando para si mesmo bate
        // aqui em vez de recorrer sem fim.
        switch (tipoDoAlvo)
        {
        case TIPO_UNIAO:      return candidata->tipo == TIPO_JOGOS;
        case TIPO_INTERSECAO: return candidata->tipo == TIPO_JOGOS ||
                                     candidata->tipo == TIPO_UNIAO;
        case TIPO_JOGOS:      return false;
        }
        return false;
    }

    const char *Palavra(Tipo t)
    {
        switch (t)
        {
        case TIPO_UNIAO:      return "uniao";
        case TIPO_INTERSECAO: return "intersecao";
        case TIPO_JOGOS:      return "jogos";
        }
        return "jogos";
    }

    bool PorPalavra(const std::string &s, Tipo *saida)
    {
        if      (s == "jogos")      { *saida = TIPO_JOGOS;      return true; }
        else if (s == "uniao")      { *saida = TIPO_UNIAO;      return true; }
        else if (s == "intersecao") { *saida = TIPO_INTERSECAO; return true; }
        return false;
    }

    Colecao *PorId(int id)
    {
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id == id)
                return g_lista[i];
        return NULL;
    }

    // Tira das compostas os ids que nao existem mais ou que a escada nao permite, e
    // APAGA a composta que ficou sem nenhuma origem: composta sem origem nao e colecao
    // vazia, e colecao que perdeu o sentido.
    //
    // Repete ate estabilizar, e com o cruzamento isso DEIXOU de ser teorico: apagar
    // uma colecao de jogos pode esvaziar a juncao, e a juncao apagada pode esvaziar o
    // cruzamento que apontava para ela. Sao duas voltas, nao uma.
    void LimparCompostas()
    {
        bool mexeu = true;
        while (mexeu)
        {
            mexeu = false;
            for (size_t i = 0; i < g_lista.size(); i++)
            {
                Colecao *c = g_lista[i];
                if (!Composta(c)) continue;

                // A PODA SO VALE PARA A JUNCAO.
                //
                // Tirar uma origem de uma juncao so ENCOLHE o conjunto, e o nome
                // continua honesto. Numa INTERSECCAO faz o contrario: tirar um fator
                // ALARGA o resultado. "Co-op nao zerado" que perdesse o "nao zerado"
                // viraria a lista inteira de co-op, zerados inclusive, com o nome
                // mentindo na tela e nada avisando -- e isso se alcanca so pela
                // interface, apagando a colecao que era a unica origem da juncao.
                //
                // Entao a interseccao NAO e podada: a origem morta fica onde esta, o
                // Tem() devolve vazio enquanto ela estiver la, e o card mostra ( 0 )
                // ate alguem arrumar pelo "Escolher colecoes". Vazio e visivel e tem
                // conserto; o conjunto errado passa despercebido.
                if (c->tipo == TIPO_UNIAO)
                {
                    for (size_t k = 0; k < c->origens.size(); )
                    {
                        if (!PodeSerOrigem(PorId(c->origens[k]), c->tipo))
                        {
                            c->origens.erase(c->origens.begin() + k);
                            mexeu = true;
                        }
                        else k++;
                    }
                }

                if (c->origens.empty())
                {
                    diario::Escrever("%s '%s' ficou sem origem e foi apagada",
                                     Palavra(c->tipo), c->nome.c_str());
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
            // fgets devolve não-NULL mesmo quando o que leu começa com byte NULO, e aí
            // o += não acrescenta caractere nenhum. Sem este guarda, saida.size() - 1
            // é (size_t)-1 e o operator[] do Release indexa 4 GB adiante: fatal crash.
            //
            // Não é hipótese: o Gravar abre com "w", que trunca na hora. Console
            // desligado no meio da regravação deixa o cluster já alocado cheio de
            // zeros, e o app morria em TODO arranque até alguém apagar o arquivo.
            if (saida.empty()) continue;
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
        g_desconhecidas.clear();

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

            // Formato NOVO:   id|tipo|nome|conteudo
            // Formato ANTIGO: nome|TitleIds
            //
            // Distinguem-se pelo comeco: o novo abre com numero seguido de barra. Linha
            // antiga ganha id na leitura e e regravada no formato novo no proximo
            // salvamento -- a migracao nao pede nada do usuario.
            //
            // So abrir com numero NAO basta para dizer que a linha e do formato novo:
            // "1942|FFED0707,..." e do antigo, e uma colecao chamada 1942 nao e
            // esquisitice nenhuma num app de jogos. O segundo campo e que decide -- no
            // formato antigo ali vao TitleIds em hexa, que tem digito, e o nome nunca
            // chega la porque Sanear tira a barra dele.
            std::string ids;
            size_t b2 = linha.find('|', barra + 1);
            std::string tipoTxt = (b2 == std::string::npos)
                                ? std::string()
                                : linha.substr(barra + 1, b2 - barra - 1);
            // O que decide e a SEGUNDA BARRA, nao o formato da palavra.
            //
            // Exigir que o tipo fosse so letras parecia mais seguro e era pior: um
            // tipo futuro com digito ou sublinhado -- "diferenca_simetrica" --
            // reprovava aqui, caia no ramo do formato antigo e era DESTRUIDO, que e
            // exatamente a perda que o g_desconhecidas existe para fechar. O formato
            // antigo nunca tem segunda barra: o Sanear tira a barra do nome, e o que
            // vem depois e hexa com virgulas.
            bool novo = (barra > 0
                         && linha.find_first_not_of("0123456789") == barra
                         && b2 != std::string::npos);

            Tipo tipo = TIPO_JOGOS;
            if (novo && !PorPalavra(tipoTxt, &tipo))
            {
                // Linha bem formada com um tipo que esta versao nao conhece. Guarda
                // inteira e devolve igual no Gravar -- ver g_desconhecidas.
                diario::Escrever("linha de tipo '%s' desconhecido preservada como esta",
                                 tipoTxt.c_str());
                g_desconhecidas.push_back(linha);
                continue;
            }

            // Linha de COMPOSTA cortada no meio (queda de energia durante o Gravar).
            // Sem o quarto campo ela fica sem origem, e o LimparCompostas a apaga
            // logo abaixo -- justo o que o guarda do "NUNCA descartar" promete nao
            // fazer. Entao ela e preservada inteira, como as de tipo desconhecido.
            // Para colecao de jogos continua valendo o combinado: o nome sobrevive,
            // o conteudo se perde, e o usuario ve a colecao vazia e refaz.
            if (novo && tipo != TIPO_JOGOS &&
                linha.find('|', b2 + 1) == std::string::npos)
            {
                diario::Escrever("linha de '%s' cortada no meio preservada como esta",
                                 tipoTxt.c_str());
                g_desconhecidas.push_back(linha);
                continue;
            }

            Colecao *c = new Colecao();
            c->id = 0;
            c->tipo = tipo;

            if (novo)
            {
                c->id = atoi(linha.substr(0, barra).c_str());

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
                if (Composta(c))
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

        // Quem veio do formato antigo ganha id agora.
        for (size_t i = 0; i < g_lista.size(); i++)
            if (g_lista[i]->id == 0) g_lista[i]->id = ProximoId();

        // Id repetido no arquivo: até agora o PorId devolvia o primeiro e seguia em
        // SILÊNCIO, e uma composta que apontasse para aquele id ficava presa na coleção
        // errada para sempre. Com dois editores isso deixou de ser teórico, então
        // vale detectar, renumerar a segunda e registrar.
        //
        // Renumerar a SEGUNDA, não a primeira, porque é a primeira que o PorId já
        // devolvia -- as compostas que existem hoje continuam apontando para onde
        // apontavam.
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            for (size_t k = 0; k < i; k++)
            {
                if (g_lista[k]->id != g_lista[i]->id)
                    continue;

                const int repetido = g_lista[i]->id;
                const int novoId = ProximoId();
                diario::Escrever("colecao '%s' tinha o id %d repetido com '%s'; "
                                 "virou %d NESTA SESSAO (o disco so muda na proxima "
                                 "gravacao)",
                                 g_lista[i]->nome.c_str(), repetido,
                                 g_lista[k]->nome.c_str(), novoId);

                // Qual das duas a composta queria e INDECIDIVEL: o arquivo guarda o
                // numero, nao a intencao. Fica com a primeira, que e quem o PorId ja
                // devolvia -- mas a ambiguidade tem de deixar rastro, senao a
                // renumeracao apaga a evidencia e o vinculo errado vira definitivo
                // sem ninguem nunca saber que houve escolha.
                for (size_t u = 0; u < g_lista.size(); u++)
                {
                    if (!Composta(g_lista[u])) continue;
                    for (size_t o = 0; o < g_lista[u]->origens.size(); o++)
                        if (g_lista[u]->origens[o] == repetido)
                            diario::Escrever("  AVISO: a composta '%s' aponta para o id "
                                             "%d e ficou com '%s'; confira",
                                             g_lista[u]->nome.c_str(), repetido,
                                             g_lista[k]->nome.c_str());
                }

                g_lista[i]->id = novoId;
                break;
            }
        }

        // Id que colide com uma LINHA PRESERVADA tambem e repetido -- so que a outra
        // metade do par nao esta no g_lista, e sim no g_desconhecidas, e por isso o
        // laco acima nunca a ve. Sem isto o arquivo sai com dois registros de mesmo
        // id, em silencio.
        //
        // Aqui a composta ambigua PODE ser consertada, ao contrario do caso de cima:
        // o PorId so enxerga coleção viva, entao toda origem que aponta para este id
        // queria a viva. Renumera-se a viva e levam-se as origens junto; a linha
        // preservada fica intocada, que e o que "preservada" quer dizer.
        for (size_t i = 0; i < g_lista.size(); i++)
        {
            if (!IdPreservado(g_lista[i]->id)) continue;

            const int repetido = g_lista[i]->id;
            const int novoId = ProximoId();
            diario::Escrever("colecao '%s' tinha o id %d repetido com uma linha "
                             "preservada; virou %d, e as origens foram junto",
                             g_lista[i]->nome.c_str(), repetido, novoId);

            for (size_t u = 0; u < g_lista.size(); u++)
            {
                if (!Composta(g_lista[u])) continue;
                for (size_t o = 0; o < g_lista[u]->origens.size(); o++)
                    if (g_lista[u]->origens[o] == repetido)
                        g_lista[u]->origens[o] = novoId;
            }
            g_lista[i]->id = novoId;
        }

        LimparCompostas();

        diario::Escrever("colecoes carregadas: %d (%d linhas preservadas)",
                         (int)g_lista.size(), (int)g_desconhecidas.size());
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
        fprintf(f, "#   tipo jogos: TitleIds em hexa\n");
        fprintf(f, "#   tipo uniao e intersecao: ids de colecao, em decimal\n");

        for (size_t i = 0; i < g_lista.size(); i++)
        {
            const Colecao *c = g_lista[i];
            fprintf(f, "%d|%s|%s|", c->id, Palavra(c->tipo), c->nome.c_str());

            if (Composta(c))
                for (size_t k = 0; k < c->origens.size(); k++)
                    fprintf(f, "%s%d", k ? "," : "", c->origens[k]);
            else
                for (size_t k = 0; k < c->ids.size(); k++)
                    fprintf(f, "%s%08X", k ? "," : "", c->ids[k]);

            fputc('\n', f);
        }

        // E as de tipo desconhecido, iguais ao que entrou.
        for (size_t i = 0; i < g_desconhecidas.size(); i++)
            fprintf(f, "%s\n", g_desconhecidas[i].c_str());

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

    // Id de coleção é SORTEADO, não sequencial.
    //
    // O arquivo tem dois editores -- este app e o xbox-vault, no PC -- e os dois
    // criavam id como "maior + 1" sobre as coleções vivas. Apagar a de maior id
    // devolvia aquele número ao estoque, e criar uma de cada lado antes de
    // sincronizar dava o MESMO id para coleções diferentes. A partir daí uma composta
    // aponta para a errada, em silêncio.
    //
    // Sortear em 31 bits resolve sem os dois lados combinarem nada, que é a única
    // forma que funciona quando não há coordenação entre eles. Os ids pequenos que já
    // existem continuam valendo: só os novos mudam.
    unsigned int g_semente = 0;

    int ProximoId()
    {
        if (g_semente == 0)
        {
            // A entropia vem do GetTickCount, e só dele. O endereço entra porque é
            // de graça, mas NÃO conta como aleatoriedade: g_semente é global num .xex
            // sem ASLR, então o endereço é o mesmo byte a byte em todo console que
            // rode este build. Quem sincroniza dois consoles ligados juntos está
            // apostando no relógio -- e em 31 bits, com dezenas de coleções, a aposta
            // paga.
            g_semente = (unsigned int)GetTickCount() ^
                        (unsigned int)(ULONG_PTR)&g_semente;
            if (g_semente == 0) g_semente = 1;
        }

        for (int tentativa = 0; tentativa < 64; tentativa++)
        {
            // xorshift32: cabe em três linhas e não depende do rand() da CRT, que
            // devolve só 15 bits -- metade do que precisamos.
            g_semente ^= g_semente << 13;
            g_semente ^= g_semente >> 17;
            g_semente ^= g_semente << 5;

            int id = (int)(g_semente & 0x7FFFFFFF);
            // As linhas preservadas contam como ocupadas: o id delas nao esta no
            // g_lista, mas esta no ARQUIVO, e reusa-lo criaria duas linhas com o mesmo
            // id -- exatamente o que a renumeracao do Carregar existe para evitar.
            if (id > 0 && PorId(id) == NULL && !IdPreservado(id))
                return id;
        }

        // 64 sorteios sem achar um livre não acontece com dezenas de coleções. Se
        // acontecer, o MENOR id livre é melhor que "maior + 1": o sorteio pode ter
        // devolvido 0x7FFFFFFF a alguém, e aí maior + 1 estoura o int com sinal e
        // volta negativo. Id negativo no arquivo não passa no teste de formato novo
        // (o '-' não é dígito), a linha cai na migração do formato antigo e a coleção
        // perde todos os jogos. Procurando de baixo isso não tem como acontecer, e o
        // laço termina sempre: há no máximo g_lista.size() + g_desconhecidas.size()
        // ids ocupados.
        for (int id = 1; id <= (int)(g_lista.size() + g_desconhecidas.size()) + 1; id++)
            if (PorId(id) == NULL && !IdPreservado(id)) return id;
        return 1;       // inalcançável: o laço acima só sai achando
    }

    // O ponteiro devolvido é estável: g_lista guarda ponteiros, então nem push_back
    // nem erase mexem no endereço das outras coleções.
    Colecao *Criar(const std::string &nome)
    {
        Colecao *c = new Colecao();
        c->id = ProximoId();
        c->tipo = TIPO_JOGOS;
        c->nome = Sanear(nome);
        g_lista.push_back(c);
        Gravar();
        return c;
    }

    Colecao *CriarComposta(const std::string &nome, Tipo tipo)
    {
        Colecao *c = new Colecao();
        c->id = ProximoId();
        c->tipo = tipo;
        c->nome = Sanear(nome);
        g_lista.push_back(c);
        // NAO grava aqui: composta sem origem seria apagada na proxima leitura. Quem
        // chama grava depois de escolher as origens.
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
                // Antes de gravar: senao o id apagado fica pendurado em alguma composta
                // no disco. O motivo original era pior -- o ProximoId sequencial podia
                // devolver esse mesmo id a uma colecao nova, que entraria na composta
                // sem ninguem ter pedido. Com id sorteado em 31 bits o reuso deixou de
                // preocupar, mas composta com origem morta continua sendo lixo no
                // arquivo.
                LimparCompostas();
                Gravar();
                return;
            }
        }
    }

    bool Tem(const Colecao *c, unsigned int titleId)
    {
        if (c == NULL) return false;

        // Responde por PERGUNTA em vez de devolver a lista toda porque os chamadores
        // varrem a biblioteca e so querem saber "este jogo entra?". Montar o vetor
        // resolvido a cada um deles seria alocacao dentro do laco de desenho.
        //
        // A recursao desce no maximo dois degraus -- cruzamento, juncao, jogos -- e o
        // PodeSerOrigem e o que garante isso: nenhum tipo aceita o proprio tipo.
        if (c->tipo == TIPO_UNIAO)
        {
            for (size_t i = 0; i < c->origens.size(); i++)
            {
                const Colecao *o = PorId(c->origens[i]);
                if (PodeSerOrigem(o, TIPO_UNIAO) && Tem(o, titleId))
                    return true;
            }
            return false;
        }

        if (c->tipo == TIPO_INTERSECAO)
        {
            // Origem que nao resolve derruba TUDO, em vez de ser ignorada. Ignorar
            // seria responder a uma pergunta com um fator a menos -- "co-op E nao
            // zerado" virando "co-op" -- e a resposta sairia MAIOR que a certa, com
            // cara de certa. Perdeu um fator, o resultado e desconhecido, e
            // desconhecido aqui se mostra como vazio.
            for (size_t i = 0; i < c->origens.size(); i++)
            {
                const Colecao *o = PorId(c->origens[i]);
                if (!PodeSerOrigem(o, TIPO_INTERSECAO))
                    return false;
                if (!Tem(o, titleId))
                    return false;
            }
            // "PARA TODA origem" sobre lista VAZIA e verdadeiro -- em logica e aqui
            // tambem, se deixarmos. Sem isto, um cruzamento sem origem nenhuma
            // responderia "sim" para todo jogo e conteria a biblioteca inteira.
            return !c->origens.empty();
        }

        for (size_t i = 0; i < c->ids.size(); i++)
            if (c->ids[i] == titleId)
                return true;
        return false;
    }

    void Alternar(Colecao *c, unsigned int titleId)
    {
        // Numa composta o ids fica vazio para sempre, e empurrar aqui dentro gravaria
        // um TitleId que ninguem le -- some da tela e reaparece se a colecao um dia
        // virar de jogos. Ver colecoes.h.
        if (Composta(c)) return;

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
        if (Composta(c)) return;

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
