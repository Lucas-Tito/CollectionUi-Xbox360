#include "config.h"
#include "diario.h"
#include <vector>
#include <stdio.h>
#include <string.h>

namespace
{
    const char *ARQUIVO = "game:\\collectionui.ini";
    const char *CHAVE   = "banco=";
}

namespace config
{
    std::string LerBanco()
    {
        FILE *f = fopen(ARQUIVO, "r");
        if (f == NULL)
            return std::string();

        char linha[512];
        std::string achado;

        while (fgets(linha, sizeof(linha), f) != NULL)
        {
            if (strncmp(linha, CHAVE, strlen(CHAVE)) != 0)
                continue;

            achado = linha + strlen(CHAVE);
            while (!achado.empty() &&
                   (achado[achado.size() - 1] == '\n' || achado[achado.size() - 1] == '\r'))
                achado.erase(achado.size() - 1);
            break;
        }

        fclose(f);
        return achado;
    }

    bool Ligado(const char *chave)
    {
        FILE *f = fopen(ARQUIVO, "r");
        if (f == NULL)
            return true;

        char linha[512];
        bool ligado = true;
        size_t n = strlen(chave);

        while (fgets(linha, sizeof(linha), f) != NULL)
        {
            if (strncmp(linha, chave, n) != 0 || linha[n] != '=')
                continue;

            // Valor VAZIO ("logDetalhe=") nao liga nada: antes o teste era so
            // "diferente de 0", e o '\n' passava como se fosse ligado.
            {
                char v = linha[n + 1];
                ligado = (v != '0' && v != '\n' && v != '\r' && v != '\0');
            }
            break;
        }

        fclose(f);
        return ligado;
    }

    bool LigadoSeDito(const char *chave)
    {
        FILE *f = fopen(ARQUIVO, "r");
        if (f == NULL)
            return false;

        char linha[512];
        bool ligado = false;
        size_t n = strlen(chave);

        while (fgets(linha, sizeof(linha), f) != NULL)
        {
            if (strncmp(linha, chave, n) != 0 || linha[n] != '=')
                continue;

            // Valor VAZIO ("logDetalhe=") nao liga nada: antes o teste era so
            // "diferente de 0", e o '\n' passava como se fosse ligado.
            {
                char v = linha[n + 1];
                ligado = (v != '0' && v != '\n' && v != '\r' && v != '\0');
            }
            break;
        }

        fclose(f);
        return ligado;
    }

    void GravarBanco(const std::string &caminho)
    {
        // Le as outras chaves ANTES de truncar, para devolve-las depois.
        std::vector<std::string> guardadas;
        FILE *leitura = fopen(ARQUIVO, "r");
        if (leitura != NULL)
        {
            char linha[512];
            while (fgets(linha, sizeof(linha), leitura) != NULL)
            {
                if (linha[0] == '#' || linha[0] == '\n' || linha[0] == '\r') continue;
                if (strncmp(linha, CHAVE, strlen(CHAVE)) == 0) continue;

                std::string l = linha;
                while (!l.empty() && (l[l.size() - 1] == '\n' || l[l.size() - 1] == '\r'))
                    l.erase(l.size() - 1);
                if (!l.empty()) guardadas.push_back(l);
            }
            fclose(leitura);
        }

        FILE *f = fopen(ARQUIVO, "w");
        if (f == NULL)
        {
            diario::Escrever("AVISO: nao consegui gravar %s — vai perguntar de novo", ARQUIVO);
            return;
        }

        fprintf(f, "# CollectionUI. Apague este arquivo para escolher a biblioteca de novo.\n");
        fprintf(f, "%s%s\n", CHAVE, caminho.c_str());

        // As OUTRAS chaves voltam. Antes este arquivo era reescrito do zero, entao
        // reescolher a biblioteca apagava em silencio o som=, o anel= e o logDetalhe=
        // que o usuario tinha posto a mao -- e o cabecalho do config.h promete que dao
        // para editar por FTP.
        for (size_t i = 0; i < guardadas.size(); i++)
            fprintf(f, "%s\n", guardadas[i].c_str());

        fclose(f);

        diario::Escrever("escolha gravada em %s", ARQUIVO);
    }
}
