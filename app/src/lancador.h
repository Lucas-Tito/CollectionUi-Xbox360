// Lancamento de jogo. E o fim de linha do app: quando da certo, o console reinicia
// no jogo e o CollectionUI morre. Voltar para ca nao faz parte da experiencia.
//
// Duas APIs do XDK, nenhuma linha do FreeStyle:
//   XEX/XBE solto  -> XLaunchNewImage          (xbox.h:417,  DECLSPEC_NORETURN VOID)
//   container      -> XContentLaunchImageFromFile (xbox.h:1293, devolve DWORD)
//
// A segunda monta o pacote e lanca de dentro dele numa chamada so, e -- ao contrario
// da primeira -- DEVOLVE codigo de erro. Ela e de maio/2011; o fonte aberto do FSD 2
// e de julho/2011 e usa o caminho antigo (XamContentOpenFile + montar + lancar).
#ifndef LANCADOR_H
#define LANCADOR_H

#include <string>
#include "biblioteca.h"

namespace lancador
{
    // Acha em qual dispositivo o jogo esta, testando os apelidos montados.
    //
    // O content.db guarda o caminho SEM o volume ("\JOGOS\X\default.xex"): o prefixo e
    // reconstruido a cada boot, e o do FreeStyle ("Hdd1:") nao e o nosso ("Hdd:").
    // Sondar custa no maximo 4 GetFileAttributes, e e de graca: o teste de existencia
    // teria de ser feito de qualquer jeito, porque XLaunchNewImage nao devolve erro.
    //
    // Devolve "" se o arquivo nao esta em nenhum dispositivo.
    std::string Resolver(const std::string &caminhoRelativo);

    // So devolve se FALHOU -- no sucesso o console ja reiniciou. Enche 'erro' com algo
    // que da para mostrar na tela.
    //
    // Quem chama tem de ter parado antes toda a I/O pendente: a doc do XDK proibe
    // lancar com leitura de disco em voo.
    bool Lancar(const biblioteca::Jogo &jogo, std::string &erro);
}

#endif
