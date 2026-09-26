#!/usr/bin/env python3
"""Monta a prévia num .html autocontido.

Reaproveita a extração do protótipo antigo (mesmos dados, mesmas capas já
recortadas): rode antes `python3 prototipo/extrair.py`.
"""
import argparse, base64, json, os

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(RAIZ, 'prototipo', 'build'),
                    help='saída de prototipo/extrair.py')
    ap.add_argument('--saida', default=os.path.join(AQUI, 'build', 'previa.html'))
    a = ap.parse_args()

    jogos = json.load(open(os.path.join(a.build, 'games.json'), encoding='utf-8'))
    for j in jogos:
        j.pop('bg', None)
        if j.get('cap'):
            with open(os.path.join(a.build, j['cap']), 'rb') as f:
                j['cap'] = 'data:image/webp;base64,' + base64.b64encode(f.read()).decode()
        for k in [k for k, v in list(j.items()) if v in (None, '')]:
            j.pop(k)

    dados = json.dumps(jogos, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
    html = open(os.path.join(AQUI, 'template.html'), encoding='utf-8').read().replace('__DADOS__', dados)

    os.makedirs(os.path.dirname(a.saida), exist_ok=True)
    open(a.saida, 'w', encoding='utf-8').write(html)
    print('%s — %.2f MB' % (a.saida, len(html.encode()) / 1048576))

if __name__ == '__main__':
    main()
