// Leitura de capas em thread separada.
//
// Ler alguns MB de um .assets bloqueia por dezenas de milissegundos. Feito dentro do
// laco de desenho, isso e o engasgo que se ve ao rolar. O FreeStyle resolve com um
// pool de threads fixadas em outra thread de hardware do Xenon; fazemos o mesmo, com
// uma diferenca: aqui o worker SO le os bytes. Criar a textura continua sendo da
// thread de render, para o D3D nao ser tocado por duas threads.
#ifndef CARREGADOR_H
#define CARREGADOR_H

#include <string>
#include <vector>

namespace carregador
{
    void Iniciar();
    void Parar();

    // Enfileira a leitura da capa de um .assets. O worker faz tudo: abre, interpreta o
    // container FSDA, acha a entrada da capa e le os bytes. Nenhum toque em disco sobra
    // para a thread de desenho.
    void Pedir(int indice, const std::string &arquivoAssets);

    // Descarta o que ainda nao comecou. Usado ao rolar depressa: o que saiu da janela
    // nao interessa mais, e insistir nele atrasa o que esta na tela.
    void DescartarPendentes();

    // Devolve um resultado pronto, ou false. Chamar da thread de render.
    // "inteira" sai true quando os bytes sao a imagem toda -- capa pequena do FSDA ou
    // .jpg de ROM -- e false quando sao o ENCARTE (900x600), de que a tela mostra so a
    // frente. Quem sabe disso e quem LEU o arquivo: o mesmo jogo rende encarte ou capa
    // pequena conforme o que existir dentro dele, entao a resposta nao cabe no Jogo.
    bool Retirar(int *indice, std::vector<unsigned char> &bytes, bool *inteira);
}

#endif
