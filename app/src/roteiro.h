// Roteiro de controle: um arquivo de texto aperta os botões no lugar de quem joga,
// tira fotos da tela e devolve o console à dashboard no fim. Serve para conferir a
// interface inteira de longe, pelo xbdm, sem ninguém no sofá.
//
// O mesmo costume do bt3xbx (port/x360/headless/roteiro.c), reduzido ao que um app de
// menus precisa. Mora em game:\roteiro.txt e é de UMA vez: ao começar, o app o renomeia
// para roteiro.lido, então um arquivo esquecido no console nunca toma conta da próxima
// abertura normal. Sem o arquivo, nada muda.
//
// Passos, um por linha ('#' começa comentário):
//   espera MS              espera MS milissegundos
//   espera capas [MS]      espera não haver capa em voo (no máximo MS, padrão 8000)
//   aperta BOTAO [N]       aperta e solta, N vezes (A B X Y START BACK LB RB
//                          CIMA BAIXO ESQUERDA DIREITA)
//   foto NOME              grava o quadro em game:\fotos\NOME.bmp (1280x720, 24 bits)
//   teclado TEXTO          a próxima abertura do teclado devolve TEXTO sem abrir a Guide
//   sai                    volta à dashboard (todo roteiro termina assim)
//
// Passo que não se entende para o roteiro com "roteiro: FALHA" no diário e também sai
// para a dashboard: quem espera do outro lado (o vigia do xbdm) não fica pendurado.
#ifndef ROTEIRO_H
#define ROTEIRO_H

#include <xtl.h>
#include <string>

namespace roteiro
{
    // Lê e renomeia o arquivo. Devolve true se há roteiro a rodar.
    bool Iniciar();

    bool Ativo();

    // Uma vez por quadro, antes de ler os controles. 'ocioso' diz se não há capa em
    // voo. Devolve os botões que o roteiro está segurando neste quadro.
    WORD Quadro(DWORD agora, bool ocioso);

    // O nome da foto pedida para este quadro (sem extensão), ou NULL. Quem desenha
    // chama FotoTirada() depois de gravar, e o roteiro segue.
    const char *FotoPedida();
    void FotoTirada(bool ok);

    // true quando o roteiro mandou sair (pelo passo "sai" ou por falha).
    bool QuerSair();

    // O teclado pergunta antes de abrir a Guide: com resposta pronta, não abre.
    bool RespostaDoTeclado(std::string &texto);
}

#endif
