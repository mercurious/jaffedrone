/* Jaffedrone UI. The look borrows from Norman Jaffe's houses: long sloping
 * rooflines, chamfered angular masses, cedar and sandstone against slate,
 * one copper edge catching the light. */
#include "ui.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"
#include "gfx.h"
#include "sync.h"

#define C_BG 0x14161b
#define C_PANEL 0x1c1f27
#define C_PANEL2 0x282c37
#define C_LINE 0x3a3f4c
#define C_TEXT 0xf2ebdd
#define C_DIM 0x7d7f8a
#define C_COPPER 0xb3875c
#define C_CEDAR 0x7f6553
#define C_SAND 0xd9c3a0
#define C_SEA 0x7fd1a8
#define C_RED 0xff5a50
#define C_OFF 0x3a3f4c
#define HEADER_H 46
#define BODY_Y 58
#define FOOTER_Y 456

static void nop(void) {}
void (*ui_lock)(void) = nop;
void (*ui_unlock)(void) = nop;

static const uint32_t scol[NS] = {0xc9a67c, 0xad7f61, 0xd9c3a0, 0x7f9aa6};
static const char *const page_names[PAGE_COUNT] = {"STRINGS", "DRONE", "MOD", "FX"};
static int page, cur_s, cur_p, gcur, mcur_l, mcur_p, fcur_c, fcur_p;
static int held[BTN_COUNT], hold_ms[BTN_COUNT], rep_ms[BTN_COUNT], rep_n[BTN_COUNT];
static int quit, changed, now_ms, toast_ms;
static char toast[48];

static int wrap(int x, int n) { return ((x % n) + n) % n; }
static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static void say(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(toast, sizeof toast, fmt, ap);
    va_end(ap);
    toast_ms = 1400;
}

void ui_init(void) {}
int ui_quit_requested(void) { return quit; }
int ui_take_changed(void)
{
    int c = changed;
    changed = 0;
    return c;
}

/* ------------------------------------------------------------- controls */

static int *cur_value(const ParamDef **d)
{
    switch (page) {
    case PAGE_STRINGS: *d = sparam_def(cur_p); return &song.sp[cur_s][cur_p];
    case PAGE_DRONE: *d = gparam_def(gcur); return &song.gp[gcur];
    case PAGE_MOD: *d = lparam_def(mcur_p); return &song.lfo[mcur_l][mcur_p];
    default: *d = fparam_def(fcur_c * 3 + fcur_p); return &song.fx[fcur_c * 3 + fcur_p];
    }
}

static void adjust(int dir, int repeats)
{
    const ParamDef *d;
    int *v = cur_value(&d);
    int step = (repeats > 12 && d->max - d->min > 30) ? 4 : 1;
    int nv = clampi(*v + dir * step, d->min, d->max);
    if (nv == *v)
        return;
    *v = nv;
    if (page == PAGE_DRONE && gcur == GP_BPM)
        engine_set_bpm((float)nv);
    if (page == PAGE_DRONE && gcur == GP_SYNC)
        engine_set_rate(1.0);
    if (page == PAGE_DRONE && gcur == GP_MODE)
        say("%s", nv == MODE_MONO ? "MONO: ALL STRINGS FOLLOW STRING 1" : "CHORD: EACH STRING ITS OWN NOTE");
}

static void action(int b, int repeats)
{
    switch (b) {
    case BTN_L1: page = wrap(page - 1, PAGE_COUNT); return;
    case BTN_R1: page = wrap(page + 1, PAGE_COUNT); return;
    case BTN_START:
        engine_set_playing(!live.playing);
        say("%s", live.playing ? "DRONE ON" : "DRONE OFF");
        return;
    case BTN_SELECT:
        engine_randomize();
        changed = 1;
        say("NEW CHORD");
        return;
    case BTN_MENU: quit = 1; return;
    case BTN_Y: adjust(-1, repeats); changed = 1; return;
    case BTN_X: adjust(+1, repeats); changed = 1; return;
    case BTN_B:
        if (page == PAGE_STRINGS)
            engine_pluck(cur_s);
        else
            for (int s = 0; s < NS; s++)
                engine_pluck(s);
        return;
    }
    changed = 1;
    int dir = b == BTN_DOWN ? 1 : b == BTN_UP ? -1 : 0;
    int side = b == BTN_RIGHT ? 1 : b == BTN_LEFT ? -1 : 0;
    switch (page) {
    case PAGE_STRINGS:
        cur_s = wrap(cur_s + side, NS);
        cur_p = wrap(cur_p + dir, NSP);
        if (b == BTN_A) {
            song.on[cur_s] = !song.on[cur_s];
            if (song.on[cur_s])
                engine_pluck(cur_s);
            say("STRING %d %s", cur_s + 1, song.on[cur_s] ? "ON" : "OFF");
        }
        break;
    case PAGE_DRONE: gcur = wrap(gcur + dir, NGP); break;
    case PAGE_MOD:
        mcur_l = wrap(mcur_l + side, NLFO);
        mcur_p = wrap(mcur_p + dir, NLP);
        break;
    default:
        fcur_c = wrap(fcur_c + side, 3);
        fcur_p = wrap(fcur_p + dir, 3);
        break;
    }
}

static int repeatable(int b) { return b <= BTN_RIGHT || b == BTN_X || b == BTN_Y; }

void ui_button(int b, int down)
{
    if (b < 0 || b >= BTN_COUNT || held[b] == down)
        return;
    held[b] = down;
    hold_ms[b] = rep_ms[b] = rep_n[b] = 0;
    if (down) {
        ui_lock();
        action(b, 0);
        ui_unlock();
    }
}

void ui_tick(int ms)
{
    now_ms += ms;
    if (toast_ms > 0)
        toast_ms -= ms;
    for (int b = 0; b < BTN_COUNT; b++) {
        if (!held[b] || !repeatable(b))
            continue;
        hold_ms[b] += ms;
        if (hold_ms[b] < 300)
            continue;
        rep_ms[b] += ms;
        int interval = hold_ms[b] > 1500 ? 35 : 75;
        while (rep_ms[b] >= interval) {
            rep_ms[b] -= interval;
            ui_lock();
            action(b, ++rep_n[b]);
            ui_unlock();
        }
    }
}

/* -------------------------------------------------------- shape helpers */

/* a mass whose top edge slopes: left corner at y, right corner at y + drop */
static void roof(int x, int y, int w, int h, int drop, uint32_t c)
{
    int top = drop > 0 ? y + drop : y, bot = y + h;
    gfx_fill(x, top, w, bot - top, c);
    if (drop > 0)
        gfx_tri(x, y, x + w, y + drop, x, y + drop, c);
    else if (drop < 0)
        gfx_tri(x, y, x + w, y + drop, x + w, y, c);
}

/* a block with its top-right corner cut off */
static void chamfer(int x, int y, int w, int h, int cut, uint32_t c)
{
    gfx_fill(x, y + cut, w, h - cut, c);
    gfx_fill(x, y, w - cut, cut, c);
    gfx_tri(x + w - cut, y, x + w, y + cut, x + w - cut, y + cut, c);
}

/* a parallelogram whose top is shifted right by skew */
static void para(int x, int y, int w, int h, int skew, uint32_t c)
{
    gfx_fill(x + skew, y, w - skew, h, c);
    gfx_tri(x, y + h, x + skew, y, x + skew, y + h, c);
    gfx_tri(x + w, y, x + w + skew, y, x + w, y + h, c);
}

/* a thin diagonal line as two triangles */
static void slant(int x0, int y0, int x1, int y1, int t, uint32_t c)
{
    gfx_tri(x0, y0, x1, y1, x1, y1 + t, c);
    gfx_tri(x0, y0, x0, y0 + t, x1, y1 + t, c);
}

/* a parameter row: name left, value right, bar along the bottom */
static void param_row(int x, int y, int w, int h, int sel, int on, const char *name, const char *val,
                      float t, int bipolar, uint32_t col)
{
    chamfer(x, y, w, h, 7, sel ? gfx_mix(C_PANEL2, col, 0.18f) : C_PANEL);
    if (sel)
        gfx_fill(x, y, 3, h, col);
    gfx_text(x + 9, y + 2, name, sel ? col : C_DIM, FONT_SMALL);
    gfx_text(x + w - 9 - gfx_text_w(val, FONT_SMALL), y + 2, val, on ? C_TEXT : C_DIM, FONT_SMALL);
    int bx = x + 9, bw = w - 18, by = y + h - 8;
    gfx_fill(bx, by, bw, 4, C_LINE);
    if (bipolar) {
        int mid = bx + bw / 2, end = bx + (int)(bw * t);
        gfx_fill(end < mid ? end : mid, by, abs(end - mid) + 1, 4, on ? col : C_DIM);
    }
    else
        gfx_fill(bx, by, (int)(bw * t), 4, on ? col : C_DIM);
}

/* a vibrating string: a standing wave whose amplitude follows the meter */
static void draw_string(int x0, int x1, int y, float amp, float ph, uint32_t col, int on)
{
    int w = x1 - x0;
    float a = fminf(1.f, sqrtf(amp)) * 11.f;
    int py = y;
    for (int i = 0; i <= w; i++) {
        float t = (float)i / w;
        float s = sinf(t * 3.14159265f) * sinf(t * 6.2831853f * 2.f + ph) * a;
        int yy = y + (int)s;
        if (i > 0) {
            int lo = yy < py ? yy : py, hi = yy < py ? py : yy;
            gfx_fill(x0 + i, lo, 1, hi - lo + 2, on ? col : C_OFF);
        }
        py = yy;
    }
    /* end posts */
    gfx_fill(x0 - 2, y - 7, 3, 15, on ? col : C_OFF);
    gfx_fill(x1, y - 7, 3, 15, on ? col : C_OFF);
}

/* ---------------------------------------------------------------- pages */

static void draw_header(void)
{
    gfx_fill(0, 0, SCREEN_W, HEADER_H, C_PANEL);
    /* the roofline: one copper edge across the band */
    slant(150, HEADER_H - 3, SCREEN_W, HEADER_H - 16, 3, C_COPPER);
    gfx_text(10, 4, "JAFFEDRONE", C_SAND, FONT_LARGE);
    int x = 168;
    for (int p = 0; p < PAGE_COUNT; p++) {
        int w = gfx_text_w(page_names[p], FONT_SMALL) + 14;
        para(x, 10, w, 24, 8, p == page ? C_COPPER : C_PANEL2);
        gfx_text(x + 11, 11, page_names[p], p == page ? 0x000000 : C_DIM, FONT_SMALL);
        x += w + 14;
    }
    char s[24], nm[8];
    note_name(song.gp[GP_ROOT], nm, sizeof nm);
    snprintf(s, sizeof s, "%s %d", nm, song.gp[GP_BPM]);
    int bw = gfx_text_w(s, FONT_LARGE);
    gfx_text(SCREEN_W - 10 - bw, 4, s, C_SAND, FONT_LARGE);
    int ix = SCREEN_W - 10 - bw - 30;
    if (live.playing) {
        float ph = 1.f - fmodf(now_ms / 1000.f * engine_bpm() / 60.f, 1.f);
        gfx_disc(ix + 8, 24, 5.f + 4.f * ph * ph, C_SEA);
    }
    else
        gfx_ring(ix + 8, 24, 7.f, 2.f, C_DIM);
    if (song.gp[GP_SYNC] != SYNC_OFF) {
        const SyncStatus *ss = sync_status();
        int linked = ss->leading || ss->have_leader;
        const char *t = ss->leading ? "LEAD" : ss->have_leader ? "FOLLOW" : "SYNC";
        int w = gfx_text_w(t, FONT_SMALL) + 16;
        para(ix - w - 14, 12, w, 22, 6, linked ? C_SEA : C_PANEL2);
        gfx_text(ix - w - 14 + 11, 13, t, linked ? 0x000000 : C_DIM, FONT_SMALL);
    }
}

static void draw_footer(void)
{
    static const char *const hints[PAGE_COUNT] = {
        "<>:STRING ^v:PARAM Y:- X:+ A:ON/OFF B:PLUCK SEL:CHORD START:DRONE",
        "^v:PARAM Y:- X:+ B:PLUCK ALL SEL:NEW CHORD START:DRONE",
        "<>:LFO ^v:PARAM Y:- X:+ B:PLUCK ALL START:DRONE",
        "<>:SECTION ^v:PARAM Y:- X:+ B:PLUCK ALL START:DRONE",
    };
    gfx_fill(0, FOOTER_Y, SCREEN_W, SCREEN_H - FOOTER_Y, C_PANEL);
    gfx_fill(0, FOOTER_Y, 4, SCREEN_H - FOOTER_Y, C_COPPER);
    gfx_text(12, FOOTER_Y + 2, hints[page], C_DIM, FONT_SMALL);
}

static void draw_toast(void)
{
    if (toast_ms <= 0)
        return;
    int w = gfx_text_w(toast, FONT_SMALL) + 36, h = 30;
    int x = (SCREEN_W - w) / 2, y = FOOTER_Y - h - 10;
    chamfer(x, y, w, h, 10, C_PANEL2);
    gfx_fill(x, y, 3, h, C_COPPER);
    gfx_text(x + 16, y + 6, toast, C_TEXT, FONT_SMALL);
}

static void draw_strings(void)
{
    const int cw = SCREEN_W / NS;
    const int hh = 72, y0 = BODY_Y + hh + 4, rh = (FOOTER_Y - 4 - y0) / NSP;
    for (int s = 0; s < NS; s++) {
        int x = s * cw + 3, w = cw - 6;
        int on = song.on[s], sel = s == cur_s;
        uint32_t col = scol[s];
        /* one long roof falls across the four columns */
        int drop = 6, top = BODY_Y + s * drop;
        roof(x, top, w, hh - s * drop, drop, sel ? gfx_mix(C_PANEL2, col, 0.14f) : C_PANEL);
        if (sel)
            gfx_fill(x, BODY_Y + hh - 3, w, 3, col);
        char nm[16], s1[8];
        int note = song.gp[GP_MODE] == MODE_MONO ? song.sp[0][SP_NOTE] : song.sp[s][SP_NOTE];
        note_name(song.gp[GP_ROOT] + note, s1, sizeof s1);
        snprintf(nm, sizeof nm, "%d  %s", s + 1, s1);
        gfx_text(x + 8, top + drop + 2, nm, on ? col : C_DIM, FONT_SMALL);
        const char *ot = on ? "ON" : "OFF";
        gfx_text(x + w - 8 - gfx_text_w(ot, FONT_SMALL), top + drop + 2, ot, on ? C_SEA : C_DIM, FONT_SMALL);
        draw_string(x + 12, x + w - 12, BODY_Y + 56, on ? live.meter[s] : 0.f, live.phase[s], col, on);

        for (int p = 0; p < NSP; p++) {
            const ParamDef *d = sparam_def(p);
            char v[20];
            sparam_format(s, p, v, sizeof v);
            float t = (float)(song.sp[s][p] - d->min) / (d->max - d->min);
            int dimmed = song.gp[GP_MODE] == MODE_MONO && s > 0 && p == SP_NOTE;
            param_row(x, y0 + p * rh, w, rh - 3, sel && p == cur_p, on && !dimmed, d->name, v, t, d->min < 0, col);
        }
    }
}

static void draw_drone(void)
{
    /* the house: a long low roof over the four strings, chimney meter */
    int lx = 8, lw = 300, ly = BODY_Y, lh = FOOTER_Y - 6 - BODY_Y;
    roof(lx, ly, lw, lh, 54, C_PANEL);
    gfx_fill(lx + lw - 30, ly + 24, 10, 36, C_PANEL2); /* chimney */
    slant(lx, ly, lx + lw, ly + 54, 3, C_COPPER);
    gfx_text(lx + 12, ly + 64, song.gp[GP_MODE] == MODE_MONO ? "MONO DRONE" : "CHORD DRONE", C_DIM, FONT_SMALL);
    for (int s = 0; s < NS; s++) {
        int y = ly + 120 + s * 72;
        char nm[8];
        int note = song.gp[GP_MODE] == MODE_MONO ? song.sp[0][SP_NOTE] : song.sp[s][SP_NOTE];
        note_name(song.gp[GP_ROOT] + note, nm, sizeof nm);
        gfx_text(lx + 14, y - 26, nm, song.on[s] ? scol[s] : C_DIM, FONT_SMALL);
        draw_string(lx + 16, lx + lw - 50, y, song.on[s] ? live.meter[s] : 0.f, live.phase[s], scol[s], song.on[s]);
    }
    int mx = lx + lw - 24, mt = ly + 100, mhh = lh - 120, mh = (int)(mhh * fminf(1.f, live.master_meter));
    gfx_fill(mx, mt, 8, mhh, C_PANEL2);
    gfx_fill(mx, mt + mhh - mh, 8, mh, live.master_meter > 0.95f ? C_RED : C_SEA);

    int x = 320, w = SCREEN_W - 8 - x, rh = (FOOTER_Y - 6 - BODY_Y) / NGP;
    for (int p = 0; p < NGP; p++) {
        const ParamDef *d = gparam_def(p);
        char v[20];
        gparam_format(p, v, sizeof v);
        float t = (float)(song.gp[p] - d->min) / (d->max - d->min);
        param_row(x, BODY_Y + p * rh, w, rh - 4, p == gcur, 1, d->name, v, t, 0, C_COPPER);
    }
}

static void draw_mod(void)
{
    const int cw = SCREEN_W / NLFO, hh = 120, y0 = BODY_Y + hh + 6, rh = (FOOTER_Y - 6 - y0) / NLP;
    for (int l = 0; l < NLFO; l++) {
        int x = l * cw + 8, w = cw - 16, sel = l == mcur_l;
        uint32_t col = l ? C_SAND : C_COPPER;
        roof(x, BODY_Y, w, hh, l ? -12 : 12, sel ? gfx_mix(C_PANEL2, col, 0.12f) : C_PANEL);
        char t[12];
        snprintf(t, sizeof t, "LFO %d", l + 1);
        gfx_text(x + 10, BODY_Y + 16, t, sel ? col : C_DIM, FONT_SMALL);
        /* the shape over one cycle, and where we are in it */
        int gx = x + 14, gw = w - 28, gy = BODY_Y + 44, gh = 60;
        gfx_fill(gx, gy + gh, gw, 1, C_LINE);
        int shape = song.lfo[l][LP_SHAPE];
        float depth = song.lfo[l][LP_DEPTH] / 100.f;
        for (int i = 0; i < gw; i++) {
            float ph = (float)i / gw, v;
            switch (shape) {
            case SH_SINE: v = 0.5f - 0.5f * cosf(ph * 6.2831853f); break;
            case SH_TRI: v = ph < 0.5f ? ph * 2.f : 2.f - ph * 2.f; break;
            case SH_SAW: v = 1.f - ph; break;
            case SH_SQUARE: v = ph < 0.5f ? 1.f : 0.f; break;
            default: v = 0.3f + 0.4f * ((i * 7919 / (gw / 6 + 1)) % 5) / 4.f; break;
            }
            int h = (int)(v * gh * (0.15f + 0.85f * depth));
            gfx_fill(gx + i, gy + gh - h, 1, h + 1, depth > 0.f ? gfx_mix(C_PANEL2, col, 0.6f) : C_LINE);
        }
        float cur = live.lfo_val[l];
        int cx = gx + (int)(gw * fmodf(now_ms * 0.001f, 1.f));
        (void)cx;
        gfx_fill(gx + gw + 4, gy + gh - (int)(cur * gh), 6, (int)(cur * gh) + 1, col);
        for (int p = 0; p < NLP; p++) {
            const ParamDef *d = lparam_def(p);
            char v[20];
            lparam_format(l, p, v, sizeof v);
            float t = (float)(song.lfo[l][p] - d->min) / (d->max - d->min);
            param_row(x, y0 + p * rh, w, rh - 4, sel && p == mcur_p, 1, d->name, v, t, 0, col);
        }
    }
}

static void draw_fx(void)
{
    static const char *const names[3] = {"DELAY", "REVERB", "GATE"};
    const int cw = SCREEN_W / 3, hh = 110, y0 = BODY_Y + hh + 6, rh = (FOOTER_Y - 6 - y0) / 3;
    for (int c = 0; c < 3; c++) {
        int x = c * cw + 6, w = cw - 12, sel = c == fcur_c;
        uint32_t col = c == 0 ? C_COPPER : c == 1 ? C_SAND : C_SEA;
        roof(x, BODY_Y + c * 8, w, hh - c * 8, 8, sel ? gfx_mix(C_PANEL2, col, 0.12f) : C_PANEL);
        gfx_text(x + 10, BODY_Y + c * 8 + 14, names[c], sel ? col : C_DIM, FONT_SMALL);
        int gx = x + 14, gy = BODY_Y + 48, gw = w - 28, gh = 50;
        if (c == 0) {
            /* echoes dying away */
            float fb = song.fx[FX_DFB] / 100.f, a = 1.f;
            for (int i = 0; i < 6; i++) {
                int h = (int)(gh * a * (0.3f + 0.7f * song.fx[FX_DMIX] / 100.f));
                gfx_fill(gx + i * (gw / 6), gy + gh - h, gw / 6 - 6, h, i ? gfx_mix(C_PANEL2, col, a) : col);
                a *= 0.35f + 0.6f * fb;
            }
        }
        else if (c == 1) {
            /* a wedge of decay */
            int len = (int)(gw * (0.3f + 0.7f * song.fx[FX_VTIME] / 100.f));
            gfx_tri(gx, gy, gx + len, gy + gh, gx, gy + gh, gfx_mix(C_PANEL2, col, 0.3f + 0.6f * song.fx[FX_VSEND] / 100.f));
        }
        else {
            /* the gate pattern over a beat, live gain on the right */
            int on = song.fx[FX_GRATE] > 0, duty = song.fx[FX_GDUTY];
            int slots = 4;
            for (int i = 0; i < slots; i++) {
                int sw = gw / slots, ow = sw * duty / 10;
                gfx_fill(gx + i * sw, gy + 10, sw - 4, gh - 10, C_PANEL2);
                gfx_fill(gx + i * sw, gy + 10, on ? ow : sw - 4, gh - 10, on ? gfx_mix(C_PANEL2, col, 0.7f) : C_LINE);
            }
            int lh = (int)((gh - 10) * live.gate);
            gfx_fill(gx + gw + 2, gy + gh - lh, 6, lh + 1, col);
        }
        for (int p = 0; p < 3; p++) {
            int idx = c * 3 + p;
            const ParamDef *d = fparam_def(idx);
            char v[20];
            fparam_format(idx, v, sizeof v);
            float t = (float)(song.fx[idx] - d->min) / (d->max - d->min);
            param_row(x, y0 + p * rh, w, rh - 4, sel && p == fcur_p, 1, d->name, v, t, 0, col);
        }
    }
}

void ui_draw(uint32_t *fb)
{
    gfx_target(fb);
    gfx_clear(C_BG);
    draw_header();
    switch (page) {
    case PAGE_STRINGS: draw_strings(); break;
    case PAGE_DRONE: draw_drone(); break;
    case PAGE_MOD: draw_mod(); break;
    default: draw_fx(); break;
    }
    draw_footer();
    draw_toast();
}
