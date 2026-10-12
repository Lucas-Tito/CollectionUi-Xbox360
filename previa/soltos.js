// ===========================================================================
// PROPOSTA: a aba "Soltos" — NADA DISTO ESTÁ NO C++.
//
// Carregada POR CIMA da prévia, que é o espelho do console:
//
//     python3 previa/montar.py --vault ~/Downloads/vault.txt \
//             --proposta previa/soltos.js --saida previa/build/soltos.html
//
// Assim a prévia continua mostrando só o que o .xex desenha, e a proposta vive
// num arquivo cujo nome diz o que ela é. O que estiver duplicado daqui (o laço da
// grade, por exemplo) é de propósito: é código que morre quando a decisão for
// tomada, e não vale emaranhar com o que espelha o console.
//
// A IDEIA: à esquerda do "Todos" entra uma pílula que não é categoria da
// FreeStyle. Ela mostra os jogos que não estão em coleção NENHUMA, na mesma
// grade de capas de dentro de uma coleção, e dali se arquiva.
// ===========================================================================

const SOLTOS = -1;
// NO FIM do vetor, não no começo: o abasPresentes original usa ABAS_FSD[0] como
// sinônimo de "Todos", e pôr o Soltos ali fazia o Todos sumir da barra. A ordem
// da tela é montada abaixo, à mão.
const ABA_SOLTOS = { id: SOLTOS, nome: 'Soltos' };
ABAS_FSD.push(ABA_SOLTOS);

// "Em coleção" é estar numa coleção DE JOGOS. Composta não conta: o conteúdo
// dela vem das origens, então um jogo que só aparece numa junção já está na
// coleção de jogos que é origem dela.
function soltos(){
  const em = new Set();
  for (const c of cols) if (c.tipo === 'jogos') for (const id of c.ids) em.add(id);
  return porNome(JOGOS.filter(g => !em.has(g.id)));
}

// A aba existe enquanto houver o que arquivar. Some sozinha quando o último for
// arquivado -- e aí o clampAba leva de volta para Todos.
const abasOriginais = abasPresentes;
abasPresentes = function(){
  const A = abasOriginais().filter(a => a.id !== SOLTOS);
  // O Soltos entra À ESQUERDA do Todos: é o que não está em lugar nenhum, antes
  // de tudo o que está.
  const comSoltos = soltos().length ? [ABA_SOLTOS].concat(A) : A;
  // A barra aparece quando há DUAS entradas, não duas categorias: Soltos + Todos
  // já é escolha de verdade, mesmo num acervo de uma categoria só.
  return comSoltos.length < 2 ? [] : comSoltos;
};

// ---------------------------------------------------------------------------
// A ROLAGEM DA BARRA
//
// Com as 7 categorias da FreeStyle mais o Soltos, a fileira soma 989 px numa
// faixa de 980: passa por 9 e esconde UMA pílula. Neste console, que tem 6
// categorias, são 882 px e nada rola. A janela segue o foco, que é o mesmo
// SeguirFoco() da grade de jogos e da de coleções.
//
// Por pílula INTEIRA, nunca por pixel: meia pílula cortada na TV parece defeito,
// e o scissor do D3D cortaria o texto no meio de uma letra.
// ---------------------------------------------------------------------------
let primeiraAba = 0;

const ABA_X0 = MARGEM_X + CHIP_L + 16;      // depois do chip LB
const ABA_FIM = 1180 - CHIP_L - 16;         // antes do chip RB encostado na margem

function janelaDeAbas(A){
  const larg = A.map(a => medir(a.nome, 1) + ABA_PAD*2);
  const faixa = ABA_FIM - ABA_X0;
  const i = Math.max(0, A.findIndex(a => a.id === aba));

  // Quantas cabem a partir de um índice.
  const cabem = de => {
    let x = 0, n = 0;
    for (let j = de; j < A.length; j++){
      const novo = x + larg[j] + (n ? ABA_GAP : 0);
      if (novo > faixa) break;
      x = novo; n++;
    }
    return n;
  };

  if (primeiraAba > i) primeiraAba = i;              // saiu pela esquerda
  while (i >= primeiraAba + cabem(primeiraAba)) primeiraAba++;   // saiu pela direita
  while (primeiraAba > 0 && cabem(primeiraAba - 1) > i - primeiraAba + 1) primeiraAba--;

  return { larg, faixa, de: primeiraAba, ate: primeiraAba + cabem(primeiraAba) };
}

desenharAbas = function(){
  const A = abasPresentes();
  if (A.length < 2) return;

  const j = janelaDeAbas(A);
  let x = ABA_X0;
  const pilulas = [];
  for (let k = j.de; k < j.ate; k++){
    pilulas.push({ x, l: j.larg[k], aba: A[k].id, nome: A[k].nome });
    x += j.larg[k] + ABA_GAP;
  }
  // O chip RB fica colado na última pílula VISÍVEL, como hoje -- e quando a
  // fileira é cortada, isso é a própria margem direita.
  const xRB = x - ABA_GAP + 16;

  // Chip apagado na ponta: LB no primeiro e RB no último não fazem nada, e a
  // tela tem de dizer isso antes de a pessoa apertar. Vale hoje também, sem o
  // Soltos: na aba Todos o LB já não faz nada e nada avisa.
  chipAba(MARGEM_X, ABA_Y + 5, 'LB', j.de > 0 || aba !== A[0].id);
  chipAba(xRB,      ABA_Y + 5, 'RB', aba !== A[A.length-1].id);

  for (const p of pilulas){
    const ativa = p.aba === aba;
    if (ativa){
      caminhoArredondado(ctx, p.x, ABA_Y, p.l, ABA_ALT, 16);
      ctx.fillStyle = COR_ANEL; ctx.fill();
    }
    texto(p.nome, p.x + ABA_PAD, ABA_Y + 3, ativa ? COR_SOBRE_ANEL : COR_APAGADO, 1);
    zona(p.x, ABA_Y, p.l, ABA_ALT, 'aba', p.aba);
  }
};

function chipAba(x, y, txt, vivo){
  ctx.fillStyle = COR_PAINEL; ctx.fillRect(x, y, CHIP_L, CHIP_ALT);
  ctx.strokeStyle = COR_LINHA; ctx.lineWidth = 1;
  ctx.strokeRect(x + 0.5, y + 0.5, CHIP_L - 1, CHIP_ALT - 1);
  texto(txt, x + CHIP_L/2, ABA_Y + 7, vivo ? COR_FRACO : 'rgb(36,44,39)', 0.72, 'center');
}

// ---------------------------------------------------------------------------
// A TELA DOS SOLTOS
//
// É a grade de dentro de uma coleção, com a mesma geometria. SEM CABEÇALHO: a
// barra ocupa o alto, e a pílula acesa já é o título -- é a mesma razão de a
// grade de coleções não ter cabeçalho. A contagem vai no rodapé, como lá.
// ---------------------------------------------------------------------------
let telaSoltos = false;      // estamos na grade dos soltos?
let alvo = null;             // o jogo que está sendo arquivado
let escolhidas = null;       // rascunho: ids das coleções marcadas

function desenharSoltos(){
  const L = soltos();
  pintarFundo(ctx);
  desenharAbas();

  if (!L.length){
    texto('Nenhum jogo fora das coleções', 640, 320, COR_APAGADO, 1.3, 'center');
    rodape([['B','Voltar']], '');
    return;
  }

  // Vindo de outra aba o foco pode estar fora da lista nova; a janela segue ele,
  // como o SeguirFoco() do C++.
  iJogo = Math.min(iJogo, L.length - 1);
  const linhaFoco = (iJogo / COLUNAS)|0;
  if (linhaFoco < primeiraLinha) primeiraLinha = linhaFoco;
  else if (linhaFoco >= primeiraLinha + LINHAS) primeiraLinha = linhaFoco - LINHAS + 1;

  const base = primeiraLinha * COLUNAS;
  const nomes = [];

  for (let i = 0; i < POR_PAGINA; i++){
    const indice = base + i;
    if (indice >= L.length) break;
    const g = L[indice];
    const x1 = GRID_MARGEM + (i % COLUNAS) * (CAPA_L + ESPACO_X);
    const y1 = TOPO + ((i / COLUNAS)|0) * (CAPA_A + ESPACO_Y);

    const img = capa(JOGOS.indexOf(g));
    if (img) ctx.drawImage(img, x1, y1, CAPA_L, CAPA_A);
    else {
      ctx.strokeStyle = COR_FRACO; ctx.lineWidth = 1;
      ctx.strokeRect(x1+.5, y1+.5, CAPA_L-1, CAPA_A-1);
    }
    if (indice === iJogo){
      ctx.strokeStyle = COR_ANEL; ctx.lineWidth = 3;
      ctx.strokeRect(x1-4+1.5, y1-4+1.5, CAPA_L+8-3, CAPA_A+8-3);
    }
    zona(x1, y1, CAPA_L, CAPA_A + 24, 'jogo', indice);
    nomes.push({x:x1, y:y1+CAPA_A+8, indice, nome:g.nome});
  }

  for (const n of nomes)
    texto(cortar(n.nome, CAPA_L, 1), n.x, n.y,
          (n.indice === iJogo) ? COR_TEXTO : COR_APAGADO, 1);

  indiceAlfabetico(L);
  // B volta para Todos. Nas outras abas o B não faz nada, mas esta é uma TELA, e
  // sair dela por onde se sai de toda tela é mais barato que lembrar do LB.
  rodape([['A','Jogar'],['B','Voltar'],['X','Adicionar a coleções']],
         `${iJogo+1} de ${L.length}`);
}

// ---------------------------------------------------------------------------
// ESCOLHER AS COLEÇÕES, para UM jogo
//
// É a tela de escolher origens, com o mesmo vocabulário: card marcado ganha o
// anel verde, não marcado leva o véu, Start conclui. Só as coleções DE JOGOS
// aparecem: composta não recebe jogo, o conteúdo dela vem das origens.
//
// Sem cabeçalho, como a de origens -- quem diz qual jogo está sendo arquivado é
// o rodapé. (É a decisão mais discutível desta tela: ver as notas da página.)
// ---------------------------------------------------------------------------
function colecoesQueRecebem(){ return ordenadas().filter(c => c.tipo === 'jogos'); }

function desenharEscolherColecoes(){
  // A ABA NÃO VALE AQUI, pelo mesmo motivo da tela de escolher origens: o que se
  // escolhe é COLEÇÃO, e a aba corrente é justamente "Soltos" -- sem isto toda
  // coleção aparecia com a colagem vazia e "( 0 )", que é o sinal de coleção
  // vazia em todo o resto do app. É o AbaEmVigor() do C++.
  const abaAntes = aba;
  aba = 0;
  try { desenharEscolherColecoesInterno(); } finally { aba = abaAntes; }
}

function desenharEscolherColecoesInterno(){
  const L = colecoesQueRecebem();
  pintarFundo(ctx);

  if (!L.length){
    texto('Nenhuma coleção para arquivar', 640, 320, COR_APAGADO, 1.3, 'center');
    rodape([['B','Cancelar']], '');
    return;
  }

  iCol = Math.min(iCol, L.length - 1);
  seguirFocoColecao();
  const base = primeiraLinhaCol * COL_POR_LINHA;

  for (let k = 0; k < COL_POR_LINHA*COL_LINHAS; k++){
    const i = base + k;
    if (i >= L.length) break;
    const x = COL_MARGEM + (k % COL_POR_LINHA) * (COL_LADO + COL_GAP);
    const y = COL_TOPO + ((k / COL_POR_LINHA)|0) * (COL_LADO + COL_GAP);
    zona(x, y, COL_LADO, COL_LADO, 'colecao', i);

    const sel = (i === iCol), marc = escolhidas.indexOf(L[i].id) >= 0;
    pintarCard(ctx, x, y, COL_LADO, COL_LADO, sel, L[i]);
    emblema(ctx, x, y, COL_LADO, L[i].tipo);

    if (!marc){
      ctx.fillStyle = 'rgba(6,9,8,0.588)';
      ctx.fillRect(x, y, COL_LADO, COL_LADO);
    } else {
      ctx.strokeStyle = COR_ANEL; ctx.lineWidth = 2;
      ctx.strokeRect(x+1, y+1, COL_LADO-2, COL_LADO-2);
    }

    const ALT_LINHA = 28, ALT_CONT = 24, meio = x + COL_LADO/2;
    const linhas = quebrar(paraMaiusculas(L[i].nome), COL_LADO - 28, 2, 1);
    const topoTexto = y + COL_LADO*0.86 - (linhas.length*ALT_LINHA + ALT_CONT);
    const cor = sel ? COR_TEXTO : COR_APAGADO;
    linhas.forEach((ln, n) => texto(ln, meio, topoTexto + n*ALT_LINHA, cor, 1, 'center'));
    texto('( ' + conta(L[i]) + ' )', meio, topoTexto + linhas.length*ALT_LINHA,
          COR_FRACO, 1, 'center');
  }

  const n = escolhidas.length;
  rodape([['A','Marcar'],['B','Cancelar'],['START','Concluir']],
         cortar(alvo.nome, 420, 1) + ' · ' + n + (n === 1 ? ' coleção' : ' coleções'));
}

function abrirEscolherColecoes(){
  const L = soltos();
  if (!L.length) return;
  alvo = L[Math.min(iJogo, L.length-1)];
  escolhidas = [];
  iCol = 0; primeiraLinhaCol = 0;
  desenhar();
}

function concluirArquivar(){
  for (const id of escolhidas){
    const c = porId(id);
    if (c && c.ids.indexOf(alvo.id) < 0) c.ids.push(alvo.id);
  }
  salvar();
  alvo = null; escolhidas = null;
  // O jogo arquivado SAI da lista. Se era o último, a aba some e o clampAba
  // devolve para Todos -- a mesma regra das categorias.
  const L = soltos();
  iJogo = Math.min(iJogo, Math.max(0, L.length - 1));
  if (!L.length){ aba = 0; telaSoltos = false; }
  desenhar();
}

// ---------------------------------------------------------------------------
// Encaixe na prévia: quem desenha, quem responde ao botão
// ---------------------------------------------------------------------------
const desenharOriginal = desenhar;
desenhar = function(){
  if (escolhidas){ zonas = []; desenharEscolherColecoes(); return; }
  if (aba === SOLTOS && (tela === 'colecoes')){ zonas = []; clampAba(); desenharSoltos(); return; }
  desenharOriginal();
};

// O clampAba não conhece o Soltos: sem isto, arquivar o último jogo deixaria o
// foco numa aba que não existe mais.
const clampOriginal = clampAba;
clampAba = function(){
  if (aba === SOLTOS){ if (!soltos().length) aba = 0; return; }
  clampOriginal();
};

document.addEventListener('keydown', e => {
  const k = e.key;

  // Escolhendo coleções para um jogo.
  if (escolhidas){
    const L = colecoesQueRecebem();
    if      (k === 'ArrowRight') iCol = Math.min(L.length-1, iCol+1);
    else if (k === 'ArrowLeft')  iCol = Math.max(0, iCol-1);
    else if (k === 'ArrowDown')  iCol = Math.min(L.length-1, iCol+COL_POR_LINHA);
    else if (k === 'ArrowUp')    iCol = Math.max(0, iCol-COL_POR_LINHA);
    else if (k === 'Enter'){
      const c = L[iCol];
      if (c){
        const i = escolhidas.indexOf(c.id);
        i >= 0 ? escolhidas.splice(i,1) : escolhidas.push(c.id);
      }
    }
    else if (k === 'Escape' || k === 'b' || k === 'B'){ alvo = null; escolhidas = null; }
    else if (k === 'm' || k === 'M'){ concluirArquivar(); e.stopImmediatePropagation(); e.preventDefault(); return; }
    else return;
    desenhar(); e.stopImmediatePropagation(); e.preventDefault();
    return;
  }

  if (aba !== SOLTOS || tela !== 'colecoes') return;

  const L = soltos();
  if      (k === 'ArrowRight') iJogo = Math.min(L.length-1, iJogo+1);
  else if (k === 'ArrowLeft')  iJogo = Math.max(0, iJogo-1);
  else if (k === 'ArrowDown')  iJogo = Math.min(L.length-1, iJogo+COLUNAS);
  else if (k === 'ArrowUp')    iJogo = Math.max(0, iJogo-COLUNAS);
  else if (k === 'x' || k === 'X'){ abrirEscolherColecoes(); e.stopImmediatePropagation(); e.preventDefault(); return; }
  else if (k === 'Escape' || k === 'b' || k === 'B'){ aba = 0; iCol = 0; }
  else if (k === 'q' || k === 'Q' || k === 'e' || k === 'E') return;   // deixa a barra trocar
  else return;

  const linha = (iJogo / COLUNAS)|0;
  if (linha < primeiraLinha) primeiraLinha = linha;
  else if (linha >= primeiraLinha + LINHAS) primeiraLinha = linha - LINHAS + 1;
  desenhar(); e.stopImmediatePropagation(); e.preventDefault();
}, true);

// O mouse não existe no console, mas é o que torna a página utilizável para
// iterar: sem isto o clique cairia no tratador da prévia, que indexa outra lista.
cv.addEventListener('click', e => {
  if (!escolhidas && (aba !== SOLTOS || tela !== 'colecoes')) return;
  const r = cv.getBoundingClientRect();
  const x = (e.clientX - r.left) * (1280 / r.width);
  const y = (e.clientY - r.top)  * (720  / r.height);

  for (const z of zonas){
    if (x < z.x || x > z.x + z.w || y < z.y || y > z.y + z.h) continue;

    if (z.tipo === 'aba' && z.dado !== aba){ aba = z.dado; iCol = 0; iJogo = 0; primeiraLinha = 0; desenhar(); }
    else if (z.tipo === 'jogo'){ iJogo = z.dado; desenhar(); }
    else if (z.tipo === 'colecao' && escolhidas){
      if (z.dado !== iCol){ iCol = z.dado; }
      else {
        const c = colecoesQueRecebem()[iCol];
        const i = escolhidas.indexOf(c.id);
        i >= 0 ? escolhidas.splice(i,1) : escolhidas.push(c.id);
      }
      desenhar();
    }
    break;
  }
  e.stopImmediatePropagation();
}, true);

desenhar();
