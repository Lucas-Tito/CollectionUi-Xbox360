#include "exportar.h"
#include "diario.h"
#include <xtl.h>
#include <stdio.h>

namespace
{
    // Ao lado do colecoes.txt, de proposito: quem pega um pega o outro no mesmo FTP.
    const char *ARQUIVO = "game:\\biblioteca.txt";

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
    bool Biblioteca(const std::vector<biblioteca::Jogo> &jogos, bool bancoOk,
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
            erro = "Nao consegui gravar biblioteca.txt";
            diario::Escrever("AVISO: nao consegui gravar %s", ARQUIVO);
            return false;
        }

        // Sem barra nestas linhas, de proposito: e o que as distingue de um item. A
        // licao e do colecoes.txt -- um leitor que tratasse '#' como comentario
        // apagaria uma colecao chamada "#1 favoritos", entao o criterio e a barra.
        fprintf(f, "# CollectionUI: inventario deste console, para o xbox-vault\n");

        // Data da geracao: sem ela o vault nao distingue inventario de agora de um de
        // tres semanas atras -- e desde que o export passou a RECUSAR quando a
        // biblioteca nao carregou, encontrar um arquivo velho virou caso normal.
        {
            SYSTEMTIME agora;
            GetLocalTime(&agora);
            fprintf(f, "#   gerado: %04d-%02d-%02d %02d:%02d\n",
                    agora.wYear, agora.wMonth, agora.wDay,
                    agora.wHour, agora.wMinute);
        }
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

        // Rodape com a contagem: sem ele, arquivo cortado por disco cheio e arquivo
        // inteiro sao indistinguiveis do lado do vault. Sem barra, como o cabecalho.
        fprintf(f, "# total: %d jogos, %d ROMs\n", nJogos, nRoms);

        // O erro de escrita aparece no flush, que acontece no fclose -- checar so os
        // fprintf nao pegaria disco cheio. Sem isto, um arquivo truncado saia daqui
        // com balao de sucesso.
        bool ok = (ferror(f) == 0);
        if (fclose(f) != 0)
            ok = false;
        if (!ok)
        {
            erro = "Escrita de biblioteca.txt falhou; disco cheio?";
            diario::Escrever("ERRO: escrita de %s falhou", ARQUIVO);
            return false;
        }

        diario::Escrever("biblioteca.txt: %d jogos e %d ROMs", nJogos, nRoms);
        return true;
    }
}
