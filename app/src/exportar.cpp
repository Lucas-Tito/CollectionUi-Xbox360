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
    bool Biblioteca(const std::vector<biblioteca::Jogo> &jogos, std::string &erro)
    {
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
        fprintf(f, "#   tipo, id, contentType, emulador, nome, caminho\n");
        fprintf(f, "#   separados por barra vertical; ids em hexa de 8 digitos\n");
        fprintf(f, "#   tipo JOGO: id e o TitleId, emulador vazio\n");
        fprintf(f, "#   tipo ROM: id e o sintetico que este console usa nas colecoes,\n");
        fprintf(f, "#             e emulador e o TitleId de quem a abre\n");

        int nJogos = 0, nRoms = 0;
        for (size_t i = 0; i < jogos.size(); i++)
        {
            const biblioteca::Jogo &j = jogos[i];

            // Id NEGATIVO marca ROM: ContentItemId do banco e sempre positivo, e a
            // varredura de emulador numera as ROMs a partir de -1. Ver a decisao 117.
            const bool rom = (j.id < 0);

            if (rom)
            {
                // Para a ROM, "genero" guarda o nome do emulador (emuladores.cpp), mas
                // o vault precisa do TITLEID dele, que e o que semeia o id sintetico.
                // Ele esta no proprio item: o caminho da ROM e o do emulador.
                fprintf(f, "ROM|%08X|%d|%08X|%s|%s\n",
                        j.titleId, j.contentType, j.titleIdEmulador,
                        SemBarra(j.nome).c_str(), SemBarra(j.caminho).c_str());
                nRoms++;
            }
            else
            {
                fprintf(f, "JOGO|%08X|%d||%s|%s\n",
                        j.titleId, j.contentType,
                        SemBarra(j.nome).c_str(), SemBarra(j.caminho).c_str());
                nJogos++;
            }
        }

        fclose(f);
        diario::Escrever("biblioteca.txt: %d jogos e %d ROMs", nJogos, nRoms);
        return true;
    }
}
