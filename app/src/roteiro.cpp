#include "roteiro.h"
#include "diario.h"
#include <vector>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

namespace
{
    const char *ARQUIVO = "game:\\roteiro.txt";
    const char *LIDO    = "game:\\roteiro.lido";

    // Segurar e soltar: 100 ms cada. Menos que a ESPERA_INICIAL da repetição do
    // direcional (380 ms) para dar UM passo só, e mais que um quadro para o app ver a
    // borda mesmo num quadro lento.
    const DWORD SEGURA = 100;
    const DWORD SOLTA  = 100;

    enum Tipo { ESPERA, ESPERA_CAPAS, APERTA, FOTO, TECLADO, SAI };

    struct Passo
    {
        Tipo        tipo;
        int         linha;
        DWORD       ms;       // ESPERA, ESPERA_CAPAS (máximo)
        WORD        botao;    // APERTA
        int         vezes;    // APERTA
        std::string texto;    // FOTO, TECLADO
    };

    std::vector<Passo> g_passos;
    size_t      g_i       = 0;
    bool        g_ativo   = false;
    bool        g_sair    = false;
    bool        g_comecou = false;   // o passo atual já marcou seu início
    DWORD       g_inicio  = 0;
    int         g_feitas  = 0;       // APERTA: quantas já foram
    bool        g_fotoPendente = false;
    bool        g_respostaPronta = false;
    std::string g_resposta;

    struct Nome { const char *nome; WORD botao; };
    const Nome BOTOES[] = {
        { "A", XINPUT_GAMEPAD_A }, { "B", XINPUT_GAMEPAD_B },
        { "X", XINPUT_GAMEPAD_X }, { "Y", XINPUT_GAMEPAD_Y },
        { "START", XINPUT_GAMEPAD_START }, { "BACK", XINPUT_GAMEPAD_BACK },
        { "LB", XINPUT_GAMEPAD_LEFT_SHOULDER }, { "RB", XINPUT_GAMEPAD_RIGHT_SHOULDER },
        { "CIMA", XINPUT_GAMEPAD_DPAD_UP }, { "BAIXO", XINPUT_GAMEPAD_DPAD_DOWN },
        { "ESQUERDA", XINPUT_GAMEPAD_DPAD_LEFT }, { "DIREITA", XINPUT_GAMEPAD_DPAD_RIGHT },
    };

    void Falha(int linha, const char *motivo)
    {
        diario::Escrever("roteiro: FALHA: roteiro.txt:%d: %s", linha, motivo);
        g_sair = true;
    }

    // Tira espaço e fim de linha das pontas.
    std::string Apara(const char *s)
    {
        while (*s == ' ' || *s == '\t') s++;
        std::string r = s;
        while (!r.empty() && (r[r.size() - 1] == '\n' || r[r.size() - 1] == '\r' ||
                              r[r.size() - 1] == ' '  || r[r.size() - 1] == '\t'))
            r.erase(r.size() - 1);
        return r;
    }

    // Nome de foto vira nome de arquivo FATX: só letra, número, '-' e '_', até 36.
    bool NomeDeFotoOk(const std::string &s)
    {
        if (s.empty() || s.size() > 36) return false;
        for (size_t i = 0; i < s.size(); i++)
        {
            char c = s[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_'))
                return false;
        }
        return true;
    }

    bool Ler(FILE *f)
    {
        char buf[256];
        int linha = 0;
        while (fgets(buf, sizeof(buf), f) != NULL)
        {
            linha++;
            char *cerq = strchr(buf, '#');
            if (cerq != NULL) *cerq = '\0';
            std::string l = Apara(buf);
            if (l.empty()) continue;

            std::string cmd = l, resto;
            size_t sp = l.find(' ');
            if (sp != std::string::npos)
            {
                cmd = l.substr(0, sp);
                resto = Apara(l.c_str() + sp);
            }

            Passo p;
            p.linha = linha; p.ms = 0; p.botao = 0; p.vezes = 1;

            if (cmd == "espera")
            {
                if (strncmp(resto.c_str(), "capas", 5) == 0)
                {
                    p.tipo = ESPERA_CAPAS;
                    std::string n = Apara(resto.c_str() + 5);
                    p.ms = n.empty() ? 8000 : (DWORD)atoi(n.c_str());
                }
                else
                {
                    p.tipo = ESPERA;
                    p.ms = (DWORD)atoi(resto.c_str());
                    if (p.ms == 0) { Falha(linha, "espera sem milissegundos"); return false; }
                }
            }
            else if (cmd == "aperta")
            {
                p.tipo = APERTA;
                std::string nome = resto, n;
                size_t s2 = resto.find(' ');
                if (s2 != std::string::npos)
                {
                    nome = resto.substr(0, s2);
                    n = Apara(resto.c_str() + s2);
                }
                for (size_t k = 0; k < sizeof(BOTOES) / sizeof(BOTOES[0]); k++)
                    if (nome == BOTOES[k].nome) p.botao = BOTOES[k].botao;
                if (p.botao == 0) { Falha(linha, "botao desconhecido"); return false; }
                if (!n.empty()) p.vezes = atoi(n.c_str());
                if (p.vezes < 1 || p.vezes > 99) { Falha(linha, "vezes fora de 1..99"); return false; }
            }
            else if (cmd == "foto")
            {
                p.tipo = FOTO;
                p.texto = resto;
                if (!NomeDeFotoOk(p.texto)) { Falha(linha, "nome de foto: 1 a 36 de letra, numero, - ou _"); return false; }
            }
            else if (cmd == "teclado")
            {
                p.tipo = TECLADO;
                p.texto = resto;
            }
            else if (cmd == "sai")
            {
                p.tipo = SAI;
            }
            else
            {
                Falha(linha, "passo desconhecido");
                return false;
            }
            g_passos.push_back(p);
        }
        return true;
    }

    void Proximo()
    {
        g_i++;
        g_comecou = false;
        g_feitas = 0;
    }
}

namespace roteiro
{
    bool Iniciar()
    {
        FILE *f = fopen(ARQUIVO, "r");
        if (f == NULL)
            return false;

        g_ativo = true;
        bool ok = Ler(f);
        fclose(f);

        // De uma vez: renomeado ANTES de rodar, mesmo com erro. Um roteiro que trava o
        // app no meio não pode voltar a rodar na abertura seguinte.
        DeleteFile(LIDO);
        if (!MoveFile(ARQUIVO, LIDO))
        {
            diario::Escrever("roteiro: nao renomeou roteiro.txt para roteiro.lido (erro %u); "
                             "nao rodo, para nao rodar de novo na proxima abertura",
                             (unsigned)GetLastError());
            g_passos.clear();
            g_sair = false;
            g_ativo = false;
            return false;
        }

        if (ok && (g_passos.empty() || g_passos.back().tipo != SAI))
            diario::Escrever("roteiro: aviso: nao termina com 'sai'; o app fica aberto no fim");

        diario::Escrever("roteiro: %d passos%s", (int)g_passos.size(), ok ? "" : " (com FALHA: sai)");
        return true;
    }

    bool Ativo() { return g_ativo; }
    bool QuerSair() { return g_sair; }

    WORD Quadro(DWORD agora, bool ocioso)
    {
        if (!g_ativo || g_sair || g_i >= g_passos.size())
            return 0;

        Passo &p = g_passos[g_i];
        if (!g_comecou)
        {
            g_comecou = true;
            g_inicio = agora;
            switch (p.tipo)
            {
            case ESPERA:       diario::Escrever("roteiro: %d espera %u", p.linha, (unsigned)p.ms); break;
            case ESPERA_CAPAS: diario::Escrever("roteiro: %d espera capas", p.linha); break;
            case APERTA:       diario::Escrever("roteiro: %d aperta %04X x%d", p.linha, (unsigned)p.botao, p.vezes); break;
            case FOTO:         diario::Escrever("roteiro: %d foto %s", p.linha, p.texto.c_str());
                               g_fotoPendente = true; break;
            case TECLADO:      diario::Escrever("roteiro: %d teclado '%s'", p.linha, p.texto.c_str());
                               g_resposta = p.texto; g_respostaPronta = true;
                               Proximo(); return 0;
            case SAI:          diario::Escrever("roteiro: %d sai", p.linha);
                               g_sair = true; return 0;
            }
        }

        switch (p.tipo)
        {
        case ESPERA:
            if (agora - g_inicio >= p.ms) Proximo();
            return 0;

        case ESPERA_CAPAS:
            if (ocioso)
            {
                diario::Escrever("roteiro: capas prontas em %u ms", (unsigned)(agora - g_inicio));
                Proximo();
            }
            else if (agora - g_inicio >= p.ms)
            {
                diario::Escrever("roteiro: capas ainda em voo depois de %u ms; sigo", (unsigned)p.ms);
                Proximo();
            }
            return 0;

        case APERTA:
        {
            DWORD t = agora - g_inicio;
            if (t < SEGURA) return p.botao;
            if (t < SEGURA + SOLTA) return 0;
            g_feitas++;
            if (g_feitas >= p.vezes) Proximo();
            else g_inicio = agora;
            return 0;
        }

        default:
            return 0;   // FOTO espera o FotoTirada
        }
    }

    const char *FotoPedida()
    {
        if (!g_fotoPendente || g_i >= g_passos.size()) return NULL;
        return g_passos[g_i].texto.c_str();
    }

    void FotoTirada(bool ok)
    {
        if (!g_fotoPendente) return;
        g_fotoPendente = false;
        if (!ok) diario::Escrever("roteiro: a foto %s falhou; sigo", g_passos[g_i].texto.c_str());
        Proximo();
    }

    bool RespostaDoTeclado(std::string &texto)
    {
        if (!g_respostaPronta) return false;
        g_respostaPronta = false;
        texto = g_resposta;
        return true;
    }
}
