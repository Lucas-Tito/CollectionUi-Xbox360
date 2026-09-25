/* VFS do SQLite para Xbox 360, somente leitura.
 *
 * O SQLite e compilado com SQLITE_OS_OTHER=1: nenhuma camada de sistema vem junto,
 * porque nem a do Windows serve. O console tem um subconjunto da API Win32 (CreateFile,
 * ReadFile, SetFilePointer), e nao tem POSIX nenhum.
 *
 * Somente leitura de proposito: o banco e do FreeStyle, nao nosso. Escrever nele seria
 * mexer no estado da dashboard alheia. Toda operacao de escrita devolve SQLITE_READONLY
 * em vez de falhar silenciosamente.
 */

#include "sqlite3.h"
#include <xtl.h>
#include <string.h>

/* O xtl.h do XDK nao traz todas as constantes do Win32 de PC. */
#ifndef INVALID_FILE_ATTRIBUTES
#  define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif
#ifndef INVALID_SET_FILE_POINTER
#  define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif

#define SETOR 512

typedef struct ArquivoXbox
{
    sqlite3_file base;
    HANDLE       handle;
} ArquivoXbox;

static int xbFechar(sqlite3_file *arquivo)
{
    ArquivoXbox *a = (ArquivoXbox *)arquivo;
    if (a->handle != INVALID_HANDLE_VALUE)
        CloseHandle(a->handle);
    a->handle = INVALID_HANDLE_VALUE;
    return SQLITE_OK;
}

static int xbLer(sqlite3_file *arquivo, void *destino, int quantos, sqlite3_int64 posicao)
{
    ArquivoXbox *a = (ArquivoXbox *)arquivo;
    LONG  alto = (LONG)(posicao >> 32);
    DWORD baixo;
    DWORD lidos = 0;

    baixo = SetFilePointer(a->handle, (LONG)(posicao & 0xFFFFFFFF), &alto, FILE_BEGIN);
    if (baixo == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return SQLITE_IOERR_READ;

    if (!ReadFile(a->handle, destino, (DWORD)quantos, &lidos, NULL))
        return SQLITE_IOERR_READ;

    if (lidos < (DWORD)quantos)
    {
        /* O SQLite exige o resto zerado, e nao so o aviso de leitura curta. */
        memset((char *)destino + lidos, 0, (size_t)quantos - lidos);
        return SQLITE_IOERR_SHORT_READ;
    }
    return SQLITE_OK;
}

static int xbEscrever(sqlite3_file *a, const void *b, int c, sqlite3_int64 d)
{
    (void)a; (void)b; (void)c; (void)d;
    return SQLITE_READONLY;
}

static int xbTruncar(sqlite3_file *a, sqlite3_int64 b) { (void)a; (void)b; return SQLITE_READONLY; }
static int xbSincronizar(sqlite3_file *a, int b)       { (void)a; (void)b; return SQLITE_OK; }

static int xbTamanho(sqlite3_file *arquivo, sqlite3_int64 *tamanho)
{
    ArquivoXbox *a = (ArquivoXbox *)arquivo;
    DWORD alto = 0;
    DWORD baixo = GetFileSize(a->handle, &alto);

    if (baixo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
        return SQLITE_IOERR_FSTAT;

    *tamanho = ((sqlite3_int64)alto << 32) | baixo;
    return SQLITE_OK;
}

/* Sem trava: o arquivo e aberto so para leitura e ninguem mais escreve nele enquanto
 * o nosso app roda (o FreeStyle nao esta em execucao, foi ele que nos lancou). */
static int xbTravar(sqlite3_file *a, int b)       { (void)a; (void)b; return SQLITE_OK; }
static int xbDestravar(sqlite3_file *a, int b)    { (void)a; (void)b; return SQLITE_OK; }
static int xbChecarTrava(sqlite3_file *a, int *r) { (void)a; *r = 0; return SQLITE_OK; }
static int xbControle(sqlite3_file *a, int b, void *c) { (void)a; (void)b; (void)c; return SQLITE_NOTFOUND; }
static int xbSetor(sqlite3_file *a)               { (void)a; return SETOR; }
static int xbCaracteristicas(sqlite3_file *a)     { (void)a; return 0; }

static const sqlite3_io_methods METODOS = {
    1,                  /* iVersion */
    xbFechar, xbLer, xbEscrever, xbTruncar, xbSincronizar, xbTamanho,
    xbTravar, xbDestravar, xbChecarTrava, xbControle,
    xbSetor, xbCaracteristicas,
    0, 0, 0, 0, 0, 0    /* metodos de versao 2 e 3, nao usados */
};

static int xbAbrir(sqlite3_vfs *vfs, const char *nome, sqlite3_file *arquivo, int flags, int *saida)
{
    ArquivoXbox *a = (ArquivoXbox *)arquivo;
    (void)vfs;

    memset(a, 0, sizeof(*a));
    a->handle = INVALID_HANDLE_VALUE;

    /* Sem nome e arquivo temporario, que exigiria escrita. */
    if (nome == NULL)
        return SQLITE_READONLY;

    a->handle = CreateFile(nome, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (a->handle == INVALID_HANDLE_VALUE)
        return SQLITE_CANTOPEN;

    a->base.pMethods = &METODOS;
    if (saida != NULL)
        *saida = (flags & ~SQLITE_OPEN_READWRITE) | SQLITE_OPEN_READONLY;
    return SQLITE_OK;
}

static int xbApagar(sqlite3_vfs *v, const char *n, int s) { (void)v; (void)n; (void)s; return SQLITE_READONLY; }

static int xbAcessar(sqlite3_vfs *vfs, const char *nome, int tipo, int *resultado)
{
    DWORD atributos;
    (void)vfs;

    atributos = GetFileAttributes(nome);
    if (atributos == INVALID_FILE_ATTRIBUTES)
        *resultado = 0;
    else if (tipo == SQLITE_ACCESS_READWRITE)
        *resultado = 0;          /* nunca: este VFS e somente leitura */
    else
        *resultado = 1;

    return SQLITE_OK;
}

static int xbCaminhoCompleto(sqlite3_vfs *vfs, const char *entrada, int tamanho, char *saida)
{
    (void)vfs;
    /* Caminhos do Xbox ja sao absolutos por dispositivo ("Hdd1:\\Freestyle\\..."),
     * nao ha diretorio corrente para resolver. */
    sqlite3_snprintf(tamanho, saida, "%s", entrada);
    return SQLITE_OK;
}

static int xbAleatorio(sqlite3_vfs *vfs, int quantos, char *destino)
{
    int i;
    DWORD semente = GetTickCount();
    (void)vfs;

    for (i = 0; i < quantos; i++)
    {
        semente = semente * 1103515245 + 12345;
        destino[i] = (char)(semente >> 16);
    }
    return quantos;
}

static int xbDormir(sqlite3_vfs *vfs, int microssegundos)
{
    (void)vfs;
    Sleep((DWORD)((microssegundos + 999) / 1000));
    return microssegundos;
}

static int xbHoraAtual(sqlite3_vfs *vfs, double *juliano)
{
    (void)vfs;
    /* O relogio do console costuma estar zerado (arquivos saem datados de 2005-11-22),
     * entao qualquer valor daqui seria mentira. Devolvemos uma data fixa: nada no nosso
     * uso depende da hora, e um valor estavel e melhor que um errado e variavel. */
    *juliano = 2451545.0;   /* 2000-01-01 12:00 UTC */
    return SQLITE_OK;
}

static sqlite3_vfs VFS_XBOX = {
    1,                      /* iVersion */
    sizeof(ArquivoXbox),    /* szOsFile */
    260,                    /* mxPathname */
    0,                      /* pNext */
    "xbox",                 /* zName */
    0,                      /* pAppData */
    xbAbrir, xbApagar, xbAcessar, xbCaminhoCompleto,
    0, 0, 0, 0,             /* dl*: sem carregamento de extensao */
    xbAleatorio, xbDormir, xbHoraAtual, 0
};

int sqlite3_os_init(void)
{
    return sqlite3_vfs_register(&VFS_XBOX, 1);
}

int sqlite3_os_end(void)
{
    return SQLITE_OK;
}
