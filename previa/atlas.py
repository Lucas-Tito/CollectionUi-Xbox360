#!/usr/bin/env python3
"""Lê o app/media/Arial_16.xpr, o atlas de fonte que o console carrega, para a prévia
desenhar o texto com os MESMOS glifos (e a sombra que vem neles).

O formato (AtgResource.cpp e AtgFont.cpp do XDK):
  - XPR2: magia, tamanho do cabeçalho, tamanho dos dados, nº de recursos; cada recurso
    é (tipo, deslocamento, tamanho, nome), deslocamentos contados a partir do byte 12.
  - "FontTexture": cabeçalho D3D de 52 bytes; a constante de busca da GPU diz o formato
    (4_4_4_4), a largura, a altura, o passo e se há tiling. Os texels começam em 12 +
    tamanho do cabeçalho.
  - "FontData": versão 5, altura, margens, avanço de linha, a tabela de tradução
    (caractere -> glifo) e os glifos (tu1, tv1, tu2, tv2, deslocamento, largura,
    avanço, máscara de canal). Tudo big-endian.

Cada glifo mora num dos quatro canais de 4 bits do texel, o que a máscara escolhe. No
shader: valor > 0,5 é a letra (luz 2v-1, opaca); abaixo, é a sombra preta com alfa 2v.

Uso: atlas.py [--png DIR]   imprime o JSON para a prévia; --png grava os 4 canais e
                            uma amostra de texto para conferir a olho.
"""
import argparse, base64, json, os, struct, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
XPR = os.path.join(AQUI, '..', 'app', 'media', 'Arial_16.xpr')


def ler(caminho=XPR):
    b = open(caminho, 'rb').read()
    magia, cab, dados, n = struct.unpack('>4sIII', b[:16])
    assert magia == b'XPR2', magia
    rec = {}
    for i in range(n):
        tipo, desl, tam, nome_desl = struct.unpack('>4sIII', b[16 + 16 * i:32 + 16 * i])
        nome = b[12 + nome_desl:].split(b'\0', 1)[0].decode()
        rec[nome] = (tipo.decode(), 12 + desl, tam)
    base_dados = 12 + cab

    # textura: a constante de busca são 6 DWORDs a partir do byte 28 do cabeçalho D3D
    _, desl, _ = rec['FontTexture']
    f = struct.unpack('>6I', b[desl + 28:desl + 52])
    formato = f[1] & 0x3F
    endian = (f[1] >> 6) & 3
    tiled = f[0] >> 31
    passo = ((f[0] >> 22) & 0x1FF) * 32
    largura = (f[2] & 0x1FFF) + 1
    altura = ((f[2] >> 13) & 0x1FFF) + 1
    base = (f[1] >> 12) << 12
    assert formato == 15, 'esperava 4_4_4_4, veio %d' % formato   # GPUTEXTUREFORMAT_4_4_4_4
    assert not tiled, 'textura com tiling: falta desfazer'
    texels = b[base_dados + base:base_dados + base + passo * altura * 2]
    # O endian da constante (1, GPUENDIAN_8IN16) é para a GPU, que lê o .xpr como está
    # na memória do console; no arquivo cada texel já é um WORD big-endian de A4R4G4B4.
    # Trocar os bytes aqui pôs cada glifo no canal do vizinho (o 'A' saía como 'C') e o
    # botão X verde em vez de azul: a prova é a comparação com a foto do console.

    # dados da fonte
    _, desl, tam = rec['FontData']
    d = b[desl:desl + tam]
    versao, alt, topo, base_pad, avanco_linha, max_glifo = struct.unpack('>IffffH', d[:22])
    assert versao == 5
    tradutor = struct.unpack('>%dH' % (max_glifo + 1), d[22:22 + 2 * (max_glifo + 1)])
    p = 22 + 2 * (max_glifo + 1)
    n_glifos, = struct.unpack('>I', d[p:p + 4]); p += 4
    glifos = [list(struct.unpack('>4H3hH', d[p + 16 * i:p + 16 * i + 16])) for i in range(n_glifos)]
    mapa = {str(c): g for c, g in enumerate(tradutor) if g}
    return {
        'largura': largura, 'altura': altura, 'passo': passo,
        'alturaFonte': alt, 'margemTopo': topo, 'margemBase': base_pad, 'avancoLinha': avanco_linha,
        'mapa': mapa, 'glifos': glifos,
        'texels': base64.b64encode(texels).decode(),
    }


def valor(a, x, y, nibble, texels):
    w = (texels[(y * a['passo'] + x) * 2] << 8) | texels[(y * a['passo'] + x) * 2 + 1]
    return (w >> (4 * nibble)) & 0xF


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--png')
    x = ap.parse_args()
    a = ler()
    if x.png:
        from PIL import Image
        t = base64.b64decode(a['texels'])
        os.makedirs(x.png, exist_ok=True)
        for k in range(4):
            im = Image.new('L', (a['largura'], a['altura']))
            im.putdata([valor(a, i % a['largura'], i // a['largura'], k, t) * 17
                        for i in range(a['largura'] * a['altura'])])
            im.save(os.path.join(x.png, 'canal%d.png' % k))
        print('glifos %d, textura %dx%d, altura %.1f' % (len(a['glifos']), a['largura'], a['altura'],
                                                         a['alturaFonte']), file=sys.stderr)
        return
    json.dump(a, sys.stdout, separators=(',', ':'))


if __name__ == '__main__':
    main()
