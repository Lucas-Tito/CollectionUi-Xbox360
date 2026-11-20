#!/usr/bin/env python3
"""Monta a prévia num .html autocontido.

Reaproveita a extração do protótipo antigo (mesmos dados, mesmas capas já
recortadas): rode antes `python3 prototipo/extrair.py`.
"""
import argparse, base64, json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import atlas
import mock
import ficha

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)

def do_vault(caminho):
    """Homebrew e ROM a partir do vault.txt, SEM capa.

    O games.json vem do content.db e por isso só tem o que a FreeStyle cataloga como
    jogo: não há uma linha sequer de \\HOMEBREW\\, e ROM de emulador nem passa pelo
    banco -- quem a enumera é o emuladores.cpp, em tempo de execução. Sem estas duas
    famílias as abas "Homebrew" e "Emulador" não existiriam na prévia, que é
    justamente o que se quer olhar.

    São dados REAIS (nome, id e caminho saem do console); só a capa falta, e a grade
    já sabe desenhar jogo sem capa. Os ids são renumerados a partir de 10000 porque o
    ContentItemId do homebrew colide com o de jogo do dump antigo.
    """
    saida, n = [], 10000
    for linha in open(caminho, encoding='utf-8', errors='replace'):
        linha = linha.strip()
        if not linha or linha.startswith('#'):
            continue
        campos = linha.split('|')
        if campos[0] == 'APP' and len(campos) >= 7:
            saida.append({'id': n, 'nome': campos[5], 'tid': campos[1], 'caminho': campos[6]})
        elif campos[0] == 'ROM' and len(campos) >= 7:
            saida.append({'id': n, 'nome': campos[5], 'tid': campos[1],
                          'caminho': campos[6], 'rom': 1})
        else:
            continue
        n += 1
    return saida


def do_colecoes(caminho, jogos):
    """O colecoes.txt do console no formato da prévia. O console guarda TitleId (hexa) e
    a prévia guarda o id do item, então cada TitleId vira TODOS os itens que o têm, como
    no app (os dois discos de um multi-disco). TitleId sem item na prévia fica de fora,
    e é dito: é dado da prévia desatualizado, não regra do app."""
    por_tid = {}
    for j in jogos:
        if j.get('tid'):
            por_tid.setdefault(j['tid'].upper(), []).append(j['id'])
    cols, sem = [], set()
    for linha in open(caminho, encoding='utf-8', errors='replace'):
        linha = linha.rstrip('\r\n')
        if not linha or linha.startswith('#') or linha.count('|') < 3:
            continue
        cid, tipo, nome, conteudo = linha.split('|', 3)
        c = {'id': int(cid), 'tipo': tipo, 'nome': nome, 'ids': [], 'origens': []}
        itens = [x for x in conteudo.split(',') if x]
        if tipo == 'jogos':
            for t in itens:
                if t.upper() in por_tid: c['ids'] += por_tid[t.upper()]
                else: sem.add(t.upper())
        else:
            c['origens'] = [int(x) for x in itens]
        cols.append(c)
    if sem:
        print('colecoes: %d TitleIds sem item na prévia (biblioteca da prévia mais velha que a do console)'
              % len(sem), file=sys.stderr)
    return cols


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(RAIZ, 'prototipo', 'build'),
                    help='saída de prototipo/extrair.py')
    ap.add_argument('--saida', default=os.path.join(AQUI, 'build', 'previa.html'))
    ap.add_argument('--vault', help='vault.txt exportado pelo console; traz homebrew e ROM')
    ap.add_argument('--colecoes', help='colecoes.txt do console; vence o que o navegador guardou')
    ap.add_argument('--mock', action='store_true',
                    help='com --colecoes: itens falsos no lugar de cada TitleId que a prévia não tem '
                         '(previa/mock.py), para as abas e as contagens saírem como no console')
    ap.add_argument('--proposta', help='.js carregado POR CIMA da prévia, para ver uma '
                                       'proposta sem sujar o espelho do console')
    a = ap.parse_args()

    jogos = json.load(open(os.path.join(a.build, 'games.json'), encoding='utf-8'))
    if a.vault:
        jogos += do_vault(a.vault)
    if a.mock:
        if not a.colecoes:
            ap.error('--mock precisa de --colecoes')
        presentes = None
        if a.vault:
            presentes = {l.split('|')[1].strip().upper() for l in open(a.vault, encoding='utf-8', errors='replace')
                         if l.count('|') >= 1 and not l.startswith('#')}
        falsos = mock.completar(a.colecoes, jogos, presentes)
        print('mock: %d itens falsos (%d com capa do vault)' % (len(falsos), sum(1 for j in falsos if j.get('cap'))),
              file=sys.stderr)
        jogos += falsos
    # A ficha de cada jogo (previa/ficha.py): só o que a tela de detalhes mostra.
    fichas = ficha.fichas(jogos)
    for j in jogos:
        if j['id'] in fichas: j['ficha'] = fichas[j['id']]
    print('fichas: %d de %d itens com dados' % (len(fichas), len(jogos)), file=sys.stderr)
    for j in jogos:
        j.pop('bg', None)
        if j.get('cap') and not j['cap'].startswith('data:'):
            with open(os.path.join(a.build, j['cap']), 'rb') as f:
                j['cap'] = 'data:image/webp;base64,' + base64.b64encode(f.read()).decode()
        for k in [k for k, v in list(j.items()) if v in (None, '')]:
            j.pop(k)

    dados = json.dumps(jogos, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
    html = open(os.path.join(AQUI, 'template.html'), encoding='utf-8').read().replace('__DADOS__', dados)
    cols = do_colecoes(a.colecoes, jogos) if a.colecoes else None
    html = html.replace('__COLECOES__', json.dumps(cols, ensure_ascii=False).replace('</', '<\\/'))
    html = html.replace('__FONTE__', json.dumps(atlas.ler(), separators=(',', ':')))

    # O desenho Metro (previa/metro.js) foi para o C++ em 19/11 (decisão 149): ele
    # passa a ser parte do espelho e entra sempre, por cima do template, que segue
    # sendo a base comum (texto pelo atlas, listas, navegação).
    metro = open(os.path.join(AQUI, 'metro.js'), encoding='utf-8').read()
    html += '\n<script>\n' + metro + '\n</script>\n'

    # A prévia é o ESPELHO do que o console desenha. Proposta não entra nela: entra
    # por cima, num arquivo cujo nome diz o que é, e que gera uma página separada.
    if a.proposta and os.path.abspath(a.proposta) != os.path.join(AQUI, 'metro.js'):
        patch = open(a.proposta, encoding='utf-8').read()
        html += '\n<script>\n' + patch + '\n</script>\n'

    os.makedirs(os.path.dirname(a.saida), exist_ok=True)
    open(a.saida, 'w', encoding='utf-8').write(html)
    print('%s — %.2f MB' % (a.saida, len(html.encode()) / 1048576))

if __name__ == '__main__':
    main()
