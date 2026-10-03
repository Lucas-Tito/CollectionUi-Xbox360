#include "emuladores.h"
#include "diario.h"
#include "lancador.h"
#include <xtl.h>
#include <stdio.h>
#include <string.h>

namespace
{
    // UMA grafia por pasta. O sistema de arquivos do console nao distingue caixa E o
    // FindFirstFile tambem nao -- ele desce para o NtCreateFile com OBJ_CASE_INSENSITIVE,
    // igual ao Windows de PC. Por isso "Roms", "roms" e "ROMS" abrem o MESMO diretorio,
    // e listar as tres fazia cada ROM entrar tres vezes na biblioteca.
    const char *PASTAS[] = { "Roms", "games" };
    const int   QUANTAS_PASTAS = sizeof(PASTAS) / sizeof(PASTAS[0]);

    // Generosa de proposito: nao ha tabela por emulador aqui, e nao deve haver. Amarrar
    // extensao a titleId quebraria na proxima versao do emulador; amarrar a pasta vale
    // para qualquer um que apareca depois.
    const char *EXTENSOES[] = {
        ".smc", ".sfc", ".fig",                      // SNES
        ".bin", ".img", ".iso", ".cue", ".pbp",      // PS1
        ".zip",                                      // arcade
        ".nes", ".gba", ".gbc", ".gb",               // Nintendo portateis e 8 bits
        ".smd", ".gen", ".32x",                      // Mega Drive
        ".pce", ".n64", ".z64", ".v64"
    };
    const int QUANTAS_EXTENSOES = sizeof(EXTENSOES) / sizeof(EXTENSOES[0]);

    // Um rip de PS1 e um descritor ao lado dos dados. Os dois estao na lista acima, e
    // sem isto o mesmo jogo entra duas vezes -- com ids sinteticos DIFERENTES, porque o
    // hash e sobre o nome com extensao. O usuario marcaria um, veria o outro desmarcado
    // e acharia que o app perdeu a marcacao.
    const char *DESCRITORES[] = { ".cue", ".pbp", ".iso" };
    const char *DADOS[]       = { ".bin", ".img" };

    bool TerminaEm(const char *nome, const char *sufixo)
    {
        size_t n = strlen(nome), s = strlen(sufixo);
        return n > s && _stricmp(nome + n - s, sufixo) == 0;
    }

    bool Casa(const char *nome, const char *const *lista, int quantas)
    {
        for (int i = 0; i < quantas; i++)
            if (TerminaEm(nome, lista[i]))
                return true;
        return false;
    }

    std::string SemExtensao(const std::string &nome)
    {
        size_t ponto = nome.rfind('.');
        return (ponto == std::string::npos) ? nome : nome.substr(0, ponto);
    }

    // Minuscula em ASCII e em Latin-1. O char do cl.exe e SIGNED, entao comparar com
    // 0xC0 sem o cast da sempre falso -- foi assim que a faixa acentuada ficou de fora
    // na primeira versao. O 0xD7 e o sinal de multiplicacao e nao tem par.
    char Minuscula(char c)
    {
        unsigned char u = (unsigned char)c;
        if (u >= 'A' && u <= 'Z')                 return (char)(u + 32);
        if (u >= 0xC0 && u <= 0xDE && u != 0xD7)  return (char)(u + 32);
        return c;
    }

    std::string Minusculas(const std::string &s)
    {
        std::string saida = s;
        for (size_t i = 0; i < saida.size(); i++)
            saida[i] = Minuscula(saida[i]);
        return saida;
    }

    bool EstaNaLista(const std::vector<std::string> &lista, const std::string &q)
    {
        for (size_t i = 0; i < lista.size(); i++)
            if (lista[i] == q)
                return true;
        return false;
    }

    // A pasta que contem o executavel do emulador.
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
        // oito linhas e nao precisar de tabela: o que importa e ser ESTAVEL entre
        // execucoes, porque este numero vai para o colecoes.txt. Enquanto o nome do
        // arquivo nao mudar, o id nao muda.
        //
        // Colisao com um titleId de verdade e possivel e tolerada: o efeito seria dois
        // itens sempre marcados juntos, que e exatamente o caso de jogo multi-disco --
        // ja previsto e ja suportado pela tela.
        unsigned int h = 2166136261u ^ titleIdEmulador;
        for (size_t i = 0; i < arquivo.size(); i++)
        {
            // O console nao distingue caixa em nome de arquivo, e um mesmo arquivo nao
            // pode render dois ids conforme quem o leu -- senao a ROM sumiria das
            // colecoes em que estava, em silencio, so por ter sido renomeada.
            h ^= (unsigned char)Minuscula(arquivo[i]);
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

            // So XEX solto: um emulador empacotado em container nao tem pasta de ROM ao
            // lado para varrer.
            if (emu.tipoArquivo != 1)
                continue;

            // Pelo dispositivo de verdade, nao supondo Hdd: -- o emulador pode estar num
            // pendrive, e e o lancador que ja sabe sondar os apelidos.
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

                // Primeira passada: so recolhe os nomes. O descarte do .bin ao lado do
                // .cue precisa conhecer a pasta inteira antes de decidir.
                std::vector<std::string> nomes;
                WIN32_FIND_DATA achado;
                HANDLE busca = FindFirstFile((raiz + "\\*").c_str(), &achado);
                if (busca == INVALID_HANDLE_VALUE)
                    continue;

                do
                {
                    if (achado.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        continue;
                    if (Casa(achado.cFileName, EXTENSOES, QUANTAS_EXTENSOES))
                        nomes.push_back(achado.cFileName);
                }
                while (FindNextFile(busca, &achado));
                FindClose(busca);

                std::vector<std::string> comDescritor;
                for (size_t i = 0; i < nomes.size(); i++)
                    if (Casa(nomes[i].c_str(), DESCRITORES, 3))
                        comDescritor.push_back(Minusculas(SemExtensao(nomes[i])));

                int quantas = 0;
                for (size_t i = 0; i < nomes.size(); i++)
                {
                    const std::string &nome = nomes[i];

                    if (Casa(nome.c_str(), DADOS, 2) &&
                        EstaNaLista(comDescritor, Minusculas(SemExtensao(nome))))
                        continue;

                    biblioteca::Jogo r;
                    // Negativo de proposito: o id e a chave do cache de texturas, e
                    // ContentItemId e sempre positivo. Assim uma ROM nunca disputa a
                    // entrada de cache de um jogo da FreeStyle.
                    r.id             = -(int)(saida.size() + 1);
                    r.titleId        = IdDaRom(emu.titleId, nome);
                    r.nome           = SemExtensao(nome);
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

                diario::Escrever("emulador '%s': %d ROMs em %s",
                                 emu.nome.c_str(), quantas, raiz.c_str());
            }
        }

        diario::Escrever("ROMs de emulador encontradas: %d", (int)saida.size());
    }
}
