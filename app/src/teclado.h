// Teclado virtual do sistema (a Guide do Xbox), ASSÍNCRONO.
//
// Assíncrono não é preferência: é a única forma que não trava o console. A Guide
// desenha POR CIMA do quadro do título, então um título que para de apresentar deixa
// o sistema sem nada para compor -- e o console congela inteiro, sem nem responder ao
// botão Guide. Foi exatamente o que aconteceu com a versão que bloqueava o laço num
// WaitForSingleObject(INFINITE).
//
// A amostra oficial do XDK (Source/Samples/Online/StringVerify/StringVerify.cpp:161)
// faz assim: dispara, volta para o laço, e a cada quadro consulta
// XHasOverlappedIoCompleted enquanto continua desenhando normalmente.
//
// Os buffers vivem no módulo, nunca na pilha. A doc do XShowKeyboardUI é explícita:
// "the buffers ... must be guaranteed to remain valid until the operation is
// finished. For this reason, the buffer should not be declared on the stack."
#ifndef TECLADO_H
#define TECLADO_H

#include <string>

namespace teclado
{
    // Dispara o teclado. Devolve false se o sistema recusou abrir.
    bool Abrir(const char *titulo, const char *descricao, const char *textoInicial);

    bool Aberto();

    // Devolve true no quadro em que o teclado terminou. 'confirmou' diz se o usuário
    // apertou Concluir (false = cancelou, ou fechou pela Guide).
    bool Terminou(bool *confirmou, std::string &saida);
}

#endif
