#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <net/net.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PROXY_IP
#define PROXY_IP "192.168.0.15"
#endif
#define PROXY_PORT 8080
static char proxy_ip[32] = PROXY_IP;

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

#define COL_BG    0x00102040
#define COL_TITLE 0x0066D9FF
#define COL_TEXT  0x00FFFFFF
#define COL_HINT  0x0090B0E0

static gcmContextData *context;
static u32 *fb[2];
static u32 fb_off[2];
static u32 cur = 0;
static int W, H;
static int first_flip = 1;
static volatile int running = 1;

/* ---------- Fonte 5x7 (ASCII 32..126), colunas, bit0 = topo ---------- */
static const u8 font5x7[95][5] = {
 {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
 {0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
 {0x36,0x49,0x56,0x20,0x50},{0x00,0x08,0x07,0x03,0x00},{0x00,0x1C,0x22,0x41,0x00},
 {0x00,0x41,0x22,0x1C,0x00},{0x2A,0x1C,0x7F,0x1C,0x2A},{0x08,0x08,0x3E,0x08,0x08},
 {0x00,0x80,0x70,0x30,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x00,0x60,0x60,0x00},
 {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
 {0x72,0x49,0x49,0x49,0x46},{0x21,0x41,0x49,0x4D,0x33},{0x18,0x14,0x12,0x7F,0x10},
 {0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x31},{0x41,0x21,0x11,0x09,0x07},
 {0x36,0x49,0x49,0x49,0x36},{0x46,0x49,0x49,0x29,0x1E},{0x00,0x00,0x14,0x00,0x00},
 {0x00,0x40,0x34,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
 {0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x59,0x09,0x06},{0x3E,0x41,0x5D,0x59,0x4E},
 {0x7C,0x12,0x11,0x12,0x7C},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
 {0x7F,0x41,0x41,0x41,0x3E},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
 {0x3E,0x41,0x41,0x51,0x73},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
 {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
 {0x7F,0x02,0x1C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
 {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
 {0x26,0x49,0x49,0x49,0x32},{0x03,0x01,0x7F,0x01,0x03},{0x3F,0x40,0x40,0x40,0x3F},
 {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
 {0x03,0x04,0x78,0x04,0x03},{0x61,0x59,0x49,0x4D,0x43},{0x00,0x7F,0x41,0x41,0x41},
 {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x41,0x7F},{0x04,0x02,0x01,0x02,0x04},
 {0x40,0x40,0x40,0x40,0x40},{0x00,0x03,0x07,0x08,0x00},{0x20,0x54,0x54,0x78,0x40},
 {0x7F,0x28,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x28},{0x38,0x44,0x44,0x28,0x7F},
 {0x38,0x54,0x54,0x54,0x18},{0x00,0x08,0x7E,0x09,0x02},{0x18,0xA4,0xA4,0x9C,0x78},
 {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x40,0x3D,0x00},
 {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x78,0x04,0x78},
 {0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0xFC,0x18,0x24,0x24,0x18},
 {0x18,0x24,0x24,0x18,0xFC},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x24},
 {0x04,0x04,0x3F,0x44,0x24},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
 {0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x4C,0x90,0x90,0x90,0x7C},
 {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x77,0x00,0x00},
 {0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}
};

/* ---------- Video ---------- */
static void init_screen(void) {
    void *host = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&context, CB_SIZE, HOST_SIZE, host);

    videoState state;
    videoGetState(0, 0, &state);
    videoResolution res;
    videoGetResolution(state.displayMode.resolution, &res);

    videoConfiguration vc;
    memset(&vc, 0, sizeof(vc));
    vc.resolution = state.displayMode.resolution;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = res.width * sizeof(u32);
    videoConfigure(0, &vc, NULL, 0);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    W = res.width;
    H = res.height;
    for (int i = 0; i < 2; i++) {
        fb[i] = (u32 *)rsxMemalign(64, H * vc.pitch);
        rsxAddressToOffset(fb[i], &fb_off[i]);
        gcmSetDisplayBuffer(i, fb_off[i], vc.pitch, W, H);
    }
    gcmResetFlipStatus();
}

static void present(void) {
    if (!first_flip) {
        while (gcmGetFlipStatus() != 0) usleep(200);
    }
    first_flip = 0;
    gcmResetFlipStatus();
    gcmSetFlip(context, cur);
    rsxFlushBuffer(context);
    gcmSetWaitFlip(context);
    cur ^= 1;
}

static void clear(u32 color) {
    u32 n = (u32)W * (u32)H;
    for (u32 i = 0; i < n; i++) fb[cur][i] = color;
}

/* ---------- Texto ---------- */
static void draw_char(int x, int y, int c, int sc, u32 color) {
    if (c < 32 || c > 126) c = '?';
    const u8 *g = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (!(g[col] & (1 << row))) continue;
            for (int dy = 0; dy < sc; dy++) {
                for (int dx = 0; dx < sc; dx++) {
                    int px = x + col * sc + dx;
                    int py = y + row * sc + dy;
                    if (px >= 0 && px < W && py >= 0 && py < H)
                        fb[cur][py * W + px] = color;
                }
            }
        }
    }
}

/* Escreve texto com quebra de linha. Retorna o Y final. */
static int draw_text(int x, int y, const char *s, int sc, u32 color, int maxw) {
    int cx = x, cw = 6 * sc, lh = 9 * sc;
    for (; *s; s++) {
        if (*s == '\n') { cx = x; y += lh; continue; }
        if (*s == '\r') continue;
        if (cx + cw > x + maxw) { cx = x; y += lh; }
        draw_char(cx, y, (unsigned char)*s, sc, color);
        cx += cw;
    }
    return y + lh;
}

static void sys_cb(u64 status, u64 param, void *ud) {
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

/* ---------- Rede ---------- */
static int http_get(const char *path, char *out, int outsz) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(PROXY_PORT);
    a.sin_addr.s_addr = inet_addr(proxy_ip);
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0) {
        close(s);
        return -2;
    }
    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n",
             path, proxy_ip);
    send(s, req, strlen(req), 0);

    static char raw[4096];
    int n = 0, r;
    while (n < (int)sizeof(raw) - 1 &&
           (r = recv(s, raw + n, sizeof(raw) - 1 - n, 0)) > 0)
        n += r;
    raw[n] = 0;
    close(s);

    char *body = strstr(raw, "\r\n\r\n");
    body = body ? body + 4 : raw;
    strncpy(out, body, outsz - 1);
    out[outsz - 1] = 0;
    return 0;
}

/* ---------- Utilidades ---------- */
static void fill_rect(int x, int y, int w, int h, u32 color) {
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= H) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= W) continue;
            fb[cur][j * W + i] = color;
        }
    }
}

/* Converte UTF-8 (acentos, aspas) para ASCII simples, no proprio buffer */
static void fold_ascii(char *s) {
    unsigned char *r = (unsigned char *)s;
    char *w = s;
    while (*r) {
        unsigned char c = *r;
        if (c < 0x80) { *w++ = (char)c; r++; continue; }
        char out = '?';
        if (c == 0xC3 && r[1]) {
            unsigned char d = r[1];
            if (d >= 0x80 && d <= 0x85) out = 'A';
            else if (d == 0x87) out = 'C';
            else if (d >= 0x88 && d <= 0x8B) out = 'E';
            else if (d >= 0x8C && d <= 0x8F) out = 'I';
            else if (d == 0x91) out = 'N';
            else if (d >= 0x92 && d <= 0x96) out = 'O';
            else if (d >= 0x99 && d <= 0x9C) out = 'U';
            else if (d >= 0xA0 && d <= 0xA5) out = 'a';
            else if (d == 0xA7) out = 'c';
            else if (d >= 0xA8 && d <= 0xAB) out = 'e';
            else if (d >= 0xAC && d <= 0xAF) out = 'i';
            else if (d == 0xB1) out = 'n';
            else if (d >= 0xB2 && d <= 0xB6) out = 'o';
            else if (d >= 0xB9 && d <= 0xBC) out = 'u';
            r += 2;
        } else if (c == 0xE2 && r[1] == 0x80 && r[2]) {
            unsigned char d = r[2];
            if (d == 0x98 || d == 0x99) out = '\'';
            else if (d == 0x9C || d == 0x9D) out = '"';
            else if (d == 0x93 || d == 0x94) out = '-';
            else if (d == 0xA2) out = '*';
            r += 3;
        } else {
            r++;
            while ((*r & 0xC0) == 0x80) r++;
        }
        *w++ = out;
    }
    *w = 0;
}

static void urlenc(const char *s, char *out, int outsz) {
    static const char hex[] = "0123456789ABCDEF";
    int n = 0;
    for (; *s && n < outsz - 4; s++) {
        unsigned char c = (unsigned char)*s;
        if (isalnum(c) || c == '-' || c == '_' || c == '.') {
            out[n++] = (char)c;
        } else {
            out[n++] = '%';
            out[n++] = hex[c >> 4];
            out[n++] = hex[c & 15];
        }
    }
    out[n] = 0;
}

/* conta quantas linhas o texto ocupa (mesma regra do draw_text) */
static int text_lines(const char *s, int sc, int maxw) {
    int cw = 6 * sc, cx = 0, lines = 1;
    for (; *s; s++) {
        if (*s == '\n') { lines++; cx = 0; continue; }
        if (*s == '\r') continue;
        if (cx + cw > maxw) { lines++; cx = 0; }
        cx += cw;
    }
    return lines;
}

/* ---------- IP salvo em arquivo ---------- */
#define CFG_PATH "/dev_hdd0/game/CLAUDE001/USRDIR/proxy_ip.txt"

static int ip_valid(const char *s) {
    if (!s || !*s) return 0;

    for (int part = 0; part < 4; part++) {
        int value = 0;
        int digits = 0;

        while (*s && *s != '.') {
            if (!isdigit((unsigned char)*s)) return 0;
            value = value * 10 + (*s - '0');
            if (value > 255 || ++digits > 3) return 0;
            s++;
        }

        if (digits == 0) return 0;
        if (part < 3) {
            if (*s != '.') return 0;
            s++;
        } else if (*s != '\0') {
            return 0;
        }
    }
    return 1;
}

static void load_cfg(void) {
    FILE *f = fopen(CFG_PATH, "r");
    if (!f) return;
    char b[40];
    memset(b, 0, sizeof(b));
    if (fgets(b, sizeof(b), f)) {
        char *p = b;
        while (*p && *p != '\r' && *p != '\n') p++;
        *p = 0;
        if (ip_valid(b)) {
            strncpy(proxy_ip, b, sizeof(proxy_ip) - 1);
            proxy_ip[sizeof(proxy_ip) - 1] = 0;
        }
    }
    fclose(f);
}

static void save_cfg(void) {
    FILE *f = fopen(CFG_PATH, "w");
    if (!f) return;
    fprintf(f, "%s\n", proxy_ip);
    fclose(f);
}

/* ---------- Botoes ---------- */
typedef struct { int prev; int cnt; } Btn;

static int pressed(Btn *b, int now, int repeat) {
    if (!now) { b->prev = 0; b->cnt = 0; return 0; }
    if (!b->prev) { b->prev = 1; b->cnt = 0; return 1; }
    b->cnt++;
    if (repeat && b->cnt >= 20 && ((b->cnt - 20) % 5) == 0) return 1;
    return 0;
}

/* ---------- Teclado ---------- */
#define MAX_IN 60

static const char *krows[4] = {
    "abcdefghij", "klmnopqrst", "uvwxyz.,?!", "0123456789"
};
static const char *acts[4] = {"Aa", "ESPACO", "APAGAR", "ENVIAR"};
static const int act_start[4] = {0, 2, 5, 8};
static const int act_span[4]  = {2, 3, 3, 2};

static int act_of(int c) { return c < 2 ? 0 : (c < 5 ? 1 : (c < 8 ? 2 : 3)); }

static void append(char *input, int *len, char c) {
    if (*len < MAX_IN) { input[(*len)++] = c; input[*len] = 0; }
}

static void render(const char *answer, const char *input, int row, int col,
                   int caps, int ip_mode, int *scroll) {
    int S = (H >= 1000) ? 4 : (H >= 700 ? 3 : 2);
    int hs = (S > 2) ? 2 : 1;
    int m = 40;
    int ch = 12 * S, gap = 4;
    int kb_h = 5 * (ch + gap);
    int kb_top = H - m - kb_h;
    int in_y = kb_top - ch - 10;
    int kw = (W - 2 * m) / 10;
    int lh = 9 * S;
    int top = m + 11 * S;

    int vis = (in_y - 8 - top) / lh;
    if (vis < 1) vis = 1;
    int lines = text_lines(answer, S, W - 2 * m);
    int maxs = (lines > vis) ? lines - vis : 0;
    if (*scroll > maxs) *scroll = maxs;
    if (*scroll < 0) *scroll = 0;

    clear(COL_BG);
    draw_text(m, top - *scroll * lh, answer, S, COL_TEXT, W - 2 * m);

    /* cabecalho por cima (cobre a parte da resposta que rolou) */
    fill_rect(0, 0, W, top - 2, COL_BG);
    draw_text(m, m, ip_mode ? "IP do proxy" : "PS3i.a", S, COL_TITLE,
              60 * S);
    draw_text(m + 70 * S, m + 2 * S,
              "X=tecla Quad=apaga Tri=espaco Start=envia Circulo=IP "
              "Select=nova L1/R1=rolar",
              hs, COL_HINT, W - 2 * m - 70 * S);

    /* cobre o que passou da area da resposta */
    fill_rect(0, in_y - 8, W, H - in_y + 8, COL_BG);
    fill_rect(m, in_y, W - 2 * m, ch, 0x00203860);
    char line[96];
    snprintf(line, sizeof(line), ip_mode ? "IP: %s_" : "%s_", input);
    draw_text(m + 8, in_y + (ch - 7 * S) / 2, line, S, COL_TEXT,
              W - 2 * m - 16);

    for (int r = 0; r < 5; r++) {
        int y = kb_top + r * (ch + gap);
        int n = (r == 4) ? 4 : 10;
        for (int c = 0; c < n; c++) {
            int x, w;
            char label[12];
            if (r == 4) {
                x = m + act_start[c] * kw;
                w = act_span[c] * kw - gap;
                strncpy(label, acts[c], 11);
                label[11] = 0;
            } else {
                x = m + c * kw;
                w = kw - gap;
                char k = krows[r][c];
                if (caps && isalpha((unsigned char)k))
                    k = (char)toupper((unsigned char)k);
                label[0] = k;
                label[1] = 0;
            }
            int sel = (r == row && c == col);
            fill_rect(x, y, w, ch, sel ? 0x0000C8FF : 0x00304878);
            int tw = (int)strlen(label) * 6 * S;
            draw_text(x + (w - tw) / 2, y + (ch - 7 * S) / 2, label, S,
                      sel ? 0x00000000 : COL_TEXT, w);
        }
    }
}

int main(void) {
    init_screen();
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_cb, NULL);
    ioPadInit(7);
    netInitialize();
    load_cfg();

    static char answer[1000];
    static char reply[520];
    snprintf(answer, sizeof(answer),
             "Digite uma pergunta e escolha ENVIAR.\n"
             "Proxy: %s  (Circulo troca o IP)", proxy_ip);

    char input[MAX_IN + 4] = "";
    int in_len = 0, caps = 0, row = 0, col = 0, ip_mode = 0, scroll = 0;
    Btn b_up = {0, 0}, b_dn = {0, 0}, b_lf = {0, 0}, b_rt = {0, 0};
    Btn b_ok = {0, 0}, b_sq = {0, 0}, b_tr = {0, 0}, b_st = {0, 0};
    Btn b_ci = {0, 0}, b_se = {0, 0}, b_l1 = {0, 0}, b_r1 = {0, 0};
    padInfo pi;
    padData pd;

    while (running) {
        sysUtilCheckCallback();

        int up = 0, dn = 0, lf = 0, rt = 0, ok = 0, sq = 0, tr = 0, st = 0;
        int ci = 0, se = 0, l1 = 0, r1 = 0;
        ioPadGetInfo(&pi);
        if (pi.status[0]) {
            ioPadGetData(0, &pd);
            up = pressed(&b_up, pd.BTN_UP, 1);
            dn = pressed(&b_dn, pd.BTN_DOWN, 1);
            lf = pressed(&b_lf, pd.BTN_LEFT, 1);
            rt = pressed(&b_rt, pd.BTN_RIGHT, 1);
            ok = pressed(&b_ok, pd.BTN_CROSS, 0);
            sq = pressed(&b_sq, pd.BTN_SQUARE, 1);
            tr = pressed(&b_tr, pd.BTN_TRIANGLE, 0);
            st = pressed(&b_st, pd.BTN_START, 0);
            ci = pressed(&b_ci, pd.BTN_CIRCLE, 0);
            se = pressed(&b_se, pd.BTN_SELECT, 0);
            l1 = pressed(&b_l1, pd.BTN_L1, 1);
            r1 = pressed(&b_r1, pd.BTN_R1, 1);
        }

        if (lf || rt) {
            int n = (row == 4) ? 4 : 10;
            col = (col + (rt ? 1 : n - 1)) % n;
        }
        if (up || dn) {
            int old = row;
            row = (row + (dn ? 1 : 4)) % 5;
            if (old == 4 && row != 4) col = act_start[col];
            else if (old != 4 && row == 4) col = act_of(col);
        }
        if (l1) scroll--;
        if (r1) scroll++;

        int send = 0;
        if (ok) {
            if (row < 4) {
                char k = krows[row][col];
                if (caps && isalpha((unsigned char)k))
                    k = (char)toupper((unsigned char)k);
                append(input, &in_len, k);
            } else if (col == 0) {
                caps ^= 1;
            } else if (col == 1) {
                append(input, &in_len, ' ');
            } else if (col == 2) {
                if (in_len > 0) input[--in_len] = 0;
            } else {
                send = 1;
            }
        }
        if (sq && in_len > 0) input[--in_len] = 0;
        if (tr) append(input, &in_len, ' ');
        if (st) send = 1;

        /* Circulo: editar o IP do proxy */
        if (ci) {
            if (!ip_mode) {
                ip_mode = 1;
                strncpy(input, proxy_ip, MAX_IN);
                input[MAX_IN] = 0;
                in_len = (int)strlen(input);
                snprintf(answer, sizeof(answer),
                         "Digite o IP do celular (ex: 192.168.0.15) e aperte "
                         "Start ou ENVIAR.\nCirculo cancela.");
            } else {
                ip_mode = 0;
                input[0] = 0;
                in_len = 0;
                snprintf(answer, sizeof(answer), "Cancelado.");
            }
            scroll = 0;
        }

        /* Select: nova conversa */
        if (se && !ip_mode) {
            char tmp[64];
            http_get("/reset", tmp, sizeof(tmp));
            snprintf(answer, sizeof(answer), "Conversa reiniciada.");
            scroll = 0;
        }

        if (send && ip_mode) {
            if (ip_valid(input)) {
                strncpy(proxy_ip, input, sizeof(proxy_ip) - 1);
                proxy_ip[sizeof(proxy_ip) - 1] = 0;
                save_cfg();
                snprintf(answer, sizeof(answer), "IP salvo: %s", proxy_ip);
                ip_mode = 0;
                input[0] = 0;
                in_len = 0;
            } else {
                snprintf(answer, sizeof(answer),
                         "IP invalido. Use o formato 192.168.0.15");
            }
            scroll = 0;
            send = 0;
        }

        if (send && in_len > 0 && !ip_mode) {
            char enc[300], path[340];
            urlenc(input, enc, sizeof(enc));
            snprintf(path, sizeof(path), "/chat?q=%s", enc);

            snprintf(answer, sizeof(answer), "Pensando...");
            scroll = 0;
            for (int i = 0; i < 2; i++) {
                render(answer, input, row, col, caps, ip_mode, &scroll);
                present();
            }
            int rc = http_get(path, reply, sizeof(reply));
            if (rc != 0) {
                snprintf(answer, sizeof(answer),
                         "Nao consegui falar com o proxy (erro %d).\n"
                         "IP atual: %s\n"
                         "Confira o Termux e aperte Circulo para trocar o IP.",
                         rc, proxy_ip);
            } else {
                fold_ascii(reply);
                if (!reply[0]) strcpy(reply, "(resposta vazia)");
                snprintf(answer, sizeof(answer), "Voce: %s\n\nIA: %s",
                         input, reply);
            }
            input[0] = 0;
            in_len = 0;
        }

        render(answer, input, row, col, caps, ip_mode, &scroll);
        present();
    }

    netDeinitialize();
    return 0;
}
