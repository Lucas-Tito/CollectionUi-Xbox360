#!/usr/bin/env python3
"""Junta template.html + build/games.json + as capas num único .html autocontido.

As imagens entram como data: URI para o arquivo abrir sozinho, por duplo clique ou
publicado como artifact, sem servidor e sem pasta ao lado.
"""
import argparse, base64, json, os

AQUI = os.path.dirname(os.path.abspath(__file__))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(AQUI, 'build'))
    ap.add_argument('--saida', default=os.path.join(AQUI, 'build', 'collectionui.html'))
    a = ap.parse_args()

    jogos = json.load(open(os.path.join(a.build, 'games.json'), encoding='utf-8'))
    for j in jogos:
        if j.get('cap'):
            with open(os.path.join(a.build, j['cap']), 'rb') as f:
                j['cap'] = 'data:image/webp;base64,' + base64.b64encode(f.read()).decode()
    dados = json.dumps(jogos, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
    html = open(os.path.join(AQUI, 'template.html'), encoding='utf-8').read().replace('__DADOS__', dados)
    open(a.saida, 'w', encoding='utf-8').write(html)
    print('%s — %.2f MB' % (a.saida, len(html.encode()) / 1048576))

if __name__ == '__main__':
    main()
