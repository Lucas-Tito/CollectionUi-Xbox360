#include "som.h"
#include "diario.h"
#include <xtl.h>
#include <xaudio2.h>
#include <xma2defs.h>
#include <string.h>

namespace
{
    // Intervalo minimo entre dois sons IGUAIS. Sem isto, segurar o analogico dispara o
    // som de foco a cada repeticao do direcional e vira metralhadora.
    const DWORD INTERVALO_MIN = 45;

    struct Voz
    {
        IXAudio2SourceVoice *voz;
        BYTE                *dados;     // XPhysicalAlloc, alinhado em 2 KB
        DWORD                tamanho;
        DWORD                ultimoToque;
    };

    IXAudio2               *g_motor = NULL;
    IXAudio2MasteringVoice *g_mestre = NULL;
    Voz                     g_vozes[som::SOM_QUANTOS];

    const char *ARQUIVOS[som::SOM_QUANTOS] =
    {
        "game:\\media\\sounds\\btn_Focus.xma",     // SOM_FOCO
        "game:\\media\\sounds\\btn_Select.xma",    // SOM_CONFIRMA
        "game:\\media\\sounds\\btn_Back.xma",      // SOM_VOLTA
        "game:\\media\\sounds\\flyout.xma",        // SOM_MENU
        "game:\\media\\sounds\\NotifyPopup.xma"    // SOM_ERRO
    };

    // Le um DWORD little-endian byte a byte.
    //
    // Duas razoes para nao fazer um cast de ponteiro: o RIFF e little-endian e o Xenon
    // e big-endian, e um chunk pode comecar em endereco nao alinhado -- no PowerPC
    // isso nao e so lento, pode falhar.
    DWORD LerDwordLE(const BYTE *p)
    {
        return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
    }

    // Acha um chunk do RIFF. Nao assume posicao fixa: o layout observado e
    // fmt/seek/data, mas quem garante isso e a varredura, nao a esperanca.
    bool AcharChunk(const BYTE *arquivo, DWORD total, const char *id,
                    DWORD *inicio, DWORD *tamanho)
    {
        if (total < 12 || memcmp(arquivo, "RIFF", 4) != 0 || memcmp(arquivo + 8, "WAVE", 4) != 0)
            return false;

        DWORD p = 12;
        while (p + 8 <= total)
        {
            DWORD tam = LerDwordLE(arquivo + p + 4);

            if (memcmp(arquivo + p, id, 4) == 0)
            {
                if (p + 8 + tam > total)
                    return false;
                *inicio = p + 8;
                *tamanho = tam;
                return true;
            }
            p += 8 + tam + (tam & 1);
        }
        return false;
    }

    bool LerArquivo(const char *caminho, BYTE **saida, DWORD *tamanho)
    {
        HANDLE h = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return false;

        DWORD tam = GetFileSize(h, NULL);
        if (tam == 0xFFFFFFFF || tam == 0 || tam > 1024 * 1024)
        {
            CloseHandle(h);
            return false;
        }

        BYTE *buf = new BYTE[tam];
        DWORD lido = 0;
        BOOL ok = ReadFile(h, buf, tam, &lido, NULL);
        CloseHandle(h);

        if (!ok || lido != tam)
        {
            delete[] buf;
            return false;
        }

        *saida = buf;
        *tamanho = tam;
        return true;
    }

    bool Montar(int i)
    {
        BYTE *arquivo = NULL;
        DWORD total = 0;
        if (!LerArquivo(ARQUIVOS[i], &arquivo, &total))
        {
            diario::Escrever("som: nao abri %s", ARQUIVOS[i]);
            return false;
        }

        DWORD fmtIni = 0, fmtTam = 0, dadosIni = 0, dadosTam = 0;
        if (!AcharChunk(arquivo, total, "fmt ", &fmtIni, &fmtTam) ||
            !AcharChunk(arquivo, total, "data", &dadosIni, &dadosTam) ||
            fmtTam < sizeof(XMA2WAVEFORMATEX))
        {
            diario::Escrever("som: %s nao e XMA2 valido (fmt=%u data=%u)",
                             ARQUIVOS[i], fmtTam, dadosTam);
            delete[] arquivo;
            return false;
        }

        // O fmt do arquivo E o formato que o CreateSourceVoice quer -- nada a
        // converter. So a ordem de bytes precisa de atencao: o RIFF vem little-endian
        // de uma ferramenta de PC e o Xenon e big-endian. O proprio XDK resolve isso,
        // em xma2defs.h:679, detectando pela wFormatTag e trocando no lugar.
        XMA2WAVEFORMATEX formato;
        memcpy(&formato, arquivo + fmtIni, sizeof(formato));
        if (FAILED(LocalizeXma2Format(&formato)))
        {
            diario::Escrever("som: %s nao e XMA2 reconhecivel", ARQUIVOS[i]);
            delete[] arquivo;
            return false;
        }

        // "XMA packets must be 2K aligned" -- a amostra XAudio2BasicSound do XDK. Um
        // new BYTE[] daria um ponteiro qualquer, e quem le estes bytes e o
        // decodificador de hardware, nao a CPU.
        BYTE *dados = (BYTE *)XPhysicalAlloc(dadosTam, MAXULONG_PTR, 2048, PAGE_READWRITE);
        if (dados == NULL)
        {
            delete[] arquivo;
            return false;
        }
        memcpy(dados, arquivo + dadosIni, dadosTam);

        IXAudio2SourceVoice *voz = NULL;
        HRESULT hr = g_motor->CreateSourceVoice(&voz, (const WAVEFORMATEX *)&formato);
        delete[] arquivo;

        if (FAILED(hr))
        {
            diario::Escrever("som: CreateSourceVoice %s = 0x%08X", ARQUIVOS[i], hr);
            XPhysicalFree(dados);
            return false;
        }

        g_vozes[i].voz         = voz;
        g_vozes[i].dados       = dados;
        g_vozes[i].tamanho     = dadosTam;
        g_vozes[i].ultimoToque = 0;
        return true;
    }
}

namespace som
{
    void Iniciar()
    {
        ZeroMemory(g_vozes, sizeof(g_vozes));

        HRESULT hr = XAudio2Create(&g_motor, 0);
        if (FAILED(hr))
        {
            diario::Escrever("som: XAudio2Create = 0x%08X, seguindo mudo", hr);
            g_motor = NULL;
            return;
        }

        hr = g_motor->CreateMasteringVoice(&g_mestre);
        if (FAILED(hr))
        {
            diario::Escrever("som: CreateMasteringVoice = 0x%08X, seguindo mudo", hr);
            g_motor->Release();
            g_motor = NULL;
            return;
        }

        int montadas = 0;
        for (int i = 0; i < SOM_QUANTOS; i++)
            if (Montar(i))
                montadas++;

        diario::Escrever("som: %d de %d efeitos prontos", montadas, (int)SOM_QUANTOS);
    }

    void Parar()
    {
        for (int i = 0; i < SOM_QUANTOS; i++)
        {
            if (g_vozes[i].voz != NULL)
            {
                g_vozes[i].voz->Stop(0);
                g_vozes[i].voz->FlushSourceBuffers();
                g_vozes[i].voz->DestroyVoice();
                g_vozes[i].voz = NULL;
            }
            if (g_vozes[i].dados != NULL)
            {
                XPhysicalFree(g_vozes[i].dados);
                g_vozes[i].dados = NULL;
            }
        }

        if (g_mestre != NULL)
        {
            g_mestre->DestroyVoice();
            g_mestre = NULL;
        }
        if (g_motor != NULL)
        {
            g_motor->Release();
            g_motor = NULL;
        }
    }

    void Tocar(Efeito e)
    {
        if (e < 0 || e >= SOM_QUANTOS)
            return;

        Voz &v = g_vozes[e];
        if (v.voz == NULL)
            return;         // este efeito nao carregou; o resto do app segue igual

        DWORD agora = GetTickCount();
        if (v.ultimoToque != 0 && agora - v.ultimoToque < INTERVALO_MIN)
            return;
        v.ultimoToque = agora;

        // Recomecar, nao empilhar: em UI, o som novo cancela o anterior. Sem o Flush, o
        // Submit enfileiraria e o som atrasaria cada vez mais em relacao ao foco.
        v.voz->Stop(0);
        v.voz->FlushSourceBuffers();

        XAUDIO2_BUFFER buffer;
        ZeroMemory(&buffer, sizeof(buffer));
        buffer.pAudioData = v.dados;
        buffer.AudioBytes = v.tamanho;
        buffer.Flags      = XAUDIO2_END_OF_STREAM;

        if (SUCCEEDED(v.voz->SubmitSourceBuffer(&buffer)))
            v.voz->Start(0);
    }
}
