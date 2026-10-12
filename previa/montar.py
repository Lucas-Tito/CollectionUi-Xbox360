#!/usr/bin/env python3
"""Monta a prévia num .html autocontido.

Reaproveita a extração do protótipo antigo (mesmos dados, mesmas capas já
recortadas): rode antes `python3 prototipo/extrair.py`.
"""
import argparse, base64, json, os

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', default=os.path.join(RAIZ, 'prototipo', 'build'),
                    help='saída de prototipo/extrair.py')
    ap.add_argument('--saida', default=os.path.join(AQUI, 'build', 'previa.html'))
    ap.add_argument('--vault', help='vault.txt exportado pelo console; traz homebrew e ROM')
    ap.add_argument('--proposta', help='.js carregado POR CIMA da prévia, para ver uma '
                                       'proposta sem sujar o espelho do console')
    a = ap.parse_args()

    jogos = json.load(open(os.path.join(a.build, 'games.json'), encoding='utf-8'))
    if a.vault:
        jogos += do_vault(a.vault)
    for j in jogos:
        j.pop('bg', None)
        if j.get('cap'):
            with open(os.path.join(a.build, j['cap']), 'rb') as f:
                j['cap'] = 'data:image/webp;base64,' + base64.b64encode(f.read()).decode()
        for k in [k for k, v in list(j.items()) if v in (None, '')]:
            j.pop(k)

    dados = json.dumps(jogos, ensure_ascii=False, separators=(',', ':')).replace('</', '<\\/')
    html = open(os.path.join(AQUI, 'template.html'), encoding='utf-8').read().replace('__DADOS__', dados)

    # A prévia é o ESPELHO do que o console desenha. Proposta não entra nela: entra
    # por cima, num arquivo cujo nome diz o que é, e que gera uma página separada.
    if a.proposta:
        patch = open(a.proposta, encoding='utf-8').read()
        html += '\n<script>\n' + patch + '\n</script>\n'

    os.makedirs(os.path.dirname(a.saida), exist_ok=True)
    open(a.saida, 'w', encoding='utf-8').write(html)
    print('%s — %.2f MB' % (a.saida, len(html.encode()) / 1048576))

if __name__ == '__main__':
    main()
