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
}

#endif
