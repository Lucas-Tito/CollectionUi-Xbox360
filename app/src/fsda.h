// Leitor do container FSDA, o formato em que o FreeStyle guarda a arte de cada jogo.
//
// Layout, tudo BIG-ENDIAN:
//   +0   char[4]  "FSDA"
//   +4   uint32   versao (1)
//   +8   uint32   0
//   +12  uint32   mascara de bits dos tipos presentes
//   +16  uint32   numero de entradas
//   +20  uint32   2
//   +24  entradas de 16 bytes: tipo, offset, tamanho, tamanho (repetido)
//
// Cada offset aponta para um DDS dentro do proprio arquivo, ja em DXT5.

#ifndef FSDA_H
#define FSDA_H

#include <vector>

namespace fsda
{
    // Tipos observados na biblioteca real. A cobertura varia: a capa grande estava
    // presente em 120 de 120 jogos, o fundo em 51.
    enum Tipo
    {
        TIPO_ICONE       = 1,    // 64x64
        TIPO_FUNDO       = 2,    // ate 1920x1080
        TIPO_BANNER      = 4,    // 420x96
        TIPO_CAPA_PEQ    = 8,    // 220x300
        TIPO_DESCONHECIDO = 64,  // 420x320, nao identificado
        TIPO_CAPA        = 128   // 900x600 -- e o ENCARTE inteiro, nao a capa
    };

    struct Imagem
    {
        unsigned int tipo;
        unsigned int offset;     // posicao do DDS dentro do arquivo
        unsigned int tamanho;
        unsigned int largura;
        unsigned int altura;
        char         formato[5]; // "DXT5", "DXT1"...
    };

    // Le a tabela de entradas e o cabecalho DDS de cada uma. Nao carrega pixel nenhum:
    // devolve so onde cada imagem esta, para quem chamou decidir o que subir para a GPU.
    bool Ler(const char *caminho, std::vector<Imagem> &saida);

    // Devolve a entrada de um tipo, ou NULL se o jogo nao tiver aquela arte.
    const Imagem *Achar(const std::vector<Imagem> &imagens, unsigned int tipo);
}

#endif
