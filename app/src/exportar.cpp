#include "exportar.h"
#include "colecoes.h"
#include "diario.h"
#include <stdio.h>

namespace
{
    const char *ARQUIVO = "game:\\vault.txt";

    // O mesmo saneamento de nome do colecoes.txt, pelo mesmo motivo: a barra e o
    // separador, e nome com barra quebraria a linha em dois campos. Aqui NAO ha corte
    // de tamanho -- o nome do jogo vai inteiro, porque quem le e o vault, nao a tela.
    std::string SemBarra(const std::string &s)
    {
        std::string saida = s;
        for (size_t i = 0; i < saida.size(); i++)
            if (saida[i] == '|' || saida[i] == '\n' || saida[i] == '\r')
                saida[i] = ' ';
        return saida;
    }
}

namespace exportar
{
    bool ParaOVault(const std::vector<biblioteca::Jogo> &jogos, bool bancoOk,
                    std::string &erro)
    {
        // Inventario vazio e inventario NAO LIDO sao a mesma coisa no arquivo, e o
        // vault nao teria como distinguir: ele concluiria que este console nao tem
        // jogo nenhum e trataria todo TitleId do colecoes.txt como referencia orfa.
        // Entao nao se escreve -- com a FreeStyle varrendo, e so tentar depois.
        if (!bancoOk)
        {
            erro = "Biblioteca nao foi lida; nao exportei";
            diario::Escrever("export recusado: a leitura do banco falhou");
            return false;
        }
        if (jogos.empty())
        {
            erro = "Biblioteca vazia; nao exportei";
            diario::Escrever("export recusado: nenhum item na biblioteca");
            return false;
        }

        FILE *f = fopen(ARQUIVO, "w");
        if (f == NULL)
        {
            erro = "Nao consegui gravar vault.txt";
            diario::Escrever("AVISO: nao consegui gravar %s", ARQUIVO);
            return false;
        }

        // O criterio de comentario aqui e o PRIMEIRO CAMPO, nao a ausencia de barra:
        // as linhas que documentam o formato precisam mostrar a barra, e mostram. A
        // regra "linha sem barra e comentario" veio do colecoes.txt, onde vale porque
        // nenhuma linha de cabecalho dele tem barra -- aqui ela estava errada desde
        // sempre, e o cabecalho a violava em duas linhas.
        //
        // Comecar com '#' tambem serve, e nao colide com colecao chamada "#1
        // favoritos": o nome nunca abre a linha, vem depois de "COLECAO|id|tipo|".
        fprintf(f, "# CollectionUI: inventario deste console, para o xbox-vault\n");
        fprintf(f, "#   linha de dado comeca com JOGO, ROM ou COLECAO; o resto e\n");
        fprintf(f, "#   comentario, inclusive as linhas abaixo, que tem barra\n");

        // NAO ha data de geracao, e a ausencia e deliberada. O relogio do 360 sem rede
        // volta para 2005, e GetLocalTime nao tem como avisar que esta perdido: a
        // linha sairia errada com a mesma cara de certa, e o vault ordenaria os
        // inventarios por ela. Quem sabe a hora de verdade e quem RECEBE o arquivo.
        fprintf(f, "#   tipo|id|contentType|emulador|item|nome|arquivo\n");
        fprintf(f, "#   os quatro primeiros em hexa de 8 digitos, ou vazios\n");
        fprintf(f, "#   JOGO: id e o TitleId, que e o que vai no colecoes.txt;\n");
        fprintf(f, "#         emulador vazio; item e o ContentItemId, e ele SIM e\n");
        fprintf(f, "#         unico -- TitleId nao e: disco 2 e instalacao repetida\n");
        fprintf(f, "#         compartilham o do disco 1, e ai as duas linhas se\n");
        fprintf(f, "#         distinguem so pelo item; arquivo e o caminho de\n");
        fprintf(f, "#         instalacao, relativo ao dispositivo\n");
        fprintf(f, "#   ROM:  id e o sintetico que vai no colecoes.txt; emulador e o\n");
        fprintf(f, "#         TitleId de quem a abre; item vazio; arquivo e o nome do\n");
        fprintf(f, "#         arquivo da ROM dentro da pasta de ROMs do emulador.\n");
        fprintf(f, "#         id = FNV-1a 32 de (emulador, arquivo em minusculas),\n");
        fprintf(f, "#         base 2166136261 xor emulador, primo 16777619\n");
        fprintf(f, "#   COLECAO: outra forma de linha, com 5 campos --\n");
        fprintf(f, "#         COLECAO|id|tipo|nome|conteudo. E a linha do colecoes.txt\n");
        fprintf(f, "#         com o prefixo na frente, para o vault reusar o parser.\n");
        fprintf(f, "#         tipo 'jogos': conteudo e TitleId em hexa, e casa com o\n");
        fprintf(f, "#         id das linhas JOGO e ROM acima. tipo 'uniao': conteudo e\n");
        fprintf(f, "#         id de COLECAO, em decimal. Uniao nunca contem uniao.\n");

        int nJogos = 0, nRoms = 0;
        for (size_t i = 0; i < jogos.size(); i++)
        {
            const biblioteca::Jogo &j = jogos[i];

            // Id NEGATIVO marca ROM: ContentItemId do banco e sempre positivo, e a
            // varredura de emulador numera as ROMs a partir de -1. Ver a decisao 117.
            const bool rom = (j.id < 0);

            if (rom)
            {
                // Nao sai o j.id: la ele e so chave do cache de texturas, muda a cada
                // varredura e nao significa nada fora daqui. O que identifica a ROM e
                // o par (emulador, arquivo), que e exatamente o que semeia o id.
                fprintf(f, "ROM|%08X|%08X|%08X||%s|%s\n",
                        j.titleId, (unsigned)j.contentType, j.titleIdEmulador,
                        SemBarra(j.nome).c_str(), SemBarra(j.arquivoRom).c_str());
                nRoms++;
            }
            else
            {
                fprintf(f, "JOGO|%08X|%08X||%08X|%s|%s\n",
                        j.titleId, (unsigned)j.contentType, (unsigned)j.id,
                        SemBarra(j.nome).c_str(), SemBarra(j.caminho).c_str());
                nJogos++;
            }
        }

        // As colecoes saem DEPOIS dos itens, para que o vault ja tenha visto todo
        // TitleId quando for resolver o conteudo de cada uma. Ordem alfabetica, a
        // mesma da tela; cada linha carrega o proprio id, entao a ordem e so conforto
        // de quem le.
        std::vector<colecoes::Colecao *> cols = colecoes::Ordenadas();
        for (size_t c = 0; c < cols.size(); c++)
        {
            const colecoes::Colecao *col = cols[c];
            fprintf(f, "COLECAO|%d|%s|%s|", col->id, col->uniao ? "uniao" : "jogos",
                    SemBarra(col->nome).c_str());

            if (col->uniao)
                for (size_t k = 0; k < col->origens.size(); k++)
                    fprintf(f, "%s%d", k ? "," : "", col->origens[k]);
            else
                for (size_t k = 0; k < col->ids.size(); k++)
                    fprintf(f, "%s%08X", k ? "," : "", col->ids[k]);

            fputc('\n', f);
        }

        // Rodape com a contagem: sem ele, arquivo cortado por disco cheio e arquivo
        // inteiro sao indistinguiveis do lado do vault. Sem barra, como o cabecalho.
        fprintf(f, "# total: %d jogos, %d ROMs, %d colecoes\n",
                nJogos, nRoms, (int)cols.size());

        // O erro de escrita aparece no flush, que acontece no fclose -- checar so os
        // fprintf nao pegaria disco cheio. Sem isto, um arquivo truncado saia daqui
        // com balao de sucesso.
        bool ok = (ferror(f) == 0);
        if (fclose(f) != 0)
            ok = false;
        if (!ok)
        {
            erro = "Escrita de vault.txt falhou; disco cheio?";
            diario::Escrever("ERRO: escrita de %s falhou", ARQUIVO);
            return false;
        }

        diario::Escrever("vault.txt: %d jogos, %d ROMs e %d colecoes",
                         nJogos, nRoms, (int)cols.size());
        return true;
    }
}
