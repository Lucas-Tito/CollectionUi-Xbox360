// Som de interface.
//
// Os arquivos sao os .xma do skin padrao da FreeStyle. Sao RIFF/WAVE com codec XMA2
// (0x166), que o XAudio2 do 360 consome NATIVAMENTE -- nao ha decodificacao a
// escrever, quem decodifica e o hardware do Xenon. Por isso nao convertemos para PCM:
// converter jogaria fora o XMA2WAVEFORMATEX do proprio arquivo, que e exatamente o
// que o CreateSourceVoice quer receber.
//
// A FreeStyle nao toca som pelo C++: ela e uma aplicacao XUI e so chama
// XuiSoundXAudioRegister(), deixando as cenas .xur dispararem os sons. Seguir por ali
// exigiria trazer o XUI inteiro. O motor, porem, e o mesmo -- o nome da funcao diz:
// o backend de som do XUI E o XAudio2.
#ifndef SOM_H
#define SOM_H

namespace som
{
    enum Efeito
    {
        SOM_FOCO = 0,     // mover o foco; o mais frequente de longe
        SOM_CONFIRMA,     // A
        SOM_VOLTA,        // B
        SOM_MENU,         // abrir o menu de opcoes
        SOM_ERRO,         // aviso na tela
        SOM_QUANTOS
    };

    // Le os arquivos e monta uma voz por efeito. Falhar aqui nao e fatal: o app
    // continua mudo. Som e enfeite, nao pode derrubar um launcher.
    void Iniciar();

    // Solta tudo. OBRIGATORIO antes de lancar um jogo: a doc do XDK proibe lancar com
    // I/O pendente, e uma voz tocando e o motor de audio vivo.
    void Parar();

    void Tocar(Efeito e);
}

#endif
