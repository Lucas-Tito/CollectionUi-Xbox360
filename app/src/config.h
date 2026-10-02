// Preferencias do app, num .ini ao lado do .xex.
//
// Texto puro de proposito: da para ler e editar do PC, e nao traz biblioteca de
// parsing para guardar duas linhas.
#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace config
{
    // Vazio se nao houver escolha gravada.
    std::string LerBanco();
    void        GravarBanco(const std::string &caminho);

    // Chaves liga/desliga para separar hipotese sem recompilar: basta editar o .ini
    // pelo FTP. "som=0" nao inicia o audio; "anel=0" nao desenha o anel do jogo
    // marcado. Ausente, vale o padrao (ligado).
    bool Ligado(const char *chave);
}

#endif
