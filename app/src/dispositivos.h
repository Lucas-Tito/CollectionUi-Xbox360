// Montagem de dispositivos.
//
// Um titulo do Xbox 360 so enxerga "game:" por padrao -- a pasta de onde ele foi
// lancado. O HD e os pendrives existem no kernel como \Device\..., mas sem um apelido
// de unidade nenhuma chamada de arquivo os alcanca. Quem monta e o proprio app.
//
// Os nomes que o FreeStyle usa no banco dele ("Hdd1:") sao apelidos DELE, nao do
// sistema. Nos criamos os nossos.
#ifndef DISPOSITIVOS_H
#define DISPOSITIVOS_H

namespace dispositivos
{
    // Cria os apelidos de unidade. Falha em um nao impede os outros: um console pode
    // nao ter pendrive, e a maquina continua util so com o HD.
    void MontarTodos();

    // Lista, para diagnostico, quais apelidos responderam.
    const char *const *Apelidos(int *quantos);
}

#endif
