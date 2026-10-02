// Log em arquivo. Escreve com WriteFile + FlushFileBuffers, nao com FILE*/fflush:
// num crash de verdade o que ficou no cache do sistema de arquivos se perde, e a linha
// que mais importa e sempre a ultima. Protegido por CRITICAL_SECTION porque a thread do
// carregador tambem escreve aqui.
//
// Log em arquivo. E o unico canal de diagnostico que temos: o OutputDebugString do 360
// so sai pelo XBDM, que exigiria plugin no console e um leitor que nao existe pronto.
// Um arquivo ao lado do .xex se le pelo pendrive ou por FTP.
#ifndef DIARIO_H
#define DIARIO_H

namespace diario
{
    void Abrir(const char *caminho);   // trunca: cada execucao comeca limpa
    void Escrever(const char *formato, ...);
    void Fechar();

    // Reabre em modo APPEND o ultimo caminho passado a Abrir. E para o caso do
    // lancamento de jogo, que fecha o arquivo antes de reiniciar o console: se o
    // lancamento falha e voltamos vivos, reabrir com Abrir truncaria justamente o
    // registro que explica a falha.
    void Reabrir();
}

#endif
