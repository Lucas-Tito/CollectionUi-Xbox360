#include "emuladores.h"
#include "diario.h"
#include "lancador.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>

namespace
{
    // As pastas em que emulador de 360 costuma guardar ROM. Minusculas e maiusculas
    // entram as duas porque o sistema de arquivos do console nao distingue, mas o
    // FindFirstFile e alimentado com o nome exato -- e o Snes360 usa "Roms" enquanto
    // o pcsxr e o FBANext usam "roms".
    const char *PASTAS[] = { "Roms", "roms", "ROMS", "games", "Games" };
    const int   QUANTAS_PASTAS = sizeof(PASTAS) / sizeof(PASTAS[0]);

    // Lista generosa de proposito: nao ha tabela por emulador aqui, e nao deve haver.
    // Amarrar extensao a titleId quebraria na proxima versao do emulador; amarrar a
    // pasta "roms" vale para qualquer um que apareca depois.
    const char *EXTENSOES[] = {
        ".smc", ".sfc", ".fig",                      // SNES
        ".bin", ".img", ".iso", ".cue", ".pbp",      // PS1
        ".zip",                                      // arcade
        ".nes", ".gba", ".gbc", ".gb",               // Nintendo portateis e 8 bits
        ".md", ".smd", ".gen", ".32x",               // Mega Drive
        ".pce", ".n64", ".z64", ".v64"
    };
    const int QUANTAS_EXTENSOES = sizeof(EXTENSOES) / sizeof(EXTENSOES[0]);

    bool TerminaEm(const char *nome, const char *sufixo)
    {
        size_t n = strlen(nome), s = strlen(sufixo);
        return n > s && _stricmp(nome + n - s, sufixo) == 0;
    }

    bool EhRom(const char *nome)
    {
        for (int i = 0; i < QUANTAS_EXTENSOES; i++)
            if (TerminaEm(nome, EXTENSOES[i]))
                return true;
        return false;
    }

    std::string SemExtensao(const std::string &nome)
    {
        size_t ponto = nome.rfind('.');
        return (ponto == std::string::npos) ? nome : nome.substr(0, ponto);
    }

    // A pasta que contem o executavel do emulador, dentro do caminho que o content.db
    // guarda -- que e relativo ao dispositivo e comeca com barra.
    std::string PastaDo(const std::string &caminhoDoXex)
    {
        size_t barra = caminhoDoXex.rfind('\\');
        return (barra == std::string::npos) ? std::string()
                                            : caminhoDoXex.substr(0, barra);
    }

    bool Existe(const std::string &caminho)
    {
        return GetFileAttributes(caminho.c_str()) != 0xFFFFFFFF;
    }
}

namespace emuladores
{
    unsigned int IdDaRom(unsigned int titleIdEmulador, const std::string &arquivo)
    {
        // FNV-1a de 32 bits, semeado com o titleId do emulador. Escolhido por caber em
        // oito linhas e nao precisar de tabela: o que importa aqui e ser ESTAVEL entre
        // execucoes, porque este numero vai para o colecoes.txt. Enquanto o nome do
        // arquivo nao mudar, o id nao muda.
        //
        // Colisao com um titleId de verdade e possivel e tolerada: o efeito seria dois
        // itens sempre marcados juntos, que e exatamente o caso de jogo multi-disco --
        // ja previsto e ja suportado pela tela.
        unsigned int h = 2166136261u ^ titleIdEmulador;
        for (size_t i = 0; i < arquivo.size(); i++)
        {
            // Minusculas: o console nao distingue caixa em nome de arquivo, e um
            // mesmo arquivo nao pode render dois ids conforme quem o leu.
            char c = arquivo[i];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);

            h ^= (unsigned char)c;
            h *= 16777619u;
        }
        return h;
    }

    void Ler(const std::vector<biblioteca::Jogo> &jogos,
             const std::string &pastaDoApp,
             std::vector<biblioteca::Jogo> &saida)
    {
        saida.clear();

        for (size_t e = 0; e < jogos.size(); e++)
        {
            const biblioteca::Jogo &emu = jogos[e];

            // So XEX solto: um emulador empacotado em container nao tem pasta de ROM
            // ao lado para varrer.
            if (emu.tipoArquivo != 1)
                continue;

            // Pelo dispositivo de verdade, nao supondo Hdd: -- o emulador pode estar
            // num pendrive, e e o lancador que ja sabe sondar os apelidos.
            std::string xex = lancador::Resolver(emu.caminho);
            if (xex.empty())
                continue;

            std::string pasta = PastaDo(xex);
            if (pasta.empty())
                continue;

            for (int k = 0; k < QUANTAS_PASTAS; k++)
            {
                std::string raiz = pasta + "\\" + PASTAS[k];
                if (!Existe(raiz))
                    continue;

                WIN32_FIND_DATA achado;
                HANDLE busca = FindFirstFile((raiz + "\\*").c_str(), &achado);
                if (busca == INVALID_HANDLE_VALUE)
                    continue;

                int quantas = 0;
                do
                {
                    if (achado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        continue;
                    if (!EhRom(achado.cFileName))
                        continue;

                    biblioteca::Jogo r;
                    // Negativo de proposito: o id e a chave do cache de texturas, e
                    // ContentItemId e sempre positivo. Assim uma ROM nunca disputa a
                    // entrada de cache de um jogo da FreeStyle.
                    r.id             = -(int)(saida.size() + 1);
                    r.titleId        = IdDaRom(emu.titleId, achado.cFileName);
                    r.nome           = SemExtensao(achado.cFileName);
                    r.genero         = emu.nome;        // "Snes360" vira o rotulo
                    r.desenvolvedora = "";
                    r.publicadora    = "";
                    r.nota           = "";
                    r.lancamento     = "";
                    // Caminho e tipo do EMULADOR: abrir a ROM abre o emulador, e isso
                    // reaproveita o lancador inteiro sem uma linha de excecao.
                    r.caminho        = emu.caminho;
                    r.tipoArquivo    = emu.tipoArquivo;
                    r.contentType    = emu.contentType;
                    r.discos         = 1;

                    // A arte e um arquivo solto com o nome da ROM. Nada de casamento
                    // aproximado aqui: isso acontece no PC, uma vez. Ver a decisao 115.
                    if (!pastaDoApp.empty())
                    {
                        std::string base = pastaDoApp + "\\capas\\" + r.nome;
                        if (Existe(base + ".jpg"))      r.capa = base + ".jpg";
                        else if (Existe(base + ".png")) r.capa = base + ".png";
                    }

                    saida.push_back(r);
                    quantas++;
                }
                while (FindNextFile(busca, &achado));

                FindClose(busca);
                diario::Escrever("emulador '%s': %d ROMs em %s",
                                 emu.nome.c_str(), quantas, raiz.c_str());
            }
        }

        diario::Escrever("ROMs de emulador encontradas: %d", (int)saida.size());
    }
}
