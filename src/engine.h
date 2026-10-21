/* Jaffedrone audio engine: five Karplus-Strong strings kept ringing by a
 * noise bow, re-plucked on a tempo grid, in MONO (unison) or CHORD voicing,
 * with two beat-synced LFOs, a tempo-synced delay, a reverb, and Wi-Fi
 * tempo sync compatible with HI5. */
#pragma once
#include <stddef.h>
#include <stdint.h>

#define JD_VERSION "0.0.4"

#define NS 5
#define NSP 10 /* params per string */
#define NGP 8  /* global params */
#define NLFO 2
#define NLP 4  /* params per LFO */
#define NFX 9  /* effect params */

enum { SP_NOTE, SP_FINE, SP_LEVEL, SP_BOW, SP_PLUCK, SP_DECAY, SP_BRIGHT, SP_FOLD, SP_STIFF, SP_POS };
enum { GP_MODE, GP_ROOT, GP_BPM, GP_DRIFT, GP_GLIDE, GP_CUTOFF, GP_MASTER, GP_SYNC };
enum { LP_RATE, LP_SHAPE, LP_TARGET, LP_DEPTH };
enum { FX_DTIME, FX_DFB, FX_DMIX, FX_VSEND, FX_VTIME, FX_VTONE, FX_GRATE, FX_GDUTY, FX_GSMOOTH };
enum { MODE_MONO, MODE_CHORD };
enum { SYNC_OFF, SYNC_AUTO, SYNC_LEAD, SYNC_FOLLOW, NSYNCMODE };
enum { LT_CUTOFF, LT_LEVEL, LT_PITCH, LT_BOW, LT_DELAY, LT_VERB, NLT };
enum { SH_SINE, SH_TRI, SH_SAW, SH_SQUARE, SH_RANDOM, NSH };

typedef struct {
    const char *name;
    int min, max;
} ParamDef;

typedef struct {
    uint32_t magic;
    int on[NS];
    int sp[NS][NSP];
    int gp[NGP];
    int lfo[NLFO][NLP];
    int fx[NFX];
} Song;

/* shared with the UI; the audio thread publishes meters and the beat */
typedef struct {
    int playing;
    volatile float meter[NS];
    volatile float master_meter;
    volatile uint32_t beat;
    volatile float phase[NS];  /* string animation phase */
    volatile float lfo_val[NLFO]; /* 0..1 for the display */
    volatile float gate;          /* current gate gain for the display */
} Live;

extern Song song;
extern Live live;

const ParamDef *sparam_def(int p);
const ParamDef *gparam_def(int p);
const ParamDef *lparam_def(int p);
const ParamDef *fparam_def(int p);
void sparam_format(int s, int p, char *b, int n);
void gparam_format(int p, char *b, int n);
void lparam_format(int l, int p, char *b, int n);
void fparam_format(int p, char *b, int n);
void note_name(int midi, char *b, int n);
const char *sync_mode_name(int m);

void engine_init(int sample_rate);
void engine_render(int16_t *out, int frames);
void engine_set_playing(int on);
void engine_pluck(int s);
void engine_randomize(void); /* new chord for the strings */

/* transport for wifi sync: position in 16th steps with fraction, a hard
 * jump, a playback speed trim (1.0 = none) and a float tempo */
double engine_position(void);
void engine_set_position(double steps);
void engine_set_rate(double r);
float engine_bpm(void);
void engine_set_bpm(float bpm);

void song_default(void);
int song_save(const char *path);
int song_load(const char *path);
