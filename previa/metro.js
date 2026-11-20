// ===========================================================================
// O app na linguagem do dashboard Metro do Xbox 360 (2011 em diante). Nasceu como
// proposta e foi para o C++ em 19/11 (decisão 149, app/main.cpp, "O DESENHO METRO"):
// agora é parte do espelho e o montar.py o carrega sempre. Mudou aqui, muda lá.
//
//     python3 previa/montar.py --colecoes colecoes.txt --mock
//
// CADA TELA SEGUE A TELA EQUIVALENTE DO METRO, com as cores medidas nela (capturas
// diretas da saída de vídeo, tema padrão; ver previa/README.md):
//
//   grade de coleções  <- "my apps" e o hub de games: ladrilhos chapados, o em foco
//                         maior e sem borda, nome no pé do ladrilho; os hubs no alto
//   grade de jogos     <- "My Games": cartões quase brancos (237,236,238) com a capa
//                         e o nome em cinza-escuro (45,44,45); o em foco maior e
//                         levantado, com sombra, sem moldura; título e "11 of 42"
//                         no canto de cima à direita, em branco
//   menus              <- o diálogo "Delete Profile": véu quase preto (19,19,19),
//                         painel (209,215,216), barra de título (130,136,139) com o
//                         título em branco, opções em texto escuro (46,53,54) com
//                         separador (191,198,198), a em foco em verde (0,126,4)
//                         com texto branco, "A Select  B Back" embaixo do painel
//   escolher/arquivar  <- a marcação é a de hoje (contorno verde, véu no não marcado):
//                         o selo com ✓ do ladrilho "Yes" foi recusado em 17/11
//   fundo              <- degradê vertical do tema padrão: (80,78,84) no topo,
//                         (177,176,180) a 78% da altura e um chão (162,160,164)
//   rodapé             <- glifos dos botões e o texto em branco
//
// REGRAS QUE VALEM EM TODAS AS TELAS:
//   1. Ladrilho e cartão são retos, sem canto arredondado.
//   2. Foco em ladrilho ou cartão: ele cresce e ganha sombra. Sem moldura.
//   3. Foco em lista (menu): a linha inteira em verde, texto branco.
//   4. Marcado: o contorno verde de 2 px de hoje (155,203,60), e o não marcado o véu
//      de hoje. Nunca igual ao foco, que agora é crescer com sombra.
//   5. Título da tela e contador no canto de cima à direita; nas telas com hubs, os
//      hubs ficam à esquerda no alto e o contador continua à direita.
//   6. Texto sem a sombra do atlas (no C++: uma linha no pixel shader da ATG).
//
// Contraste: como no Metro, o texto branco do rodapé fica sobre a parte clara do
// fundo, a cerca de 2,2:1. É fiel e fica abaixo dos 4,5:1 de leitura.
// ===========================================================================

// O QUADRADO DE COLEÇÃO É O DE HOJE (pedido de 17/11: o ladrilho verde ficou feio; o
// texto na frente e o fundo do cartão de hoje são melhores). Guardados aqui, antes de
// a proposta trocar as cores, e postos de volta só enquanto o cartão é desenhado.
const HOJE = { texto: COR_TEXTO, apagado: COR_APAGADO, fraco: COR_FRACO, linha: COR_LINHA,
               anel: COR_ANEL, sombra: FONTE_SOMBRA, pintarCard, emblema };

COR_TEXTO   = 'rgb(251,250,255)';
COR_APAGADO = 'rgb(225,224,230)';
COR_FRACO   = 'rgb(200,199,205)';
COR_LINHA   = 'rgb(130,129,135)';
COR_ANEL    = 'rgb(0,126,4)';
COR_PAINEL  = 'rgb(209,215,216)';
FONTE_SOMBRA = false;

const METRO = {
  fundo: [[0, [80, 78, 84]], [0.78, [177, 176, 180]], [1, [162, 160, 164]]],
  verde: 'rgb(0,126,4)', vermelho: 'rgb(178,34,30)',
  ladrilhoVerde: 'rgb(53,160,53)',
  cartao: 'rgb(237,236,238)', cartaoTexto: 'rgb(45,44,45)', cartaoSub: 'rgb(93,92,93)',
  faixa: 'rgba(24,24,26,0.86)',
  painel: 'rgb(209,215,216)', painelBarra: 'rgb(130,136,139)', painelTexto: 'rgb(46,53,54)',
  painelLinha: 'rgb(191,198,198)', veu: 'rgba(19,19,19,0.88)',
  hubInativo: 'rgb(178,177,184)',
  CRESCE: 8,
};

// ---- fundo --------------------------------------------------------------------
let FUNDO_METRO = null;
pintarFundo = function(ctx){
  if (!FUNDO_METRO){
    FUNDO_METRO = ctx.createImageData(1280, 720);
    const d = FUNDO_METRO.data, P = METRO.fundo;
    for (let y = 0; y < 720; y++){
      const t = y / 719;
      let k = 0; while (k < P.length - 2 && t > P[k + 1][0]) k++;
      const [t0, c0] = P[k], [t1, c1] = P[k + 1], f = (t - t0) / (t1 - t0);
      const cor = [0, 1, 2].map(c => Math.round(c0[c] + (c1[c] - c0[c]) * f));
      for (let x = 0; x < 1280; x++){
        const i = (y * 1280 + x) * 4;
        d[i] = cor[0]; d[i + 1] = cor[1]; d[i + 2] = cor[2]; d[i + 3] = 255;
      }
    }
  }
  ctx.putImageData(FUNDO_METRO, 0, 0);
};

// ---- peças --------------------------------------------------------------------
// O levantado (regra 2). r: o raio do que está em cima, para a sombra acompanhar a
// curva de algo arredondado; desde 19/11 tudo na proposta é reto (r = 0).
function sombra(x, y, l, a, r){
  r = r || 0;
  ctx.fillStyle = 'rgba(0,0,0,0.12)';
  caminhoArredondado(ctx, x + 2, y + 3, l + 4, a + 4, r ? r + 2 : 0); ctx.fill();
  ctx.fillStyle = 'rgba(0,0,0,0.18)';
  caminhoArredondado(ctx, x + 4, y + 6, l, a, r); ctx.fill();
}
// Marcado (regra 4) = o contorno verde de 2 px de HOJE, na cor de hoje (155,203,60).
// Foco e marcado continuam diferentes: o foco agora é crescer com sombra.
const VERDE_HOJE = 'rgb(155,203,60)';
function marcadoHoje(x, y, l, a, r){
  ctx.strokeStyle = VERDE_HOJE; ctx.lineWidth = 2;
  caminhoArredondado(ctx, x + 1, y + 1, l - 2, a - 2, r ? r - 1 : 0); ctx.stroke();
}
function tituloDaTela(titulo, contador){              // regra 5, como "My Games / 11 of 42"
  if (titulo) texto(cortar(String(titulo), 560, 1.5), 1180, 20, COR_TEXTO, 1.5, 'right');
  if (contador) texto(contador, 1180, 60, COR_APAGADO, 0.8, 'right');
}

// ---- hubs (as abas): à esquerda, o ativo branco --------------------------------
const HUB_Y = 24, HUB_GAP = 26;
desenharAbas = function(){
  const A = abasPresentes();
  if (A.length < 2) return;
  let x = MARGEM_X;
  for (const ab of A){
    const ativa = ab.id === aba, nome = ab.nome.toLowerCase();
    texto(nome, x, HUB_Y, ativa ? COR_TEXTO : METRO.hubInativo, 1.0);
    const w = medir(nome, 1.0);
    zona(x, HUB_Y, w, 28, 'aba', ab.id);
    x += w + HUB_GAP;
  }
};
cabecalho = function(titulo, sub){ tituloDaTela(titulo, sub); };

// ---- cartão de coleção: o de hoje, igual ao console ---------------------------------
// Colagem com o tom e o véu, cantos redondos, nome em maiúsculas centrado a 86% da
// altura, "( n )" embaixo, anel verde de 3 px no foco. Nas telas de escolha, o não
// marcado leva o véu de hoje e o marcado ganha o selo da proposta (regra 4: marcado
// nunca igual ao foco).
function comAsCoresDeHoje(f){
  const agora = [COR_TEXTO, COR_APAGADO, COR_FRACO, COR_LINHA, COR_ANEL, FONTE_SOMBRA];
  COR_TEXTO = HOJE.texto; COR_APAGADO = HOJE.apagado; COR_FRACO = HOJE.fraco;
  COR_LINHA = HOJE.linha; COR_ANEL = HOJE.anel; FONTE_SOMBRA = HOJE.sombra;
  try { f(); } finally {
    [COR_TEXTO, COR_APAGADO, COR_FRACO, COR_LINHA, COR_ANEL, FONTE_SOMBRA] = agora;
  }
}

// O quadrado de hoje (colagem de três capas, tom por cima, véu embaixo do nome) no grafite
// neutro do fundo do Metro em vez do verde-escuro: o verde fica para o destaque (emblema
// de composta, marcado), como no Metro. O fundo claro do cartão de jogo foi testado e
// desfeito em 18/11.
// Pedido de 19/11: canto RETO e sombra reta, como o cartão de jogo e o resto do Metro
// (regra 1); e a colagem aparece mais só no selecionado. "vivo" (0 a 1, o progresso do
// foco) leva a colagem de 60% a 92% e tira metade do tom; o véu embaixo do nome fica,
// para o nome continuar legível.
const GRAFITE = { base: 'rgb(46,46,50)', tomDe: 'rgba(28,28,32,0.659)', tomPara: 'rgba(40,40,45,0.38)',
                  veu: [26, 26, 29], contorno: 'rgb(70,70,76)' };
function pintarCardGrafite(x, y, l, a, col, vivo, op){
  vivo = vivo || 0; op = op || {};
  const comVeu = op.veu !== false, comContorno = op.contorno !== false;
  ctx.save();
  ctx.beginPath(); ctx.rect(x, y, l, a); ctx.clip();
  ctx.fillStyle = GRAFITE.base; ctx.fillRect(x, y, l, a);
  const ids = col ? jogosDe(col).slice(0, 3).map(g => g.id) : [];
  if (ids.length){
    ctx.save();
    ctx.translate(x + l/2, y + a/2);
    ctx.rotate(CARD.giro * Math.PI / 180);
    ctx.globalAlpha = CARD.alfa + (0.92 - CARD.alfa) * vivo;
    const w = l * CARD.larg, tres = ids.map(capaPorId), prim = tres.find(Boolean);
    const h = prim ? w * (prim.height / prim.width) : w * 1.425, g = w * CARD.folga;
    const pos = [{x:-(w+g), y:-h/2}, {x:0, y:-(h+g/2)}, {x:0, y:g/2}];
    tres.forEach((img, n) => { if (img) ctx.drawImage(img, pos[n].x, pos[n].y, w, h); });
    ctx.restore();
    const t = ctx.createLinearGradient(x, y, x, y + a);
    t.addColorStop(0, GRAFITE.tomDe); t.addColorStop(1, GRAFITE.tomPara);
    ctx.globalAlpha = 1 - 0.5 * vivo;
    ctx.fillStyle = t; ctx.fillRect(x, y, l, a);
    ctx.globalAlpha = 1;
    if (comVeu){
      const [r, g2, b] = GRAFITE.veu;
      const v = ctx.createLinearGradient(0, y + a*0.42, 0, y + a);
      v.addColorStop(0, `rgba(${r},${g2},${b},0)`); v.addColorStop(1, `rgba(${r},${g2},${b},0.941)`);
      ctx.fillStyle = v; ctx.fillRect(x, y + a*0.42, l, a*0.58);
    }
  }
  ctx.restore();
  if (comContorno){
    ctx.strokeStyle = GRAFITE.contorno; ctx.lineWidth = 1;
    ctx.strokeRect(x + 0.5, y + 0.5, l - 1, a - 1);
  }
}

// O quadrado de hoje dentro do encarte branco do cartão de jogo (escolhido em 19/11 entre
// quatro estilos). O tamanho do todo é o de hoje (COL_LADO): o encarte de 6 px (o
// CARTAO_PAD do jogo) entra TIRANDO do quadrado, não somando.
// O encarte apaga junto com o resto no não selecionado (pedido de 19/11): o texto de
// hoje apagado tem ~62% do brilho do aceso (142,156,148 contra 231,237,233), e o encarte
// segue a mesma conta, (147,146,148) contra o (237,236,238) do cartão, acendendo com o
// progresso do foco.
const ENCARTE_APAGADO = [147, 146, 148], ENCARTE_ACESO = [237, 236, 238];

function desenharCartao(x, y, col, sel, escolhendo){
  const prog = progressoDoFoco('c' + col.id, sel);
  // Foco igual ao dos cartões de jogo (pedido de 17/11): cresce 8 px para cada lado e
  // levanta com sombra; sem o anel verde.
  const d = Math.round(METRO.CRESCE * prog);
  const X = x - d, Y = y - d, LADO = COL_LADO + 2*d;
  if (prog > 0.02){ ctx.save(); ctx.globalAlpha = prog; sombra(X, Y, LADO, LADO, 0); ctx.restore(); }

  const P = CARTAO_PAD, ix = X + P, iy = Y + P, il = LADO - 2*P, ia = LADO - 2*P;
  const enc = ENCARTE_APAGADO.map((c, i) => Math.round(c + (ENCARTE_ACESO[i] - c) * prog));
  ctx.fillStyle = `rgb(${enc.join(',')})`; ctx.fillRect(X, Y, LADO, LADO);

  comAsCoresDeHoje(() => {
    pintarCardGrafite(ix, iy, il, ia, col, prog, { contorno: false });
    HOJE.emblema(ctx, ix, iy, il, col.tipo);
    if (escolhendo && !marcada(col)){
      ctx.fillStyle = 'rgba(6,9,8,0.588)'; ctx.fillRect(X, Y, LADO, LADO);    // o véu de hoje
    }
    const ALT_LINHA = 28, ALT_CONT = 24, meio = ix + il/2;
    const linhas = quebrar(paraMaiusculas(col.nome), il - 28, 2, 1);
    const topoTexto = iy + ia*0.86 - (linhas.length*ALT_LINHA + ALT_CONT);
    const cor = sel ? COR_TEXTO : COR_APAGADO;
    linhas.forEach((ln, n) => texto(ln, meio, topoTexto + n*ALT_LINHA, cor, 1, 'center'));
    texto('( ' + conta(col) + ' )', meio, topoTexto + linhas.length*ALT_LINHA, COR_FRACO, 1, 'center');
  });
  if (escolhendo && marcada(col)) marcadoHoje(ix, iy, il, ia, 0);
}

telaColecoes = function(){
  if (tela === 'arquivar' && aba !== 0){
    const antes = aba; aba = 0;
    try { telaColecoes(); } finally { aba = antes; }
    return;
  }
  const arquivando = tela === 'arquivar';
  const escolhendo = tela === 'origens' || arquivando;
  const L = listaDeColecoes();
  pintarFundo(ctx);

  if (!L.length){
    texto(!escolhendo ? 'No collections yet'
        : arquivando ? 'No collections to add it to'
        : editandoComposta.tipo === 'uniao' ? 'No game collections to combine'
        :                                     'No collections to intersect',
          640, 320, COR_TEXTO, 1.3, 'center');
    rodape(escolhendo ? [['B','Cancel']] : [['X','New collection'],['BACK','Export']], '');
    return;
  }

  iCol = Math.min(iCol, L.length-1);
  seguirFocoColecao();

  if (!escolhendo){
    desenharAbas();
    tituloDaTela('', `${iCol+1} of ${L.length}`);
  } else if (arquivando){
    const q = escolhidas.length;
    tituloDaTela('add to collections', `${alvo ? cortar(alvo.nome, 400, 0.8) : ''} · ${q} ${q === 1 ? 'collection' : 'collections'}`);
  } else {
    tituloDaTela('choose collections', `${origens.length} chosen · ${contaRascunho()} games`);
  }

  desenharGrades(() => gradeColecoes(escolhendo));

  if (escolhendo) rodape([['A','Select'],['B','Cancel'],['START','Done']], '');
  else rodape([['A','Open'],['X','New collection'],['START','Options'],['BACK','Export']], '');
};

// A grade de coleções sozinha, com o estado atual (aba, foco, rolagem): a troca de hub
// a desenha duas vezes, a da aba nova e a da anterior, cada uma no seu deslocamento.
function gradeColecoes(escolhendo){
  const L = listaDeColecoes();
  if (!L.length) return;
  iCol = Math.min(iCol, L.length-1);
  seguirFocoColecao();
  const base = primeiraLinhaCol * COL_POR_LINHA;
  let foco = null;
  for (let k = 0; k < COL_POR_LINHA*COL_LINHAS; k++){
    const i = base + k;
    if (i >= L.length) break;
    const x = COL_MARGEM + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
    const y = COL_TOPO_ABA + ((k / COL_POR_LINHA)|0) * (COL_LADO + COL_GAP);
    zona(x, y, COL_LADO, COL_LADO, 'colecao', i);
    if (i === iCol){ foco = {x, y, c: L[i]}; continue; }     // o em foco por cima
    desenharCartao(x, y, L[i], false, escolhendo);
  }
  if (foco) desenharCartao(foco.x, foco.y, foco.c, true, escolhendo);
}

// ---- grade de jogos: os cartões do "My Games" -------------------------------------
const CARTAO_PAD = 6, CARTAO_NOME = 34;
telaJogos = function(){
  if (tela === 'detalhes') return telaDetalhes();
  const L = lista(), total = L.length;
  pintarFundo(ctx);
  const escolhendo = tela === 'adicionar';
  const naSoltos = tela === 'soltos';
  if (naSoltos) desenharAbas();

  if (!total){
    let vazia = 'No games in this collection';
    if (!escolhendo && atual){
      if      (atual.tipo === 'intersecao') vazia = 'No game is in all of them';
      else if (atual.tipo === 'uniao')      vazia = 'No games in the chosen collections';
    }
    if (!escolhendo && aba !== 0) vazia = `No ${nomeAba(aba)} games in this collection`;
    if (naSoltos) vazia = 'No games outside your collections';
    texto(escolhendo ? 'The library is empty' : vazia, 640, 320, COR_TEXTO, 1.3, 'center');
    rodape(naSoltos ? [['B','Back']] : escolhendo ? [['B','Cancel']]
         : composta(atual) ? [['B','Back'],['START','Options']] : [['B','Back'],['X','Add games']], '');
    return;
  }

  // "show me / all games" do My Games: a aba que filtra, no alto à esquerda.
  // Só quando filtra: no Todos, que é o padrão, não diz nada (pedido de 18/11).
  if (!naSoltos && aba !== 0){
    texto('showing', MARGEM_X, 18, COR_APAGADO, 0.8);
    texto(nomeAba(aba).toLowerCase(), MARGEM_X, 38, COR_TEXTO, 1.0);
  }
  if (escolhendo) tituloDaTela('add games', `${selecao.length} of ${JOGOS.length} selected`);
  else if (naSoltos) tituloDaTela('', `${iJogo+1} of ${total}`);
  else tituloDaTela(atual.nome, `${iJogo+1} of ${total}`);

  desenharGrades(() => gradeJogos(escolhendo));

  if (escolhendo) rodape([['A','Select'],['B','Cancel'],['START','Done']], '');
  else if (naSoltos) rodape([['A','Play'],['Y','Details'],['B','Back'],['X','Add to collections']], '');
  else if (composta(atual)) rodape([['A','Play'],['Y','Details'],['B','Back'],['START','Options']], '');
  else rodape([['A','Play'],['Y','Details'],['B','Back'],['X','Add games'],['START','Options']], '');
};

function gradeJogos(escolhendo){
  const L = lista(), total = L.length;
  if (!total) return;
  const base = primeiraLinha * COLUNAS;
  const tem = new Set(escolhendo ? selecao : []);
  let foco = null;

  const cartao = (g, x1, y1, sel) => {
    const prog = progressoDoFoco('g' + g.id, sel);
    const d = Math.round(METRO.CRESCE * prog);
    const cx = x1 - CARTAO_PAD - d, cy = y1 - CARTAO_PAD - d;
    const cl = CAPA_L + 2*CARTAO_PAD + 2*d, ca = CAPA_A + 2*CARTAO_PAD + CARTAO_NOME + 2*d;
    if (prog > 0.02){ ctx.save(); ctx.globalAlpha = prog; sombra(cx, cy, cl, ca); ctx.restore(); }
    ctx.fillStyle = METRO.cartao; ctx.fillRect(cx, cy, cl, ca);
    const img = capa(JOGOS.indexOf(g));
    const ix = x1 - d, iy = y1 - d, il = CAPA_L + 2*d, ia = CAPA_A + 2*d;
    if (img) ctx.drawImage(img, ix, iy, il, ia);
    else { ctx.fillStyle = 'rgb(120,119,124)'; ctx.fillRect(ix, iy, il, ia); }
    texto(cortar(g.nome, il, 0.9), ix, iy + ia + 6, METRO.cartaoTexto, 0.9);
    if (escolhendo){
      // Como hoje: o não marcado leva o véu ARGB(140,6,9,8) na capa, o marcado o
      // contorno verde de 2 px.
      if (tem.has(g.id)) marcadoHoje(ix, iy, il, ia, 0);
      else { ctx.fillStyle = 'rgba(6,9,8,0.549)'; ctx.fillRect(ix, iy, il, ia); }
    }
  };

  for (let i = 0; i < POR_PAGINA; i++){
    const indice = base + i;
    if (indice >= total) break;
    const g = L[indice];
    const x1 = GRID_MARGEM + (i % COLUNAS) * (CAPA_L + ESPACO_X);
    const y1 = TOPO + 4 + ((i / COLUNAS)|0) * (CAPA_A + ESPACO_Y);
    zona(x1, y1, CAPA_L, CAPA_A + 24, 'jogo', indice);
    if (indice === iJogo) foco = {g, x1, y1}; else cartao(g, x1, y1, false);
  }
  if (foco) cartao(foco.g, foco.x1, foco.y1, true);

  indiceAlfabetico(L);

}

indiceAlfabetico = function(L){
  const letras = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ#'.split('');
  const presentes = new Set(L.map(g => inicial(g.nome)));
  const atualL = inicial(L[iJogo].nome);
  const passo = (640 - 92) / letras.length, ESCALA = 0.72, ALT_LETRA = ALT_GLIFO * ESCALA;
  for (let i = 0; i < letras.length; i++){
    const y = 92 + i * passo, acesa = letras[i] === atualL;
    if (presentes.has(letras[i])) zona(1204, y, 28, passo, 'letra', letras[i]);
    if (acesa){ ctx.fillStyle = METRO.verde; ctx.fillRect(1204, y, 28, passo); }
    texto(letras[i], 1218, y + (passo - ALT_LETRA)/2,
          acesa ? COR_TEXTO : (presentes.has(letras[i]) ? COR_TEXTO : COR_LINHA), ESCALA, 'center');
  }
};

// ---- menu: o diálogo "Delete Profile" do Metro, medido em 720p ---------------------
// Painel de 548 px centrado, de y 45 a 618, altura FIXA (sobra vazio entre o texto e as
// opções, como lá); barra de 47 px em degradê (146,150,152) -> (130,136,139) com o título
// em branco; corpo em degradê (231,234,233) -> (206,210,211) com o texto escuro no alto;
// opções encostadas embaixo, linhas de 40 px com separador, a em foco verde (0,126,4);
// véu quase preto atrás; "A Select  B Back" logo abaixo, alinhado ao painel.
// O texto do corpo não existe no C++ de hoje (o menu só tem rótulo, nome e opções):
// é uma frase por menu, nova.
const DIALOGO = { x: 366, l: 548, topo: 45, barra: 47, fim: 618, linha: 40 };
// Barra de título própria e corpo só com a frase, sem a linha do nome: no exportar a
// barra já diz o que é ("Export to Xbox Vault") e o nome ("Biblioteca e coleções
// para o Xbox Vault") repetia (pedido de 18/11).
const BARRA = { exportar: 'Export to Xbox Vault' };
const SEM_NOME = { exportar: true };
const FRASE = {
  colecao:  'What do you want to do with this collection?',
  jogo:     'What do you want to do with this game in this collection?',
  exportar: 'Saves your collections to vault.txt',
  tipo:     'What kind of collection will it be?',
  apagar:   'Delete this collection? The games stay on the console; only the collection goes away.',
};
function degrade(x, y, l, a, de, para){
  const g = ctx.createLinearGradient(0, y, 0, y + a);
  g.addColorStop(0, de); g.addColorStop(1, para);
  ctx.fillStyle = g; ctx.fillRect(x, y, l, a);
}
desenharMenu = function(){
  const m = menu, D = DIALOGO;
  ctx.fillStyle = 'rgba(8,8,8,0.9)'; ctx.fillRect(0, 0, 1280, 720);
  degrade(D.x, D.topo, D.l, D.barra, 'rgb(146,150,152)', 'rgb(130,136,139)');
  let tx = D.x + 14;
  if (m.de === 'apagar'){
    // o "!" amarelo da barra do Delete Profile, só no que destrói
    ctx.fillStyle = 'rgb(249,190,55)';
    ctx.beginPath(); ctx.arc(D.x + 22, D.topo + D.barra/2, 11, 0, Math.PI*2); ctx.fill();
    texto('!', D.x + 22, D.topo + D.barra/2 - 11, 'rgb(60,50,20)', 0.85, 'center');
    tx = D.x + 42;
  }
  texto(BARRA[m.de] || m.rotulo, tx, D.topo + (D.barra - ALT_GLIFO*0.95)/2, COR_TEXTO, 0.95);

  degrade(D.x, D.topo + D.barra, D.l, D.fim - D.topo - D.barra, 'rgb(231,234,233)', 'rgb(206,210,211)');
  let ty = D.topo + D.barra + 16;
  if (!SEM_NOME[m.de]){
    for (const ln of quebrar(m.titulo, D.l - 32, 2, 1)){ texto(ln, D.x + 16, ty, METRO.painelTexto, 1); ty += 28; }
    ty += 12;
  }
  for (const ln of quebrar(FRASE[m.de] || '', D.l - 32, 4, 0.9)){ texto(ln, D.x + 16, ty, METRO.painelTexto, 0.9); ty += 25; }

  const n = m.itens.length;
  for (let i = 0; i < n; i++){
    const ly = D.fim - (n - i) * D.linha;
    const mau = (i === m.perigo), sel = (i === m.foco);
    ctx.fillStyle = 'rgb(201,207,208)'; ctx.fillRect(D.x, ly, D.l, D.linha);
    ctx.fillStyle = 'rgb(186,192,193)'; ctx.fillRect(D.x, ly, D.l, 1);
    if (sel){ ctx.fillStyle = mau ? METRO.vermelho : METRO.verde; ctx.fillRect(D.x, ly, D.l, D.linha); }
    texto(cortar(m.itens[i], D.l - 32, 0.95), D.x + 16, ly + (D.linha - ALT_GLIFO*0.95)/2,
          sel ? COR_TEXTO : METRO.painelTexto, 0.95);
    zona(D.x, ly, D.l, D.linha, 'menu', i);
  }
  texto(linhaDeBotoes(RODAPE_MODAL), D.x + 4, D.fim + 10, COR_TEXTO, 0.8);
};

// ---- rodapé: botões e texto em branco, sem contador (ele vai para o alto) -----------
rodape = function(itens, direita){
  texto(linhaDeBotoes(itens), MARGEM_X, RODAPE_Y, COR_TEXTO, RODAPE_ESCALA);
  if (direita) texto(direita, 1180, RODAPE_Y, COR_TEXTO, RODAPE_ESCALA, 'right');
};


// ===========================================================================
// ANIMAÇÕES, medidas num vídeo de captura direta do Metro (a prévia do dashboard
// 2.0.14448, de 2011, no Internet Archive: "xbox-360-14448-dash-preview-out-in-
// november"), quadro a quadro:
//   FOCO   o ladrilho novo cresce ~10 px para cada lado em ~120 ms, desacelerando, e
//          o antigo encolhe ao mesmo tempo. Aqui: exponencial com constante de 40 ms.
//   HUB    o conteúdo desliza uma página para o lado; a distância que falta cai a
//          ~63% a cada 100 ms (constante de ~220 ms), 95% feito em ~0,65 s e parado
//          perto de 1 s. O nome do hub acende logo no começo.
//   TELA   abrir uma página (ou o menu) é um esmaecimento cruzado de ~200 ms.
// No C++ as três são aritmética por quadro com o tempo desde a troca (o app já
// redesenha a cada quadro); o esmaecimento cruzado usa o quadro anterior resolvido
// numa textura, o mesmo Resolve que a foto do roteiro já faz.
// ===========================================================================
const ANIM = { tauFoco: 40, tauHub: 220, fade: 200, pagina: 768 };
const agoraMs = () => performance.now();

// --- foco -------------------------------------------------------------------
// O em foco é desenhado por último; o que acabou de perder o foco aparece antes, ainda
// marcado como atual, e é aí que a troca começa.
let focoAtual = null, focoAntes = null, focoT0 = 0;
function progressoDoFoco(chave, sel){
  if (ESTATICO) return sel ? 1 : 0;
  if (!sel && chave === focoAtual){ focoAntes = chave; focoAtual = null; focoT0 = agoraMs(); }
  if (sel && chave !== focoAtual){
    if (focoAtual) focoAntes = focoAtual;
    focoAtual = chave; focoT0 = agoraMs();
  }
  const p = 1 - Math.exp(-(agoraMs() - focoT0) / ANIM.tauFoco);
  if (sel) return p;
  return chave === focoAntes ? 1 - p : 0;
}

// --- hub: as duas páginas ao mesmo tempo -------------------------------------
// A da aba nova entra de um lado enquanto a anterior sai pelo outro, como no Metro:
// as duas grades são desenhadas no mesmo quadro, a anterior com o estado que ela tinha
// (aba, foco, rolagem), parada no foco que estava. O fundo, os hubs, o título e o
// rodapé ficam fixos. A distância é a tela inteira (1280): no Metro o conteúdo de um
// hub é mais estreito e 768 bastavam; aqui a grade ocupa quase a largura toda, e com
// menos que isso a página anterior terminaria ainda visível à esquerda.
let hubT0 = -1e9, hubDir = 0, estadoAntes = null, ESTATICO = false;
const irParaAbaSemAnim = irParaAba;
irParaAba = function(id){
  const A = abasPresentes().map(a => a.id);
  const de = A.indexOf(aba), para = A.indexOf(id);
  const foto = { tela, aba, iCol, primeiraLinhaCol, iJogo, primeiraLinha };
  irParaAbaSemAnim(id);
  if (de >= 0 && para >= 0 && de !== para){
    hubDir = para > de ? 1 : -1; hubT0 = agoraMs(); estadoAntes = foto;
  }
};
const emTrocaDeHub = () => agoraMs() - hubT0 < 5 * ANIM.tauHub && estadoAntes;
function deslocamentoDoHub(){
  return hubDir * 1280 * Math.exp(-(agoraMs() - hubT0) / ANIM.tauHub);
}
function comDeslocamento(dx, f){
  if (!dx) return f();
  ctx.save(); ctx.translate(dx, 0);
  try { f(); } finally { ctx.restore(); }
}
function desenharGrades(gradeNova){
  if (!emTrocaDeHub() || (tela !== 'colecoes' && tela !== 'soltos')) return gradeNova();
  const dx = Math.round(deslocamentoDoHub());
  comDeslocamento(dx, gradeNova);
  // A anterior, um passo de página atrás.
  const agora = { tela, aba, iCol, primeiraLinhaCol, iJogo, primeiraLinha };
  const nZonas = zonas.length;
  ({ tela, aba, iCol, primeiraLinhaCol, iJogo, primeiraLinha } = estadoAntes);
  ESTATICO = true;
  try {
    comDeslocamento(dx - hubDir * 1280, () => tela === 'soltos' ? gradeJogos(false) : gradeColecoes(false));
  } finally {
    ({ tela, aba, iCol, primeiraLinhaCol, iJogo, primeiraLinha } = agora);
    ESTATICO = false;
    zonas.length = nZonas;      // a página que sai não recebe clique
  }
}

// --- tela e menu: esmaecimento cruzado ---------------------------------------
const antes = document.createElement('canvas'); antes.width = 1280; antes.height = 720;
let fadeT0 = -1e9, ultimaTela = null, ultimoMenu = null;
const desenharSemAnim = desenhar;
desenhar = function(){
  // Coleções <-> Soltos é troca de HUB (desliza), não de tela: sem esmaecimento.
  // Os detalhes também não: eles têm a animação própria, a faixa subindo.
  const trocaDeHub = (tela === 'soltos' && ultimaTela === 'colecoes') || (tela === 'colecoes' && ultimaTela === 'soltos');
  const semFade = trocaDeHub || tela === 'detalhes' || ultimaTela === 'detalhes';
  const mudou = (ultimaTela !== null) && ((tela !== ultimaTela && !semFade) || !!menu !== ultimoMenu);
  if (mudou){ antes.getContext('2d').drawImage(cv, 0, 0); fadeT0 = agoraMs(); }
  ultimaTela = tela; ultimoMenu = !!menu;
  desenharSemAnim();
  const tf = agoraMs() - fadeT0;
  if (tf < ANIM.fade){
    ctx.save(); ctx.globalAlpha = 1 - tf / ANIM.fade; ctx.drawImage(antes, 0, 0); ctx.restore();
  }
  // Enquanto algo anima, redesenha no próximo quadro.
  const vivo = tf < ANIM.fade || agoraMs() - focoT0 < 6 * ANIM.tauFoco || agoraMs() - hubT0 < 5 * ANIM.tauHub;
  if (vivo && !pedido) pedido = requestAnimationFrame(() => { pedido = 0; desenhar(); });
};
let pedido = 0;


// ===========================================================================
// DETALHES DO JOGO, NUMA FAIXA QUE SOBE DE BAIXO (escolhida em 19/11 entre cinco formatos
// de popup; o desenho da tela dedicada estava bom, mas os dados não justificavam uma tela
// inteira). Abre com Y na grade de jogos e no Soltos, por cima da grade; B fecha; o A da
// grade continua lançando direto. O Play fica só no rodapé, sem botão verde.
// Animação: a faixa sobe (e desce ao fechar) com o decaimento exponencial do Metro, e o
// véu de cima acompanha; sem o esmaecimento cruzado das outras trocas de tela.
// Dados: previa/ficha.py. Co-op só do Co-Optimus: sem ele, nada.
// ===========================================================================
let jogoDetalhe = null, telaAntesDetalhes = null;
const FAIXA_TAU = 70;                 // ms; 95% do caminho em ~210 ms
let faixaT0 = -1e9, faixaFechando = false, pedidoFaixa = 0;
const horas = h => h < 1 ? Math.round(h * 60) + ' min' : (Math.round(h * 2) / 2).toString() + ' h';
function corMetacritic(n){ return n >= 75 ? 'rgb(102,204,51)' : n >= 50 ? 'rgb(255,204,51)' : 'rgb(255,0,0)'; }

// As cores do corpo do diálogo do Metro (a faixa clara escolhida em 18/11).
const PAINEL = { de: 'rgb(231,234,233)', para: 'rgb(206,210,211)', texto: 'rgb(46,53,54)',
                 rotulo: 'rgb(104,112,114)', estrela: 'rgb(0,126,4)' };
const rotulo = (t, x, y) => texto(t, x, y, PAINEL.rotulo, 0.7);
function linhasEm(t, x, y, l, max, e, cor){
  for (const ln of quebrar(String(t), l, max, e)){ texto(ln, x, y, cor || PAINEL.texto, e); y += Math.round(29 * e); }
  return y;
}
function capaEm(g, x, y, l, moldura){
  const a = Math.round(l * CAPA_A / CAPA_L);
  if (moldura){ ctx.fillStyle = METRO.cartao; ctx.fillRect(x - moldura, y - moldura, l + 2*moldura, a + 2*moldura); }
  const img = capa(JOGOS.indexOf(g));
  if (img) ctx.drawImage(img, x, y, l, a); else { ctx.fillStyle = 'rgb(120,119,124)'; ctx.fillRect(x, y, l, a); }
  return a;
}
// O rodapé num painel claro: na faixa da barra do diálogo, com texto branco, porque os
// glifos dos botões pegam a cor do texto e sumiriam em escuro.
function rodapeNoPainel(x, l, itens, xTexto){
  degrade(x, 676, l, 44, 'rgb(146,150,152)', 'rgb(130,136,139)');
  texto(linhaDeBotoes(itens), xTexto != null ? xTexto : x + 32, 676 + (44 - ALT_GLIFO*0.75)/2, COR_TEXTO, 0.75);
}
const nomeLimpo = g => g.nome.replace(/[™®]/g, '');

// O que existe para mostrar, já com rótulo e texto; quem desenha escolhe onde.
function dadosDoJogo(g){
  const f = g.ficha || {};
  const d = { f, genero: f.genero ? f.genero.toLowerCase() : '' };
  d.coop = f.coop ? (f.coop > 1 ? `up to ${f.coop} players` : '1 player') : '';
  d.coopExtras = f.coop && f.coopExtras ? f.coopExtras.join('  ·  ') : '';
  d.coopTexto = f.coop ? (f.coopTexto || '') : '';
  d.ficha = [];
  if (f.dev) d.ficha.push(['DEVELOPER', f.dev]);
  if (f.pub) d.ficha.push(['PUBLISHER', f.pub]);
  d.tempo = [];
  if (f.tempo){
    if (f.tempo.main) d.tempo.push(['main story', horas(f.tempo.main)]);
    if (f.tempo.plus) d.tempo.push(['main + extras', horas(f.tempo.plus)]);
    if (f.tempo.cem)  d.tempo.push(['100%', horas(f.tempo.cem)]);
  }
  d.tempoLinha = d.tempo.map(([n, h]) => n + ' ' + h).join('  ·  ');
  return d;
}
// A nota: o quadrado do Metacritic ou as estrelas dos jogadores. Devolve o y de baixo.
function nota(f, x, y, e){
  e = e || 0.9;
  if (f.metacritic){
    ctx.fillStyle = corMetacritic(f.metacritic); ctx.fillRect(x, y, Math.round(44 * e), Math.round(32 * e));
    texto(String(f.metacritic), x + Math.round(22 * e), y + 2, f.metacritic < 50 ? COR_TEXTO : 'rgb(20,20,20)', e, 'center');
    return y + Math.round(34 * e);
  }
  if (f.jogadores_nota){
    const cheias = Math.round(f.jogadores_nota); let sx = x;
    for (let i = 0; i < 5; i++){ texto(String.fromCharCode(0x145), sx, y, i < cheias ? PAINEL.estrela : PAINEL.rotulo, e); sx += Math.round(24 * e); }
    texto(String(f.jogadores_nota) + ' of 5', sx + 6, y, PAINEL.texto, e);
    return y + Math.round(28 * e);
  }
  return y;
}
const rotuloDaNota = f => f.metacritic ? 'METACRITIC' : f.jogadores_nota ? 'USER RATING' : '';

// Um campo empilhado: rótulo pequeno e o valor. Devolve o y de baixo.
function campo(r, desenha, x, y){ rotulo(r, x, y); return desenha(y + 20) + 12; }
function tempoEmLinhas(d, x, y, colL){
  for (const [n, h] of d.tempo){ texto(n, x, y, PAINEL.texto, 0.82); texto(h, x + colL, y, PAINEL.texto, 0.82); y += 23; }
  return y;
}

// ---- a faixa: capa saindo da borda, e o resto num bloco centrado -----------------------
// k: 0 = no lugar, 1 = escondida embaixo.
function faixaDetalhes(g, d, k){
  const FY = 300, SOBE = 720 - (FY - 60);
  ctx.fillStyle = `rgba(8,8,8,${0.6 * (1 - k)})`; ctx.fillRect(0, 0, 1280, 720);
  ctx.save();
  ctx.translate(0, Math.round(SOBE * k));
  ctx.fillStyle = 'rgba(0,0,0,0.25)'; ctx.fillRect(0, FY - 6, 1280, 6);
  degrade(0, FY, 1280, 720 - FY, PAINEL.de, PAINEL.para);

  // o bloco: capa (150) + 36 + meio (470) + 40 + ficha (300), centrado
  const CL = 150, MW = 470, RWf = 300, LARG = CL + 36 + MW + 40 + RWf;
  const CX = Math.round((1280 - LARG) / 2), CY = FY - 60;
  sombra(CX - 6, CY - 6, CL + 12, Math.round(CL * CAPA_A / CAPA_L) + 12, 0);
  const ca = capaEm(g, CX, CY, CL, 6);

  const NX = CX + CL + 36, NW = LARG - CL - 36;
  let y = FY + 18;
  if (d.genero){ texto(cortar(d.genero, NW, 0.8), NX, y, PAINEL.rotulo, 0.8); y += 26; }
  texto(cortar(nomeLimpo(g), NW, 1.35), NX, y, PAINEL.texto, 1.35);

  // sob a capa, os números; no meio, co-op e sobre; à direita, a ficha
  let ny = CY + ca + 24;
  if (d.coop){ rotulo('COUCH CO-OP', CX, ny); linhasEm(d.coop, CX, ny + 20, CL + 10, 2, 0.9); ny += 74; }
  if (d.f.ano){ rotulo('RELEASED', CX, ny); texto(String(d.f.ano), CX, ny + 20, PAINEL.texto, 0.9); }

  const MX = NX;
  let my = FY + 100;
  if (d.coop && (d.coopExtras || d.coopTexto)){
    rotulo('ON CO-OPTIMUS', MX, my); my += 20;
    if (d.coopExtras) my = linhasEm(d.coopExtras, MX, my, MW, 2, 0.76);
    if (d.coopTexto) my = linhasEm(d.coopTexto, MX, my, MW, 2, 0.78);
    my += 12;
  }
  if (d.f.descricao){
    const cabe = Math.floor((662 - my - 20) / 23);
    if (cabe >= 2){ rotulo('ABOUT', MX, my); my += 20; linhasEm(d.f.descricao, MX, my, MW, cabe, 0.78); }
  }
  const RX = MX + MW + 40, RW = RWf;
  let ry = FY + 100;
  for (const [r, v] of d.ficha) ry = campo(r, yy => linhasEm(v, RX, yy, RW, 1, 0.82), RX, ry);
  if (rotuloDaNota(d.f)) ry = campo(rotuloDaNota(d.f), yy => nota(d.f, RX, yy, 0.82), RX, ry);
  if (d.tempo.length) ry = campo('HOW LONG TO BEAT', yy => tempoEmLinhas(d, RX, yy, 120), RX, ry);
  rodapeNoPainel(0, 1280, [['A','Play'],['B','Back']], CX);
  ctx.restore();
}

function telaDetalhes(){
  // por baixo, a grade de onde ele veio; por cima, a faixa
  const embaixo = tela; tela = telaAntesDetalhes;
  try { telaJogos(); } finally { tela = embaixo; }
  const t = agoraMs() - faixaT0;
  let k = Math.exp(-t / FAIXA_TAU);
  if (faixaFechando) k = 1 - k;
  const anda = t < 5 * FAIXA_TAU;
  if (!anda) k = faixaFechando ? 1 : 0;
  faixaDetalhes(jogoDetalhe, dadosDoJogo(jogoDetalhe), k);
  if (anda){
    if (!pedidoFaixa) pedidoFaixa = requestAnimationFrame(() => { pedidoFaixa = 0; desenhar(); });
  } else if (faixaFechando){
    faixaFechando = false; tela = telaAntesDetalhes; jogoDetalhe = null;
    requestAnimationFrame(() => desenhar());
  }
}

document.addEventListener('keydown', e => {
  const k = e.key;
  if (menu) return;
  if (tela === 'detalhes'){
    if ((k === 'Escape' || k === 'b' || k === 'B') && !faixaFechando){
      faixaFechando = true; faixaT0 = agoraMs(); desenhar();
    }
    e.stopImmediatePropagation(); e.preventDefault();
    return;
  }
  if ((k === 'y' || k === 'Y') && (tela === 'jogos' || tela === 'soltos')){
    const L = lista();
    if (!L[iJogo]) return;
    jogoDetalhe = L[iJogo]; telaAntesDetalhes = tela; tela = 'detalhes';
    faixaFechando = false; faixaT0 = agoraMs();
    desenhar(); e.stopImmediatePropagation(); e.preventDefault();
    return;
  }
}, true);
