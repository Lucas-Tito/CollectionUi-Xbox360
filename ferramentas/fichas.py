#!/usr/bin/env python3
"""Gera o fichas.txt que o app lê para a faixa de detalhes (Y na grade de jogos).

Uso: fichas.py [--vault VAULT_TXT] [--games GAMES_JSON] [--saida fichas.txt]

Os jogos são os do console: as linhas JOGO do vault.txt que o próprio app exporta (o
TitleId é a chave) e, junto, os do games.json da prévia, que trazem o que a FreeStyle
guarda (descrição, gênero, desenvolvedora, publicadora, nota, data). A ficha de cada um
é a mesma da prévia (previa/ficha.py): co-op só do Co-Optimus e só local, a nota do
Metacritic ou a dos jogadores, a descrição, o tempo do HowLongToBeat, ano, gênero,
desenvolvedora e publicadora.

Formato, uma linha por jogo, em UTF-8, campos separados por TAB:
    TITLEID  chave=valor  chave=valor ...
    coop  quantos no mesmo console (só se o Co-Optimus tem co-op local)
    cx    o que é de sofá, separado por " · "      ct   o texto do Co-Optimus
    mc    nota do Metacritic                       nj   nota dos jogadores (0 a 5)
    hm hp hc  horas: história, com extras, 100%    ano  ano de lançamento
    gen dev pub  gênero, desenvolvedora, publicadora
    desc  a descrição
Linha que começa com '#' é comentário. Valor não tem TAB nem quebra de linha.
"""
import argparse, json, os, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
sys.path.insert(0, os.path.join(RAIZ, 'previa'))
import ficha  # noqa: E402

# A descrição na faixa cabe em ~10 linhas de 470 px; o resto só pesaria no arquivo.
DESC_MAX = 900


def jogos_do_vault(caminho):
    saida = {}
    for linha in open(caminho, encoding='utf-8', errors='replace'):
        campos = linha.rstrip('\r\n').split('|')
        if campos[0] == 'JOGO' and len(campos) >= 7 and campos[1]:
            tid = campos[1].upper()
            saida.setdefault(tid, {'id': tid, 'tid': tid, 'nome': campos[5]})
    return saida


def limpo(v):
    # O app converte para Latin-1 (main.cpp, Larga): o que passa disso não tem glifo no
    # atlas e sairia como quadradinho. O ficha.limpa() já trocou a pontuação tipográfica.
    t = ''.join(ch for ch in str(v) if ord(ch) <= 0xFF)
    return ' '.join(t.replace('\t', ' ').split())


def gerar(vault, games, saida):
    jogos = jogos_do_vault(vault) if vault and os.path.exists(vault) else {}
    if games and os.path.exists(games):
        for g in json.load(open(games, encoding='utf-8')):
            tid = (g.get('tid') or '').upper()
            if not tid or tid == '00000000':
                continue
            j = jogos.setdefault(tid, {'id': tid, 'tid': tid, 'nome': g.get('nome', '')})
            for k in ('desc', 'genero', 'dev', 'pub', 'nota', 'lanc'):
                if g.get(k) and g[k] != 'N/A':
                    j[k] = g[k]
    fichas = ficha.fichas(list(jogos.values()))

    linhas = ['# CollectionUI: fichas da faixa de detalhes, geradas por ferramentas/fichas.py']
    for tid in sorted(fichas):
        f = fichas[tid]
        c = []
        if f.get('coop'):
            c.append('coop=%d' % int(f['coop']))
            if f.get('coopExtras'): c.append('cx=' + ' · '.join(f['coopExtras']))
            if f.get('coopTexto'):  c.append('ct=' + f['coopTexto'])
        if f.get('metacritic'):       c.append('mc=%d' % int(f['metacritic']))
        elif f.get('jogadores_nota'): c.append('nj=%.1f' % float(f['jogadores_nota']))
        t = f.get('tempo') or {}
        for k, chave in (('main', 'hm'), ('plus', 'hp'), ('cem', 'hc')):
            if t.get(k): c.append('%s=%g' % (chave, float(t[k])))
        if f.get('ano'):    c.append('ano=%d' % int(f['ano']))
        if f.get('genero'): c.append('gen=' + f['genero'])
        if f.get('dev'):    c.append('dev=' + f['dev'])
        if f.get('pub'):    c.append('pub=' + f['pub'])
        if f.get('descricao'):
            d = f['descricao']
            if len(d) > DESC_MAX:
                d = d[:DESC_MAX].rsplit(' ', 1)[0] + '...'
            c.append('desc=' + d)
        if c:
            linhas.append(tid + '\t' + '\t'.join(limpo(x) for x in c))
    texto = '\n'.join(linhas) + '\n'
    if saida:
        with open(saida, 'w', encoding='utf-8', newline='\n') as f:
            f.write(texto)
    return texto, len(linhas) - 1, len(jogos)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--vault', default=os.path.expanduser('~/Downloads/vault.txt'))
    ap.add_argument('--games', default=os.path.join(RAIZ, 'previa', 'build', 'games.json'))
    ap.add_argument('--saida', default=os.path.join(RAIZ, 'local', 'fichas.txt'))
    a = ap.parse_args()
    _, n, total = gerar(a.vault, a.games, a.saida)
    print('%s: %d fichas de %d jogos' % (a.saida, n, total))


if __name__ == '__main__':
    main()
