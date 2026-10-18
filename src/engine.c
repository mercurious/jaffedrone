#include "engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONG_MAGIC 0x4a445204u    /* JDR4: five strings */
#define SONG_MAGIC_V3 0x4a445203u /* JDR3: four strings (v0.0.1) */
#define NS_V3 4
typedef struct {
    uint32_t magic;
    int on[NS_V3];
    int sp[NS_V3][NSP];
    int gp[NGP];
    int lfo[NLFO][NLP];
    int fx[NFX];
} SongV3;

Song song;
Live live;

static const ParamDef sparams[NSP] = {{"NOTE", -24, 24}, {"FINE", -50, 50}, {"LEVEL", 0, 100},
                                      {"BOW", 0, 100},   {"PLUCK", 0, 5},   {"DECAY", 0, 100},
                                      {"BRIGHT", 0, 100}, {"FOLD", 0, 100},  {"STIFF", 0, 100},
                                      {"POS", 0, 100}};
static const ParamDef gparams[NGP] = {{"VOICING", 0, 1},  {"ROOT", 24, 72},  {"BPM", 40, 200},
                                      {"DRIFT", 0, 100},  {"CUTOFF", 0, 100}, {"MASTER", 0, 100},
                                      {"WIFI SYNC", 0, NSYNCMODE - 1}};
static const ParamDef lparams[NLP] = {{"RATE", 0, 6}, {"SHAPE", 0, NSH - 1}, {"TARGET", 0, NLT - 1}, {"DEPTH", 0, 100}};
static const ParamDef fparams[NFX] = {{"TIME", 0, 7},   {"FEEDBK", 0, 95}, {"MIX", 0, 100},
                                      {"SEND", 0, 100}, {"TIME", 0, 100},  {"TONE", 0, 100},
                                      {"RATE", 0, 7},   {"DUTY", 1, 9},    {"SMOOTH", 0, 100}};
static const float gate_div16[8] = {0.f, 8.f, 4.f, 2.f, 1.f, 0.5f, 1.3333f, 0.6667f};
static const char *const gate_names[8] = {"OFF", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"};
static const int pluck_beats[6] = {0, 1, 2, 4, 8, 16};
static const char *const pluck_names[6] = {"OFF", "1 BEAT", "2 BEATS", "1 BAR", "2 BARS", "4 BARS"};
static const char *const note_names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *const sync_names[NSYNCMODE] = {"OFF", "AUTO", "LEAD", "FOLLOW"};
static const float lfo_div16[7] = {64.f, 32.f, 16.f, 8.f, 4.f, 2.f, 1.f};
static const char *const lfo_rate_names[7] = {"4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16"};
static const char *const shape_names[NSH] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
static const char *const target_names[NLT] = {"CUTOFF", "LEVEL", "PITCH", "BOW", "DELAY", "REVERB"};
static const float dly_div16[8] = {1.f, 2.f, 3.f, 4.f, 6.f, 8.f, 12.f, 16.f};
static const char *const dly_names[8] = {"1/16", "1/8", "3/16", "1/4", "3/8", "1/2", "3/4", "1 BAR"};
/* {note, fine, level, bow, pluck, decay, bright, fold, stiff, pos} */
static const int def_sp[NS][NSP] = {{0, 0, 80, 35, 3, 85, 45, 0, 0, 0},
                                    {7, -4, 65, 45, 0, 92, 40, 0, 0, 0},
                                    {12, 3, 55, 40, 0, 95, 55, 0, 0, 0},
                                    {19, 0, 40, 50, 4, 90, 65, 0, 0, 0},
                                    {24, 2, 30, 55, 5, 93, 70, 0, 0, 0}};
static const int def_gp[NGP] = {MODE_CHORD, 45, 72, 30, 70, 80, SYNC_OFF};
static const int def_lfo[NLFO][NLP] = {{2, SH_SINE, LT_CUTOFF, 35}, {4, SH_TRI, LT_LEVEL, 0}};
static const int def_fx[NFX] = {5, 45, 25, 35, 60, 55, 0, 5, 40};

const ParamDef *sparam_def(int p) { return &sparams[p]; }
const ParamDef *gparam_def(int p) { return &gparams[p]; }
const ParamDef *lparam_def(int p) { return &lparams[p]; }
const ParamDef *fparam_def(int p) { return &fparams[p]; }
const char *sync_mode_name(int m) { return sync_names[m]; }

void note_name(int midi, char *b, int n) { snprintf(b, n, "%s%d", note_names[midi % 12], midi / 12 - 1); }

void sparam_format(int s, int p, char *b, int n)
{
    int x = song.sp[s][p];
    switch (p) {
    case SP_NOTE: {
        char nm[12];
        if (song.gp[GP_MODE] == MODE_MONO && s > 0) {
            snprintf(b, n, "= 1");
            return;
        }
        note_name(song.gp[GP_ROOT] + x, nm, sizeof nm);
        snprintf(b, n, "%+d %s", x, nm);
        return;
    }
    case SP_FINE: snprintf(b, n, "%+dc", x); return;
    case SP_PLUCK: snprintf(b, n, "%s", pluck_names[x]); return;
    case SP_FOLD:
    case SP_STIFF:
    case SP_POS:
        if (x == 0)
            snprintf(b, n, "OFF");
        else
            snprintf(b, n, "%d", x);
        return;
    default: snprintf(b, n, "%d", x); return;
    }
}

void gparam_format(int p, char *b, int n)
{
    int x = song.gp[p];
    switch (p) {
    case GP_MODE: snprintf(b, n, "%s", x == MODE_MONO ? "MONO" : "CHORD"); return;
    case GP_ROOT: note_name(x, b, n); return;
    case GP_SYNC: snprintf(b, n, "%s", sync_names[x]); return;
    default: snprintf(b, n, "%d", x); return;
    }
}

void lparam_format(int l, int p, char *b, int n)
{
    int x = song.lfo[l][p];
    switch (p) {
    case LP_RATE: snprintf(b, n, "%s", lfo_rate_names[x]); return;
    case LP_SHAPE: snprintf(b, n, "%s", shape_names[x]); return;
    case LP_TARGET: snprintf(b, n, "%s", target_names[x]); return;
    default: snprintf(b, n, "%d", x); return;
    }
}

void fparam_format(int p, char *b, int n)
{
    int x = song.fx[p];
    if (p == FX_DTIME)
        snprintf(b, n, "%s", dly_names[x]);
    else if (p == FX_GRATE)
        snprintf(b, n, "%s", gate_names[x]);
    else if (p == FX_GDUTY)
        snprintf(b, n, "%d%%", x * 10);
    else if (p == FX_DFB || p == FX_DMIX || p == FX_VSEND)
        snprintf(b, n, "%d%%", x);
    else
        snprintf(b, n, "%d", x);
}

/* ------------------------------------------------------------------ dsp */

static float sr;
static uint32_t rng = 0x9e3779b9u;
static inline uint32_t xr(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static inline float frand(void) { return (xr() >> 8) * (1.f / 16777216.f); }
static inline float fnoise(void) { return frand() * 2.f - 1.f; }

static inline float softclip(float x)
{
    if (x > 3.f)
        return 1.f;
    if (x < -3.f)
        return -1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

#define DL_MAX 8192
typedef struct {
    float buf[DL_MAX];
    int w;
    float lp;     /* loop damping state */
    float env;    /* loop amplitude follower, for the bow's energy balance */
    float bow_lp; /* bow noise colour */
    float gain;   /* on/off fade */
    float drift, drift_target, drift_t;
    float vis_ph;
    int burst; /* samples of pluck burst left */
    float burst_amp;
    float ap[2][2]; /* STIFF: two dispersive allpasses in the loop */
    /* per sub-block targets, ramped */
    float D, D_cur, lvl, lvl_cur;
} String;

static String str[NS];
static double beat_phase, sync_rate = 1.0;
static uint32_t beat;
static float bpm_f = 72.f;
static float master_g, play_g;
static float f_ic1[2], f_ic2[2];
static float lfo_rand[NLFO];
static int lfo_cycle[NLFO];
/* tempo delay: up to 4 s per channel */
#define DLY_MAX (1 << 18)
static float dly[2][DLY_MAX];
static uint32_t dly_w;
static float dly_time_cur, dly_lp[2];
static float gate_g = 1.f;
/* reverb: four damped combs + two allpasses per side */
#define NCOMB 4
#define COMB_MAX 2048
#define AP_MAX 1024
static const int comb_len44[NCOMB] = {1116, 1188, 1277, 1356};
static const int ap_len44[2] = {556, 441};
static float comb_buf[2][NCOMB][COMB_MAX], comb_lp[2][NCOMB];
static int comb_i[2][NCOMB];
static float ap_buf[2][2][AP_MAX];
static int ap_i[2][2];

static float midi_hz(float m) { return 440.f * exp2f((m - 69.f) / 12.f); }

static void string_pluck(String *s, float amp)
{
    s->burst = (int)(0.004f * sr);
    s->burst_amp = amp;
}

void engine_pluck(int s)
{
    if (s >= 0 && s < NS)
        string_pluck(&str[s], 0.9f);
}

void engine_set_playing(int on) { live.playing = on; }

float engine_bpm(void) { return bpm_f; }
void engine_set_bpm(float b)
{
    bpm_f = b < 40.f ? 40.f : b > 250.f ? 250.f : b;
    song.gp[GP_BPM] = (int)(bpm_f + 0.5f);
}

double engine_position(void) { return ((double)beat + beat_phase) * 4.0; }

void engine_set_position(double steps)
{
    if (steps < 0.0)
        steps = 0.0;
    beat = (uint32_t)(steps / 4.0);
    beat_phase = steps / 4.0 - beat;
    live.beat = beat;
}

void engine_set_rate(double r) { sync_rate = r < 0.9 ? 0.9 : r > 1.1 ? 1.1 : r; }

/* a new chord: notes from a minor pentatonic + major sixth, spread over
 * three octaves, low to high */
void engine_randomize(void)
{
    static const int scale[] = {0, 3, 5, 7, 9, 10};
    static const int octs[NS] = {-12, 0, 12, 12, 24};
    for (int s = 0; s < NS; s++) {
        int oct = octs[s];
        int note = oct + scale[xr() % 6] + (s == 3 && frand() < 0.5f ? 12 : 0);
        if (note > 24)
            note -= 12;
        song.sp[s][SP_NOTE] = note;
        song.sp[s][SP_FINE] = (int)(fnoise() * 8.f);
        string_pluck(&str[s], 0.9f);
    }
}

static void reverb_tick(float in, float *outL, float *outR, float decay, float tone)
{
    float scale = sr / 44100.f;
    float fb = 0.72f + 0.26f * decay, damp = 0.85f - 0.75f * tone;
    for (int ch = 0; ch < 2; ch++) {
        float acc = 0.f, x = in * 0.05f;
        for (int c = 0; c < NCOMB; c++) {
            int len = (int)((comb_len44[c] + ch * 23) * scale);
            if (len > COMB_MAX)
                len = COMB_MAX;
            float *b = comb_buf[ch][c];
            int i = comb_i[ch][c];
            float o = b[i];
            comb_lp[ch][c] = o * (1.f - damp) + comb_lp[ch][c] * damp;
            b[i] = x + comb_lp[ch][c] * fb;
            comb_i[ch][c] = i + 1 < len ? i + 1 : 0;
            acc += o;
        }
        for (int a = 0; a < 2; a++) {
            int len = (int)((ap_len44[a] + ch * 23) * scale);
            if (len > AP_MAX)
                len = AP_MAX;
            float *b = ap_buf[ch][a];
            int i = ap_i[ch][a];
            float bo = b[i];
            float o = -acc + bo;
            b[i] = acc + bo * 0.5f;
            ap_i[ch][a] = i + 1 < len ? i + 1 : 0;
            acc = o;
        }
        if (ch)
            *outR = acc;
        else
            *outL = acc;
    }
}

/* LFO value 0..1 from the beat position */
static float lfo_value(int l, double pos16)
{
    const int *P = song.lfo[l];
    double cyc = pos16 / lfo_div16[P[LP_RATE]];
    int n = (int)cyc;
    float ph = (float)(cyc - n);
    if (n != lfo_cycle[l]) {
        lfo_cycle[l] = n;
        lfo_rand[l] = frand();
    }
    switch (P[LP_SHAPE]) {
    case SH_SINE: return 0.5f - 0.5f * cosf(ph * 6.2831853f);
    case SH_TRI: return ph < 0.5f ? ph * 2.f : 2.f - ph * 2.f;
    case SH_SAW: return 1.f - ph;
    case SH_SQUARE: return ph < 0.5f ? 1.f : 0.f;
    default: return lfo_rand[l];
    }
}

#define SUB 64

void engine_render(int16_t *out, int frames)
{
    float drift_amt = song.gp[GP_DRIFT] / 100.f;
    float verb_send = song.fx[FX_VSEND] / 100.f, verb_decay = song.fx[FX_VTIME] / 100.f,
          verb_tone = song.fx[FX_VTONE] / 100.f;
    float master_t = song.gp[GP_MASTER] / 100.f;
    master_t *= master_t * 1.3f;
    float play_t = live.playing ? 1.f : 0.f;
    float g_ramp = 1.f / (0.02f * sr);
    float meter[NS] = {0}, mm = 0.f;
    int mono = song.gp[GP_MODE] == MODE_MONO;
    float sps16 = sr * 15.f / bpm_f; /* samples per 16th */
    float dly_target = fminf(sps16 * dly_div16[song.fx[FX_DTIME]], DLY_MAX - 4.f);
    float dly_fb = song.fx[FX_DFB] / 100.f, dly_mix_p = song.fx[FX_DMIX] / 100.f;
    if (dly_time_cur <= 0.f)
        dly_time_cur = dly_target;
    /* tempo gate: a square on the beat grid, smoothed into a swell */
    float gate_per = gate_div16[song.fx[FX_GRATE]], gate_duty = song.fx[FX_GDUTY] * 0.1f;
    float gate_k = 1.f - expf(-1.f / ((0.0005f + 0.06f * song.fx[FX_GSMOOTH] / 100.f) * sr));

    /* static per-string settings for this block */
    float lp_k[NS], rho[NS], bow_amp[NS], bow_col[NS], base_midi[NS], fold_g[NS], stiff_a[NS], pos_f[NS];
    int pluck_every[NS];
    for (int s = 0; s < NS; s++) {
        const int *P = song.sp[s];
        String *st = &str[s];
        st->drift_t -= frames;
        if (st->drift_t <= 0.f) {
            st->drift_t = (1.f + 2.f * frand()) * sr;
            st->drift_target = fnoise();
        }
        st->drift += (st->drift_target - st->drift) * 0.002f * frames / 64.f;
        float cents = P[SP_FINE] + st->drift * 25.f * drift_amt;
        int note = mono ? song.sp[0][SP_NOTE] : P[SP_NOTE];
        base_midi[s] = song.gp[GP_ROOT] + note + cents / 100.f;
        lp_k[s] = 0.08f + 0.9f * powf(P[SP_BRIGHT] / 100.f, 1.5f);
        float d = P[SP_DECAY] / 100.f;
        rho[s] = 0.85f + 0.14995f * powf(d, 0.35f);
        bow_amp[s] = P[SP_BOW] / 100.f * 0.02f;
        bow_col[s] = 0.05f + 0.5f * P[SP_BRIGHT] / 100.f;
        pluck_every[s] = pluck_beats[P[SP_PLUCK]];
        /* tone shapers: FOLD drives the output into a wavefolder, STIFF
         * sets the allpass dispersion in the loop (inharmonic, bell-like),
         * POS is the pick-position comb (0 = off, else 1 .. 50% along) */
        fold_g[s] = P[SP_FOLD] / 100.f;
        stiff_a[s] = P[SP_STIFF] / 100.f * 0.7f;
        pos_f[s] = P[SP_POS] ? 0.005f + 0.495f * P[SP_POS] / 100.f : 0.f;
    }

    for (int n0 = 0; n0 < frames; n0 += SUB) {
        int nn = frames - n0 < SUB ? frames - n0 : SUB;
        /* modulation, once per sub-block */
        double pos16 = engine_position();
        float m_cut = 0.f, m_lvl = 1.f, m_pitch = 0.f, m_bow = 1.f, m_dly = 1.f, m_verb = 1.f;
        for (int l = 0; l < NLFO; l++) {
            float v = lfo_value(l, pos16), depth = song.lfo[l][LP_DEPTH] / 100.f;
            live.lfo_val[l] = v;
            switch (song.lfo[l][LP_TARGET]) {
            case LT_CUTOFF: m_cut += depth * 3.f * (v - 0.5f) * 2.f; break; /* octaves */
            case LT_LEVEL: m_lvl *= 1.f - depth * (1.f - v); break;
            case LT_PITCH: m_pitch += depth * (v - 0.5f) * 2.f; break; /* semitones */
            case LT_BOW: m_bow *= 1.f - depth * (1.f - v); break;
            case LT_DELAY: m_dly *= 1.f - depth * (1.f - v); break;
            case LT_VERB: m_verb *= 1.f - depth * (1.f - v); break;
            }
        }
        float cut = 60.f * powf(300.f, song.gp[GP_CUTOFF] / 100.f) * exp2f(m_cut);
        float fg = tanf(3.14159265f * fminf(fmaxf(cut, 30.f), 0.45f * sr) / sr), fk = 1.2f;
        float fa1 = 1.f / (1.f + fg * (fg + fk)), fa2 = fg * fa1, fa3 = fg * fa2;
        float dly_mix = dly_mix_p * m_dly, vsend = verb_send * m_verb;
        for (int s = 0; s < NS; s++) {
            String *st = &str[s];
            float hz = midi_hz(base_midi[s] + m_pitch);
            hz = fmaxf(20.f, fminf(hz, sr / 8.f));
            float lp_delay = (1.f - lp_k[s]) / lp_k[s];
            if (lp_delay > 6.f)
                lp_delay = 6.f;
            float ap_delay = stiff_a[s] > 0.f ? 2.f * (1.f - stiff_a[s]) / (1.f + stiff_a[s]) : 0.f;
            st->D = fmaxf(2.f, fminf(sr / hz - lp_delay - ap_delay, DL_MAX - 3.f));
            if (st->D_cur <= 0.f)
                st->D_cur = st->D;
            float l = song.sp[s][SP_LEVEL] / 100.f;
            st->lvl = l * l * m_lvl;
        }
        float dD[NS], dl[NS];
        for (int s = 0; s < NS; s++) {
            dD[s] = (str[s].D - str[s].D_cur) / nn;
            dl[s] = (str[s].lvl - str[s].lvl_cur) / nn;
        }

        for (int n = n0; n < n0 + nn; n++) {
            if (live.playing) {
                beat_phase += bpm_f / 60.f / sr * sync_rate;
                if (beat_phase >= 1.0) {
                    beat_phase -= 1.0;
                    beat++;
                    live.beat = beat;
                    for (int s = 0; s < NS; s++)
                        if (song.on[s] && pluck_every[s] && beat % (uint32_t)pluck_every[s] == 0)
                            string_pluck(&str[s], 0.8f);
                }
            }
            play_g += (play_t - play_g) * g_ramp;

            float L = 0.f, R = 0.f, send = 0.f;
            for (int s = 0; s < NS; s++) {
                String *st = &str[s];
                float on_t = song.on[s] ? 1.f : 0.f;
                st->gain += (on_t - st->gain) * g_ramp;
                st->D_cur += dD[s];
                st->lvl_cur += dl[s];
                if (st->gain < 1e-4f && !song.on[s])
                    continue;

                /* fractional read D samples back (linear interpolation so
                 * pitch modulation glides instead of stepping) */
                float rp = st->w - st->D_cur;
                if (rp < 0.f)
                    rp += DL_MAX;
                int ri = (int)rp;
                float fr = rp - ri;
                int rj = ri + 1 < DL_MAX ? ri + 1 : 0;
                float y = st->buf[ri] + (st->buf[rj] - st->buf[ri]) * fr;
                float o = y;
                if (pos_f[s] > 0.f) {
                    /* comb with a tap further back: the pick-position filter */
                    float rq = rp - st->D_cur * pos_f[s];
                    if (rq < 0.f)
                        rq += DL_MAX;
                    int qi = (int)rq, qj = qi + 1 < DL_MAX ? qi + 1 : 0;
                    o = y - (st->buf[qi] + (st->buf[qj] - st->buf[qi]) * (rq - qi));
                }
                if (fold_g[s] > 0.f) {
                    float g = 1.f + fold_g[s] * 5.f, f = o * g;
                    /* triangle fold back into -1..1 */
                    f = fmodf(f + 1.f, 4.f);
                    if (f < 0.f)
                        f += 4.f;
                    f = f < 2.f ? f - 1.f : 3.f - f;
                    o = f * (0.8f / sqrtf(g));
                }
                float x = y;
                if (stiff_a[s] > 0.f)
                    for (int k = 0; k < 2; k++) {
                        float *st2 = st->ap[k];
                        float yk = stiff_a[s] * x + st2[0] - stiff_a[s] * st2[1];
                        st2[0] = x;
                        st2[1] = yk;
                        x = yk;
                    }
                st->lp += (x - st->lp) * lp_k[s];
                float fbk = st->lp * rho[s];

                st->env = fmaxf(fabsf(y), st->env * 0.9997f);
                float headroom = fmaxf(0.f, 1.f - st->env * 1.6f);
                st->bow_lp += (fnoise() - st->bow_lp) * bow_col[s];
                float ex = st->bow_lp * bow_amp[s] * m_bow * headroom * play_g;
                if (st->burst > 0) {
                    st->burst--;
                    ex += fnoise() * st->burst_amp * play_g;
                }
                st->buf[st->w] = softclip(fbk + ex);
                st->w = st->w + 1 < DL_MAX ? st->w + 1 : 0;

                o *= st->lvl_cur * st->gain * play_g;
                float pan = (s - (NS - 1) * 0.5f) * 0.25f;
                L += o * (1.f - pan * 0.5f);
                R += o * (1.f + pan * 0.5f);
                send += o;
                float a = fabsf(y);
                if (a > meter[s])
                    meter[s] = a;
                st->vis_ph += 0.6f / st->D_cur;
            }

            /* rhythmic gate before the effects, so the delay and reverb
             * smear the chops */
            if (gate_per > 0.f) {
                float gp = (float)fmod(engine_position(), (double)gate_per) / gate_per;
                gate_g += ((gp < gate_duty ? 1.f : 0.f) - gate_g) * gate_k;
            }
            else
                gate_g += (1.f - gate_g) * gate_k;
            L *= gate_g;
            R *= gate_g;

            /* master lowpass: 2-pole SVF with a little resonance */
            for (int ch = 0; ch < 2; ch++) {
                float x = ch ? R : L;
                float v3 = x - f_ic2[ch];
                float v1 = fa1 * f_ic1[ch] + fa2 * v3;
                float v2 = f_ic2[ch] + fa2 * f_ic1[ch] + fa3 * v3;
                f_ic1[ch] = 2.f * v1 - f_ic1[ch];
                f_ic2[ch] = 2.f * v2 - f_ic2[ch];
                if (ch)
                    R = v2;
                else
                    L = v2;
            }

            /* tempo delay, time slewed so tempo changes glide */
            dly_time_cur += (dly_target - dly_time_cur) * 0.0005f;
            float rp = (float)dly_w - dly_time_cur;
            if (rp < 0.f)
                rp += DLY_MAX;
            uint32_t ri = (uint32_t)rp;
            float fr = rp - ri;
            float dl0 = dly[0][ri & (DLY_MAX - 1)] + (dly[0][(ri + 1) & (DLY_MAX - 1)] - dly[0][ri & (DLY_MAX - 1)]) * fr;
            float dr0 = dly[1][ri & (DLY_MAX - 1)] + (dly[1][(ri + 1) & (DLY_MAX - 1)] - dly[1][ri & (DLY_MAX - 1)]) * fr;
            dly_lp[0] += (dl0 - dly_lp[0]) * 0.5f;
            dly_lp[1] += (dr0 - dly_lp[1]) * 0.5f;
            dly[0][dly_w & (DLY_MAX - 1)] = softclip(L + dly_lp[1] * dly_fb); /* ping-pong */
            dly[1][dly_w & (DLY_MAX - 1)] = softclip(R + dly_lp[0] * dly_fb);
            dly_w++;
            L += dl0 * dly_mix;
            R += dr0 * dly_mix;

            float wl = 0.f, wr = 0.f;
            reverb_tick((send + (dl0 + dr0) * 0.5f * dly_mix) * vsend, &wl, &wr, verb_decay, verb_tone);
            L += wl;
            R += wr;

            master_g += (master_t - master_g) * g_ramp;
            L = softclip(L * master_g);
            R = softclip(R * master_g);
            float a = fmaxf(fabsf(L), fabsf(R));
            if (a > mm)
                mm = a;
            out[2 * n] = (int16_t)(L * 32000.f);
            out[2 * n + 1] = (int16_t)(R * 32000.f);
        }
        for (int s = 0; s < NS; s++) {
            str[s].D_cur = str[s].D;
            str[s].lvl_cur = str[s].lvl;
        }
    }
    for (int s = 0; s < NS; s++) {
        live.meter[s] = meter[s];
        live.phase[s] = str[s].vis_ph;
    }
    live.master_meter = mm;
    live.gate = gate_g;
}

void engine_init(int sample_rate)
{
    sr = (float)sample_rate;
    memset(str, 0, sizeof str);
    bpm_f = (float)song.gp[GP_BPM];
    beat = 0;
    beat_phase = 0.0;
    dly_time_cur = 0.f;
}

/* ------------------------------------------------------------ song state */

static int clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }

static void fill_default(Song *s)
{
    memset(s, 0, sizeof *s);
    s->magic = SONG_MAGIC;
    for (int i = 0; i < NS; i++) {
        s->on[i] = 1;
        memcpy(s->sp[i], def_sp[i], sizeof s->sp[i]);
    }
    memcpy(s->gp, def_gp, sizeof s->gp);
    memcpy(s->lfo, def_lfo, sizeof s->lfo);
    memcpy(s->fx, def_fx, sizeof s->fx);
}

void song_default(void)
{
    fill_default(&song);
    bpm_f = (float)song.gp[GP_BPM];
}

/* a four-string save: strings 1-4 as saved, string 5 from the defaults */
static int load_v3(Song *s, const unsigned char *buf, size_t got)
{
    SongV3 o;
    if (got != sizeof o)
        return 0;
    memcpy(&o, buf, sizeof o);
    fill_default(s);
    for (int i = 0; i < NS_V3; i++) {
        s->on[i] = o.on[i];
        memcpy(s->sp[i], o.sp[i], sizeof s->sp[i]);
    }
    memcpy(s->gp, o.gp, sizeof s->gp);
    memcpy(s->lfo, o.lfo, sizeof s->lfo);
    memcpy(s->fx, o.fx, sizeof s->fx);
    return 1;
}

int song_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    Song s;
    unsigned char buf[sizeof s + 1];
    size_t got = fread(buf, 1, sizeof buf, f);
    fclose(f);
    uint32_t magic = 0;
    if (got >= sizeof magic)
        memcpy(&magic, buf, sizeof magic);
    if (magic == SONG_MAGIC && got == sizeof s)
        memcpy(&s, buf, sizeof s);
    else if (magic == SONG_MAGIC_V3) {
        if (!load_v3(&s, buf, got))
            return 0;
    }
    else
        return 0;
    for (int i = 0; i < NS; i++) {
        s.on[i] = !!s.on[i];
        for (int p = 0; p < NSP; p++)
            s.sp[i][p] = clampi(s.sp[i][p], sparams[p].min, sparams[p].max);
    }
    for (int p = 0; p < NGP; p++)
        s.gp[p] = clampi(s.gp[p], gparams[p].min, gparams[p].max);
    for (int l = 0; l < NLFO; l++)
        for (int p = 0; p < NLP; p++)
            s.lfo[l][p] = clampi(s.lfo[l][p], lparams[p].min, lparams[p].max);
    for (int p = 0; p < NFX; p++)
        s.fx[p] = clampi(s.fx[p], fparams[p].min, fparams[p].max);
    song = s;
    bpm_f = (float)song.gp[GP_BPM];
    return 1;
}

int song_save(const char *path)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f)
        return 0;
    int ok = fwrite(&song, sizeof song, 1, f) == 1;
    ok &= fclose(f) == 0;
    return ok && rename(tmp, path) == 0;
}
