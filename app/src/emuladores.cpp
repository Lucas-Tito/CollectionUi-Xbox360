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

    // Nomes de exibicao, lidos de capas\\nomes.txt. Existe porque romset de arcade usa
    // o nome curto do MAME -- "mslug3.zip" nao diz nada na grade. O arquivo e
    // "arquivo da ROM|Nome a mostrar" por linha, com # de comentario.
    //
    // Nao renomear a ROM em disco: o FBANext casa o zip com a DAT dele pelo nome, e
    // renomear quebraria o romset.
    std::vector<std::string> g_chaves, g_rotulos;

    void CarregarNomes(const std::string &pastaDoApp)
    {
        g_chaves.clear();
        g_rotulos.clear();
        if (pastaDoApp.empty())
            return;

        std::string caminho = pastaDoApp + "\\capas\\nomes.txt";
        HANDLE h = CreateFile(caminho.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return;

        DWORD tam = GetFileSize(h, NULL);
        if (tam == 0xFFFFFFFF || tam == 0 || tam > 256u * 1024u)
        {
            CloseHandle(h);
            return;
        }

        std::vector<char> buf(tam + 1);
        DWORD lidos = 0;
        bool ok = (ReadFile(h, &buf[0], tam, &lidos, NULL) != FALSE) && lidos == tam;
        CloseHandle(h);
        if (!ok)
            return;
        buf[lidos] = '\0';

        std::string texto(&buf[0], lidos);
        size_t i = 0;
        while (i < texto.size())
        {
            size_t fim = texto.find('\n', i);
            if (fim == std::string::npos) fim = texto.size();

            std::string linha = texto.substr(i, fim - i);
            i = fim + 1;
            if (!linha.empty() && linha[linha.size() - 1] == '\r')
                linha.erase(linha.size() - 1);
            if (linha.empty() || linha[0] == '#')
                continue;

            size_t barra = linha.find('|');
            if (barra == std::string::npos || barra == 0 || barra + 1 >= linha.size())
                continue;

            g_chaves.push_back(Minusculas(linha.substr(0, barra)));
            g_rotulos.push_back(linha.substr(barra + 1));
        }

        diario::Escrever("nomes de exibicao: %d", (int)g_chaves.size());
    }

    // Vazio quando nao ha troca para esta ROM.
    std::string RotuloDe(const std::string &arquivo)
    {
        std::string chave = Minusculas(arquivo);
        for (size_t i = 0; i < g_chaves.size(); i++)
            if (g_chaves[i] == chave)
                return g_rotulos[i];
        return std::string();
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

    // Uma pasta, sem descer. "rotulo" e o caminho dela dentro da pasta de ROMs
    // ("Arcade\\") ou vazio na raiz -- ele entra no id sintetico para que duas ROMs
    // de mesmo nome em sistemas diferentes nao recebam o mesmo id. Na raiz o rotulo e
    // vazio, entao as ROMs que ja estao em colecao nao mudam de id.
    void Varrer(const biblioteca::Jogo &emu, const std::string &raiz,
                const std::string &rotulo, const std::string &pastaDoApp,
                std::vector<biblioteca::Jogo> &saida)
    {
        std::vector<std::string> nomes;
        WIN32_FIND_DATA achado;
        HANDLE busca = FindFirstFile((raiz + "\\*").c_str(), &achado);
        if (busca == INVALID_HANDLE_VALUE)
            return;

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
            // ContentItemId e sempre positivo. Assim uma ROM nunca disputa a entrada
            // de cache de um jogo da FreeStyle.
            r.id             = -(int)(saida.size() + 1);
            r.titleId        = emuladores::IdDaRom(emu.titleId, rotulo + nome);
            // O nome de exibicao troca, o nome do ARQUIVO nao: o id sintetico e a
            // busca da capa continuam saindo do arquivo, entao trocar um rotulo no
            // nomes.txt nao tira a ROM das colecoes nem invalida a capa.
            std::string rotuloDele = RotuloDe(nome);
            r.nome           = rotuloDele.empty() ? SemExtensao(nome) : rotuloDele;
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
            r.titleIdEmulador = emu.titleId;
            r.arquivoRom      = rotulo + nome;   // exatamente o que semeia o id acima

            // A arte e um arquivo solto com o nome da ROM. Nada de casamento
            // aproximado aqui: isso acontece no PC, uma vez. Ver a decisao 115.
            if (!pastaDoApp.empty())
            {
                std::string base = pastaDoApp + "\\capas\\" + SemExtensao(nome);
                if (Existe(base + ".jpg"))      r.capa = base + ".jpg";
                else if (Existe(base + ".png")) r.capa = base + ".png";
            }

            saida.push_back(r);
            quantas++;
        }

        if (quantas > 0)
            diario::Escrever("emulador '%s': %d ROMs em %s",
                             emu.nome.c_str(), quantas, raiz.c_str());
    }

    namespace
    {
        // Onde o laco caro ja achou raiz de ROM, para o arranque seguinte nao repetir
        // o passeio pelos 223 XEX soltos. Guarda a RAIZ, nunca a ROM: as ROMs continuam
        // saindo de um FindFirstFile vivo, senao uma ROM nova so apareceria no arranque
        // depois -- e, pior, o id sintetico poderia ser servido de cache para um arquivo
        // que nao existe mais.
        const char *ARQ_CACHE = "game:\\emus.txt";

        // Identidade do conjunto de XEX soltos. Nao toca o disco: sai do g_jogos que ja
        // esta na memoria. Emulador instalado ou removido muda o conjunto de caminhos,
        // o carimbo erra e a busca completa roda de novo.
        unsigned int CarimboDeXexSoltos(const std::vector<biblioteca::Jogo> &jogos)
        {
            unsigned int h = 2166136261u;
            for (size_t i = 0; i < jogos.size(); i++)
            {
                if (jogos[i].tipoArquivo != 1)
                    continue;
                const std::string &c = jogos[i].caminho;
                for (size_t k = 0; k < c.size(); k++)
                {
                    h ^= (unsigned char)c[k];
                    h *= 16777619u;
                }
                h ^= (unsigned char)'\n';      // separador: "ab"+"c" nao pode dar "a"+"bc"
                h *= 16777619u;
            }
            return h;
        }

        const biblioteca::Jogo *PorCaminho(const std::vector<biblioteca::Jogo> &jogos,
                                           const std::string &caminho)
        {
            for (size_t i = 0; i < jogos.size(); i++)
                if (jogos[i].caminho == caminho)
                    return &jogos[i];
            return NULL;
        }

        // Devolve true so com o cache INTEIRO valido. O cache guarda o caminho do
        // emulador, nao os campos dele: o titleId e o nome vem sempre do banco vivo, e
        // assim nao ha como servir dado velho de emulador.
        bool LerCache(unsigned int carimbo,
                      std::vector<std::string> &caminhos, std::vector<std::string> &raizes)
        {
            caminhos.clear();
            raizes.clear();

            FILE *f = fopen(ARQ_CACHE, "r");
            if (f == NULL)
                return false;

            bool viCarimbo = false;
            char linha[1024];
            while (fgets(linha, sizeof(linha), f) != NULL)
            {
                std::string s = linha;
                while (!s.empty() && (s[s.size() - 1] == '\n' || s[s.size() - 1] == '\r'))
                    s.erase(s.size() - 1);
                if (s.empty() || s[0] == '#')
                    continue;

                if (!viCarimbo)
                {
                    // Primeira linha util e o carimbo, em hexa. Errou, nao le o resto.
                    unsigned int lido = 0;
                    if (sscanf(s.c_str(), "%x", &lido) != 1 || lido != carimbo)
                    {
                        fclose(f);
                        return false;
                    }
                    viCarimbo = true;
                    continue;
                }

                const size_t barra = s.find('|');
                if (barra == std::string::npos)
                    continue;
                caminhos.push_back(s.substr(0, barra));
                raizes.push_back(s.substr(barra + 1));
            }
            fclose(f);
            return viCarimbo;
        }

        void GravarCache(unsigned int carimbo,
                         const std::vector<std::string> &caminhos,
                         const std::vector<std::string> &raizes)
        {
            FILE *f = fopen(ARQ_CACHE, "w");
            if (f == NULL)
            {
                diario::Escrever("AVISO: nao consegui gravar %s", ARQ_CACHE);
                return;
            }
            fprintf(f, "# CollectionUI: onde varrer ROM. Apague para forcar nova busca.\n");
            fprintf(f, "%08X\n", carimbo);
            for (size_t i = 0; i < raizes.size(); i++)
                fprintf(f, "%s|%s\n", caminhos[i].c_str(), raizes[i].c_str());
            fclose(f);
        }

        // O laco caro: passeia por todo XEX solto da biblioteca atras de pasta de ROM.
        // E ele que o cache existe para pular.
        void Descobrir(const std::vector<biblioteca::Jogo> &jogos,
                       std::vector<std::string> &caminhos, std::vector<std::string> &raizes)
        {
            DWORD msResolver = 0, msSondar = 0;
            int   soltos = 0, semArquivo = 0;

            for (size_t e = 0; e < jogos.size(); e++)
            {
                const biblioteca::Jogo &emu = jogos[e];

                // So XEX solto: um emulador empacotado em container nao tem pasta de ROM ao
                // lado para varrer.
                if (emu.tipoArquivo != 1)
                    continue;
                soltos++;

                // Pelo dispositivo de verdade, nao supondo Hdd: -- o emulador pode estar num
                // pendrive, e e o lancador que ja sabe sondar os apelidos.
                DWORD t = GetTickCount();
                std::string xex = lancador::Resolver(emu.caminho);
                msResolver += GetTickCount() - t;
                if (xex.empty())
                {
                    semArquivo++;       // linha morta do content.db: jogo que saiu do disco
                    continue;
                }

                std::string pasta = PastaDo(xex);
                if (pasta.empty())
                    continue;

                t = GetTickCount();
                for (int k = 0; k < QUANTAS_PASTAS; k++)
                {
                    std::string raiz = pasta + "\\" + PASTAS[k];
                    if (!Existe(raiz))
                        continue;
                    caminhos.push_back(emu.caminho);
                    raizes.push_back(raiz);
                }
                msSondar += GetTickCount() - t;
            }

            diario::Escrever("  busca completa: %d XEX soltos, %u ms resolvendo "
                             "(%d sem arquivo no disco), %u ms sondando pasta de ROM",
                             soltos, (unsigned)msResolver, semArquivo, (unsigned)msSondar);
        }

        // A pasta de ROM e, depois, UM nivel de subpasta. O FBANext guarda tudo em
        // subpasta por sistema (Arcade, megadrive, neocdz, pce) e sem isto nenhuma ROM
        // dele aparecia. Um nivel so, de proposito: varrer fundo custa tempo de arranque
        // e entra em pasta de save e de arte.
        //
        // Isto roda SEMPRE, com cache ou sem: e o trabalho util, e e o que faz uma ROM
        // posta por FTP aparecer ja no arranque seguinte.
        void VarrerRaiz(const biblioteca::Jogo &emu, const std::string &raiz,
                        const std::string &pastaDoApp,
                        std::vector<biblioteca::Jogo> &saida)
        {
            std::vector<std::string> ondeVarrer, rotulos;
            ondeVarrer.push_back(raiz);
            rotulos.push_back("");

            WIN32_FIND_DATA sub;
            HANDLE bsub = FindFirstFile((raiz + "\\*").c_str(), &sub);
            if (bsub != INVALID_HANDLE_VALUE)
            {
                do
                {
                    if (!(sub.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                        continue;
                    if (strcmp(sub.cFileName, ".") == 0 ||
                        strcmp(sub.cFileName, "..") == 0)
                        continue;

                    ondeVarrer.push_back(raiz + "\\" + sub.cFileName);
                    rotulos.push_back(std::string(sub.cFileName) + "\\");
                }
                while (FindNextFile(bsub, &sub));
                FindClose(bsub);
            }

            for (size_t d = 0; d < ondeVarrer.size(); d++)
                Varrer(emu, ondeVarrer[d], rotulos[d], pastaDoApp, saida);
        }
    }

    void Ler(const std::vector<biblioteca::Jogo> &jogos,
             const std::string &pastaDoApp,
             std::vector<biblioteca::Jogo> &saida)
    {
        saida.clear();
        CarregarNomes(pastaDoApp);

        std::vector<std::string> caminhos, raizes;
        const unsigned int carimbo = CarimboDeXexSoltos(jogos);
        bool doCache = LerCache(carimbo, caminhos, raizes);

        if (doCache)
        {
            // Rede de protecao para o que o carimbo nao ve: a pasta renomeada no
            // disco sem o banco mudar, ou o pendrive que nao esta montado hoje.
            // Custa uma chamada por raiz -- quatro, nao duzentas e vinte e tres.
            for (size_t i = 0; i < raizes.size() && doCache; i++)
                if (PorCaminho(jogos, caminhos[i]) == NULL || !Existe(raizes[i]))
                    doCache = false;
        }

        if (!doCache)
        {
            caminhos.clear();
            raizes.clear();
            Descobrir(jogos, caminhos, raizes);
            GravarCache(carimbo, caminhos, raizes);
        }

        diario::Escrever("  onde varrer: %d raiz(es), %s",
                         (int)raizes.size(), doCache ? "do cache" : "busca completa");

        for (size_t i = 0; i < raizes.size(); i++)
        {
            const biblioteca::Jogo *emu = PorCaminho(jogos, caminhos[i]);
            if (emu != NULL)
                VarrerRaiz(*emu, raizes[i], pastaDoApp, saida);
        }

        diario::Escrever("ROMs de emulador encontradas: %d", (int)saida.size());
    }
}
