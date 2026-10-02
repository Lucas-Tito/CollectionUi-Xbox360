#include "config.h"
#include "diario.h"
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
            ligado = (linha[n + 1] != '0');
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
            ligado = (linha[n + 1] != '0');
            break;
        }

        fclose(f);
        return ligado;
    }

    void GravarBanco(const std::string &caminho)
    {
        FILE *f = fopen(ARQUIVO, "w");
        if (f == NULL)
        {
            diario::Escrever("AVISO: nao consegui gravar %s — vai perguntar de novo", ARQUIVO);
            return;
        }

        fprintf(f, "# CollectionUI. Apague este arquivo para escolher a biblioteca de novo.\n");
        fprintf(f, "%s%s\n", CHAVE, caminho.c_str());
        fclose(f);

        diario::Escrever("escolha gravada em %s", ARQUIVO);
    }
}
