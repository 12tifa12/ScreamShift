// ============================================================================
//  SCREAM SHIFT  -  DMET B402 Computer Graphics, Assignment 1 (Winter 2026)
//  A 2D side-view Scare Floor game written with legacy OpenGL + GLUT.
//
//  Player : a round green one-eyed monster (one big eye, two little horns)
//  Rival  : a huge teal monster with purple spots who steals canisters
//
//  Controls
//    A / D  or  LEFT / RIGHT  : move           W / SPACE / UP : jump
//    Mouse hover + left click : Door Station   P              : pause
//    R (end screen)           : new shift      ESC            : quit
//
//  Fonts: every piece of text uses the GLUT stroke font, rendered as a
//  "candy" style: drop shadow + thick dark outline + two-tone fill + shine.
//
//  Sound (bonus): all music / effects are synthesised in code at start-up and
//  played through the Windows waveOut API, so no external sound files needed.
// ============================================================================

#define _USE_MATH_DEFINES
#define _CRT_SECURE_NO_WARNINGS          // lets sprintf compile in Visual Studio
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <string>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#if defined(__has_include)
#if __has_include("glut.h")
#include "glut.h"
#else
#include <GL/glut.h>
#endif
#else
#include <glut.h>
#endif

// ============================================================================
//  Constants
// ============================================================================
static const float PI = 3.14159265f;
static const float W = 1000.f, H = 700.f;      // logical scene size (glOrtho)
static const float FLOOR_Y = 90.f;              // top of the Scare Floor
static const float RAIL_Y0 = 618.f, RAIL_Y1 = 636.f;
static const float HUD_Y0 = 652.f;
static const float DOOR_W = 70.f, DOOR_H = 106.f, DOOR_TOP = 610.f;
static const float SHIFT_TIME = 90.f;           // seconds in a shift
static const int   QUOTA = 300;                 // scream points needed
static const int   MAX_LIVES = 5;
static const float STATION_COOLDOWN = 10.f;
static const float WARN_T = 0.8f, OPEN_T = 0.3f, HOLD_T = 0.35f;
static const float POWER_TIME = 6.f;
static const float WALL_R = 0.17f, WALL_G = 0.19f, WALL_B = 0.27f;

// ============================================================================
//  Small helpers
// ============================================================================
static float frand(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

static int winW = 1000, winH = 700;

// Camouflage factor used by col(): 0 = normal colours, 1 = blends into wall.
static float gCamo = 0.f;
static void col(float r, float g, float b, float a = 1.f) {
    if (gCamo > 0.f) {
        r = lerpf(r, WALL_R, gCamo); g = lerpf(g, WALL_G, gCamo); b = lerpf(b, WALL_B, gCamo);
        a *= 1.f - 0.85f * gCamo;
    }
    glColor4f(r, g, b, a);
}

// --- primitive wrappers (each one names the GL primitive it uses) ----------
static void ellipse(float cx, float cy, float rx, float ry, int seg = 32) {      // GL_POLYGON
    glBegin(GL_POLYGON);
    for (int i = 0; i < seg; i++) { float a = 2 * PI * i / seg; glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a)); }
    glEnd();
}
static void ellipseLine(float cx, float cy, float rx, float ry, int seg = 32) {  // GL_LINE_LOOP
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; i++) { float a = 2 * PI * i / seg; glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a)); }
    glEnd();
}
static void quad(float x0, float y0, float x1, float y1) {                         // GL_QUADS
    glBegin(GL_QUADS); glVertex2f(x0, y0); glVertex2f(x1, y0); glVertex2f(x1, y1); glVertex2f(x0, y1); glEnd();
}
static void quadGrad(float x0, float y0, float x1, float y1, const float* cb, const float* ct, float a = 1.f) {
    glBegin(GL_QUADS);
    glColor4f(cb[0], cb[1], cb[2], a); glVertex2f(x0, y0); glVertex2f(x1, y0);
    glColor4f(ct[0], ct[1], ct[2], a); glVertex2f(x1, y1); glVertex2f(x0, y1);
    glEnd();
}
static void tri(float x0, float y0, float x1, float y1, float x2, float y2) {      // GL_TRIANGLES
    glBegin(GL_TRIANGLES); glVertex2f(x0, y0); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
}
static void seg(float x0, float y0, float x1, float y1) {                          // GL_LINES
    glBegin(GL_LINES); glVertex2f(x0, y0); glVertex2f(x1, y1); glEnd();
}
static void roundRect(float x0, float y0, float x1, float y1, float r) {           // GL_POLYGON
    glBegin(GL_POLYGON);
    float cx[4] = { x1 - r, x0 + r, x0 + r, x1 - r }, cy[4] = { y1 - r, y1 - r, y0 + r, y0 + r };
    for (int c = 0; c < 4; c++)
        for (int i = 0; i <= 6; i++) { float a = (c * 90 + i * 15) * PI / 180; glVertex2f(cx[c] + r * cosf(a), cy[c] + r * sinf(a)); }
    glEnd();
}
static void roundRectLine(float x0, float y0, float x1, float y1, float r) {       // GL_LINE_LOOP
    glBegin(GL_LINE_LOOP);
    float cx[4] = { x1 - r, x0 + r, x0 + r, x1 - r }, cy[4] = { y1 - r, y1 - r, y0 + r, y0 + r };
    for (int c = 0; c < 4; c++)
        for (int i = 0; i <= 6; i++) { float a = (c * 90 + i * 15) * PI / 180; glVertex2f(cx[c] + r * cosf(a), cy[c] + r * sinf(a)); }
    glEnd();
}

// ============================================================================
//  FANCY TEXT  ("candy" stroke font)
//  The GLUT stroke font is a set of thin line strips. We make it chunky by
//  "stamping" each glyph many times on a small disc of offsets with a thick
//  line width.  Layers: shadow -> dark outline -> bottom fill -> top fill
//  (scissored to the upper half) -> white shine (scissored to the very top).
// ============================================================================
static float gLineMax = 1.f;
static void* FONT = GLUT_STROKE_ROMAN;

struct TextFx {
    float size;            // cap height in scene units
    float top[3], bot[3], out[3];
    float shear;           // italic slant
    float tracking;        // extra letter spacing (fraction of size)
    float wave;            // per-letter bounce amplitude
    float weight;          // stroke thickness multiplier
    float alpha;
    bool  shadow;
};
enum { PAL_GOLD, PAL_WHITE, PAL_GREEN, PAL_TEAL, PAL_RED, PAL_PURPLE, PAL_BLUE };
static TextFx mkText(float size, int pal) {
    static const float PALS[7][9] = {
        {1.00f,0.95f,0.50f, 1.00f,0.60f,0.10f, 0.20f,0.05f,0.25f},  // gold
        {1.00f,1.00f,1.00f, 0.68f,0.84f,1.00f, 0.07f,0.09f,0.24f},  // white/ice
        {0.85f,1.00f,0.55f, 0.42f,0.82f,0.18f, 0.04f,0.18f,0.07f},  // green
        {0.70f,1.00f,1.00f, 0.22f,0.74f,0.86f, 0.12f,0.06f,0.28f},  // teal
        {1.00f,0.78f,0.70f, 1.00f,0.25f,0.25f, 0.25f,0.02f,0.06f},  // red
        {0.92f,0.80f,1.00f, 0.62f,0.42f,0.95f, 0.12f,0.03f,0.20f},  // purple
        {0.75f,0.92f,1.00f, 0.25f,0.58f,1.00f, 0.04f,0.08f,0.25f},  // blue
    };
    TextFx f;
    f.size = size;
    for (int i = 0; i < 3; i++) { f.top[i] = PALS[pal][i]; f.bot[i] = PALS[pal][3 + i]; f.out[i] = PALS[pal][6 + i]; }
    f.shear = 0.16f; f.tracking = 0.08f; f.wave = 0.f; f.weight = 1.f; f.alpha = 1.f; f.shadow = true;
    return f;
}

static float textWidth(const char* s, const TextFx& f) {
    float sc = f.size / 119.05f, w = 0; int n = (int)strlen(s);
    for (int i = 0; i < n; i++) w += glutStrokeWidth(FONT, s[i]) * sc + f.tracking * f.size;
    if (n) w -= f.tracking * f.size;
    return w;
}

// Draw all glyphs once for every offset in a disc of radius R.
static void stampPass(const char* s, const std::vector<float>& px, const std::vector<float>& py,
                      float sc, float shear, float R, float ox, float oy, float clipFrac, float size) {
    float lw = clampf(R * 2.f, 1.f, gLineMax);
    glLineWidth(lw);
    std::vector<float> offs; offs.push_back(0); offs.push_back(0);
    float inner = R - lw * 0.5f, step = std::max(0.6f, lw * 0.7f);
    for (float r = inner; r > 0.05f; r -= step) {
        int k = std::max(6, (int)ceilf(2 * PI * r / step));
        for (int j = 0; j < k; j++) { float a = 2 * PI * j / k; offs.push_back(r * cosf(a)); offs.push_back(r * sinf(a)); }
    }
    GLfloat m[16] = { 1,0,0,0, shear,1,0,0, 0,0,1,0, 0,0,0,1 };
    int n = (int)strlen(s);
    for (int i = 0; i < n; i++) {
        if (s[i] == ' ') continue;
        if (clipFrac > 0.f) {   // only keep pixels above a horizontal line (two-tone fill)
            int sy = (int)((py[i] + oy + clipFrac * size) * winH / H);
            glEnable(GL_SCISSOR_TEST); glScissor(0, sy, winW, winH);
        }
        for (size_t o = 0; o < offs.size(); o += 2) {
            glPushMatrix();
            glTranslatef(px[i] + ox + offs[o], py[i] + oy + offs[o + 1], 0);
            glMultMatrixf(m);
            glScalef(sc, sc, 1);
            glutStrokeCharacter(FONT, s[i]);
            glPopMatrix();
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glLineWidth(1);
}

// align: 0 = left, 1 = centre, 2 = right.  t drives the per-letter wave.
static void drawText(const char* s, float x, float y, const TextFx& f, int align = 0, float t = 0.f) {
    int n = (int)strlen(s); if (!n) return;
    float sc = f.size / 119.05f, w = textWidth(s, f);
    float sx = x - (align == 1 ? w * 0.5f : align == 2 ? w : 0.f);
    std::vector<float> px(n), py(n);
    float acc = 0;
    for (int i = 0; i < n; i++) {
        px[i] = sx + acc;
        py[i] = y + (f.wave > 0 ? sinf(t * 5.f + i * 0.55f) * f.wave : 0.f);
        acc += glutStrokeWidth(FONT, s[i]) * sc + f.tracking * f.size;
    }
    float fillR = std::max(0.6f, f.size * 0.045f * f.weight);
    float outR = fillR + std::max(1.2f, f.size * 0.042f);
    if (f.shadow) {
        float d = 1.f + f.size * 0.05f;
        glColor4f(0, 0, 0, 0.45f * f.alpha);
        stampPass(s, px, py, sc, f.shear, outR, d, -d, 0, f.size);
    }
    glColor4f(f.out[0], f.out[1], f.out[2], f.alpha);
    stampPass(s, px, py, sc, f.shear, outR, 0, 0, 0, f.size);
    glColor4f(f.bot[0], f.bot[1], f.bot[2], f.alpha);
    stampPass(s, px, py, sc, f.shear, fillR, 0, 0, 0, f.size);
    glColor4f(f.top[0], f.top[1], f.top[2], f.alpha);
    stampPass(s, px, py, sc, f.shear, fillR, 0, 0, 0.48f, f.size);
    if (f.size >= 14) {
        glColor4f(1, 1, 1, 0.5f * f.alpha);
        stampPass(s, px, py, sc, f.shear, fillR * 0.35f, 0, fillR * 0.3f, 0.80f, f.size);
    }
}

// ============================================================================
//  SOUND (bonus) - synthesised at start-up, played with waveOut (Windows only)
// ============================================================================
#ifdef _WIN32
namespace Audio {
    const int RATE = 22050;
    typedef std::vector<short> Clip;
    Clip music, sCatch, sPower, sAlarm, sSteal, sQuota, sLose;
    HWAVEOUT musicOut = 0; WAVEHDR musicHdr;
    const int NCH = 4; HWAVEOUT chOut[NCH]; WAVEHDR chHdr[NCH]; int nextCh = 0;
    bool ok = false;

    float midiHz(float m) { return 440.f * powf(2.f, (m - 69.f) / 12.f); }

    // wave: 0 sine, 1 square, 2 triangle, 3 saw, 4 noise
    void note(std::vector<float>& b, float t0, float dur, float f0, float f1, float vol, int wave,
              float decay = 2.f, float rel = 0.04f, float vib = 0.f) {
        int i0 = (int)(t0 * RATE), n = (int)((dur + rel) * RATE);
        float ph = 0;
        for (int k = 0; k < n; k++) {
            int idx = i0 + k; if (idx < 0) continue; if (idx >= (int)b.size()) break;
            float t = k / (float)RATE;
            float f = f0 + (f1 - f0) * std::min(1.f, t / dur);
            if (vib > 0) f *= 1.f + vib * sinf(2 * PI * 5.5f * t);
            ph += f / RATE; ph -= floorf(ph);
            float v;
            switch (wave) {
            case 0: v = sinf(2 * PI * ph); break;
            case 1: v = ph < 0.5f ? 0.8f : -0.8f; break;
            case 2: v = 4.f * fabsf(ph - 0.5f) - 1.f; break;
            case 3: v = 2.f * ph - 1.f; break;
            default: v = frand(-1, 1); break;
            }
            float env = std::min(1.f, t / 0.004f) * expf(-decay * t);
            if (t > dur) env *= 1.f - (t - dur) / rel;
            b[idx] += v * vol * env;
        }
    }
    Clip toPCM(std::vector<float>& b, float lp = 1.f) {
        float y = 0, peak = 0.0001f;
        for (size_t i = 0; i < b.size(); i++) { y += lp * (b[i] - y); b[i] = y; peak = std::max(peak, fabsf(y)); }
        float g = peak > 0.95f ? 0.95f / peak : 1.f;
        Clip c(b.size());
        for (size_t i = 0; i < b.size(); i++) c[i] = (short)(clampf(b[i] * g, -1, 1) * 32000);
        return c;
    }
    std::vector<float> buf(float sec) { return std::vector<float>((size_t)(sec * RATE), 0.f); }

    void build() {
        // ---- background music: swingy walking-bass loop in D minor --------
        const float beat = 60.f / 132.f; const int BEATS = 32;
        std::vector<float> m = buf(beat * BEATS);
        int bass[BEATS] = { 38,41,45,48, 43,47,50,47, 48,45,43,40, 45,49,52,49,
                            38,40,41,45, 46,45,44,43, 45,40,43,49, 38,45,38,37 };
        for (int b = 0; b < BEATS; b++) {
            note(m, b * beat, beat * 0.85f, midiHz(bass[b] + 12.f), midiHz(bass[b] + 12.f), 0.30f, 2, 2.5f);
            if (b % 2 == 0) note(m, b * beat, 0.12f, 120, 45, 0.45f, 0, 16.f);            // kick
            else            note(m, b * beat, 0.09f, 0, 0, 0.10f, 4, 30.f);               // brush snare
            note(m, (b + 0.66f) * beat, 0.03f, 0, 0, 0.05f, 4, 60.f);                      // swung hat
        }
        struct N { float b, l; int p; };
        N mel[] = {
            {0,.5f,74},{.5f,.5f,72},{1,.5f,69},{2,.5f,65},{2.5f,1,69},
            {4,.5f,71},{4.5f,.5f,74},{5,1,77},{6.5f,.5f,74},{7,.5f,71},
            {8,1,76},{9,.5f,72},{9.5f,.5f,67},{10,1,64},{11,.5f,67},
            {12,.5f,73},{12.5f,.5f,76},{13,.5f,79},{13.5f,.5f,76},{14,1.5f,73},
            {16,.5f,74},{16.5f,.5f,72},{17,.5f,69},{18,.5f,65},{18.5f,1,69},
            {20,.5f,74},{20.5f,.5f,77},{21,1,82},{22,.5f,80},{22.5f,.5f,77},
            {24,.5f,76},{24.5f,.5f,73},{25,.5f,69},{25.5f,.5f,67},{26,1,73},{27,.5f,76},
            {28,1.5f,74},{30,.5f,69},{30.5f,.5f,62},{31,.5f,62},
        };
        for (size_t i = 0; i < sizeof(mel) / sizeof(mel[0]); i++) {
            float b = mel[i].b; if (b - floorf(b) > 0.4f) b += 0.16f;                       // swing
            float hz = midiHz((float)mel[i].p);
            note(m, b * beat, mel[i].l * beat * 0.8f, hz, hz, 0.07f, 1, 3.f);
            note(m, b * beat, mel[i].l * beat * 0.8f, hz * 2, hz * 2, 0.04f, 2, 4.f);
        }
        music = toPCM(m, 0.35f);

        // ---- sound effects -----------------------------------------------
        std::vector<float> b;
        b = buf(0.25f); note(b, 0, 0.06f, 880, 880, 0.4f, 1, 8); note(b, 0.06f, 0.1f, 1320, 1320, 0.4f, 1, 10);
        note(b, 0.06f, 0.15f, 2640, 2640, 0.15f, 0, 12); sCatch = toPCM(b, 0.6f);

        b = buf(0.45f); { float ps[4] = { 72,76,79,84 }; for (int i = 0; i < 4; i++) note(b, i * 0.07f, 0.12f, midiHz(ps[i]), midiHz(ps[i]), 0.35f, 2, 5); }
        note(b, 0.28f, 0.15f, midiHz(96), midiHz(96), 0.12f, 0, 10); sPower = toPCM(b, 0.7f);

        b = buf(1.05f); for (int i = 0; i < 8; i++) note(b, i * 0.125f, 0.11f, i % 2 ? 700.f : 980.f, i % 2 ? 680.f : 1000.f, 0.38f, 1, 1.f, 0.01f);
        note(b, 0, 1.0f, 0, 0, 0.05f, 4, 1.f); sAlarm = toPCM(b, 0.5f);

        b = buf(0.5f); note(b, 0, 0.4f, 520, 140, 0.45f, 3, 2.f, 0.05f, 0.06f); sSteal = toPCM(b, 0.4f);

        b = buf(1.5f); { float ps[4] = { 67,72,76,79 }; for (int i = 0; i < 4; i++) note(b, i * 0.11f, 0.1f, midiHz(ps[i]), midiHz(ps[i]), 0.3f, 1, 4); }
        { float ch[3] = { 72,76,84 }; for (int i = 0; i < 3; i++) note(b, 0.46f, 0.8f, midiHz(ch[i]), midiHz(ch[i]), 0.2f, 2, 1.2f, 0.15f, 0.01f); }
        sQuota = toPCM(b, 0.6f);

        b = buf(2.3f); { float ps[4] = { 63,62,61,60 }; for (int i = 0; i < 4; i++) note(b, i * 0.45f, i == 3 ? 0.8f : 0.38f, midiHz(ps[i]), midiHz(ps[i]) * (i == 3 ? 0.97f : 1.f), 0.35f, 3, 0.8f, 0.08f, 0.02f); }
        sLose = toPCM(b, 0.25f);
    }

    WAVEFORMATEX fmt() {
        WAVEFORMATEX f; ZeroMemory(&f, sizeof f);
        f.wFormatTag = WAVE_FORMAT_PCM; f.nChannels = 1; f.nSamplesPerSec = RATE;
        f.wBitsPerSample = 16; f.nBlockAlign = 2; f.nAvgBytesPerSec = RATE * 2;
        return f;
    }
    void init() {
        build();
        WAVEFORMATEX f = fmt();
        if (waveOutOpen(&musicOut, WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
        for (int i = 0; i < NCH; i++) { waveOutOpen(&chOut[i], WAVE_MAPPER, &f, 0, 0, CALLBACK_NULL); ZeroMemory(&chHdr[i], sizeof(WAVEHDR)); }
        ZeroMemory(&musicHdr, sizeof musicHdr);
        ok = true;
    }
    void stopMusic() {
        if (!ok) return;
        waveOutReset(musicOut);
        if (musicHdr.dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(musicOut, &musicHdr, sizeof musicHdr);
        waveOutRestart(musicOut);
    }
    void playMusic() {
        if (!ok) return;
        stopMusic();
        ZeroMemory(&musicHdr, sizeof musicHdr);
        musicHdr.lpData = (LPSTR)&music[0]; musicHdr.dwBufferLength = (DWORD)(music.size() * 2);
        waveOutPrepareHeader(musicOut, &musicHdr, sizeof musicHdr);
        musicHdr.dwFlags |= WHDR_BEGINLOOP | WHDR_ENDLOOP; musicHdr.dwLoops = 100000;
        waveOutWrite(musicOut, &musicHdr, sizeof musicHdr);
    }
    void pauseMusic(bool p) { if (ok) { if (p) waveOutPause(musicOut); else waveOutRestart(musicOut); } }
    void play(const Clip& c) {
        if (!ok || c.empty()) return;
        int i = nextCh++ % NCH;
        waveOutReset(chOut[i]);
        if (chHdr[i].dwFlags & WHDR_PREPARED) waveOutUnprepareHeader(chOut[i], &chHdr[i], sizeof(WAVEHDR));
        ZeroMemory(&chHdr[i], sizeof(WAVEHDR));
        chHdr[i].lpData = (LPSTR)&c[0]; chHdr[i].dwBufferLength = (DWORD)(c.size() * 2);
        waveOutPrepareHeader(chOut[i], &chHdr[i], sizeof(WAVEHDR));
        waveOutWrite(chOut[i], &chHdr[i], sizeof(WAVEHDR));
    }
    void shutdown() {
        if (!ok) return;
        stopMusic(); waveOutClose(musicOut);
        for (int i = 0; i < NCH; i++) { waveOutReset(chOut[i]); waveOutClose(chOut[i]); }
    }
}
#define SND(x) Audio::play(Audio::x)
#else
namespace Audio { inline void init() {} inline void playMusic() {} inline void stopMusic() {} inline void pauseMusic(bool) {} inline void shutdown() {} }
#define SND(x) ((void)0)
#endif

// ============================================================================
//  Game objects
// ============================================================================
enum { ST_START, ST_PLAY, ST_END };
enum { END_QUOTA, END_LOSE, END_SHIFT };
enum { D_IDLE, D_WARN, D_OPENING, D_OPEN, D_CLOSING };
enum { IT_CANISTER, IT_LAUGH, IT_SOCK, IT_DUCK, IT_SUIT, IT_SPEED };

struct Player {
    float x, y, vy, knock, walk, squash, frozen, suit, speed, blinkT;
    int dir, lives; bool onGround, moving;
};
struct Rival { float x, speed, walk, camo, camoTimer; int dir, camoState; };
struct Door { float x, vx, t, open; int state, forced; bool dropped; float flash; };
struct Item { int type; float x, y, angle, spin, phase; bool alive; };
struct Particle { float x, y, vx, vy, life, maxLife, r, g, b, size; };
struct Popup { std::string text; float x, y, life; int pal; };
struct Sched { float t; int type; bool done; };

static Player P;
static Rival Rv;
static Door doors[4];
static std::vector<Item> items;
static std::vector<Particle> parts;
static std::vector<Popup> popups;
static Sched sched[4];

static int   gState = ST_START, gEnd = END_SHIFT, gPendingEnd = -1;
static bool  gPaused = false;
static float gReal = 0, gClock = 0, gElapsed = 0;
static int   gScore = 0, gCaught = 0, gMissed = 0, gStolen = 0;
static float gStationCD = 0, gAlarm = 0, gNextDrop = 1.2f, gEndDelay = -1.f;
static float gMouseX = -100, gMouseY = -100; static int gHover = -1;
static bool  kA = false, kD = false, kL = false, kR = false, gJumpQueued = false;
static int   gLastMs = 0;

static float progress() { return clampf(gElapsed / SHIFT_TIME, 0, 1); }

static void itemHalf(int type, float& hw, float& hh) {
    switch (type) {
    case IT_CANISTER: case IT_LAUGH: hw = 12; hh = 22; break;
    case IT_SOCK: hw = 16; hh = 17; break;
    case IT_DUCK: hw = 18; hh = 15; break;
    default: hw = 21; hh = 21; break;
    }
}
static bool isToxic(int t) { return t == IT_SOCK || t == IT_DUCK; }

static void burst(float x, float y, float r, float g, float b, int n, float spd, float size = 5) {
    for (int i = 0; i < n; i++) {
        Particle p; float a = frand(0, 2 * PI), s = frand(0.3f, 1.f) * spd;
        p.x = x; p.y = y; p.vx = cosf(a) * s; p.vy = sinf(a) * s + spd * 0.3f;
        p.life = p.maxLife = frand(0.4f, 0.8f); p.r = r; p.g = g; p.b = b; p.size = size * frand(0.6f, 1.2f);
        parts.push_back(p);
    }
}
static void popup(const char* t, float x, float y, int pal) {
    Popup p; p.text = t; p.x = x; p.y = y; p.life = 1.1f; p.pal = pal; popups.push_back(p);
}

static void resetGame() {
    memset(&P, 0, sizeof P);
    P.x = W / 2; P.y = FLOOR_Y; P.dir = 1; P.lives = MAX_LIVES; P.onGround = true; P.blinkT = 2.f;
    Rv.x = 860; Rv.dir = -1; Rv.speed = 95; Rv.walk = 0; Rv.camo = 0; Rv.camoState = 0; Rv.camoTimer = frand(3.5f, 5.f);
    float xs[4] = { 140, 380, 620, 860 };
    for (int i = 0; i < 4; i++) {
        doors[i].x = xs[i]; doors[i].vx = (i % 2 ? -1.f : 1.f) * frand(35, 60);
        doors[i].state = D_IDLE; doors[i].t = 0; doors[i].open = 0; doors[i].forced = -1;
        doors[i].dropped = false; doors[i].flash = 0;
    }
    items.clear(); parts.clear(); popups.clear();
    gScore = gCaught = gMissed = gStolen = 0;
    gStationCD = 0; gAlarm = 0; gNextDrop = 1.0f; gEndDelay = -1; gPendingEnd = -1;
    gElapsed = 0; gClock = 0; gPaused = false;
    Sched s[4] = { {9, IT_SUIT, false}, {24, IT_SPEED, false}, {44, IT_SUIT, false}, {62, IT_SPEED, false} };
    for (int i = 0; i < 4; i++) sched[i] = s[i];
}

static void startGame() { resetGame(); gState = ST_PLAY; Audio::playMusic(); }

static void endGame(int type) {
    gState = ST_END; gEnd = type; Audio::stopMusic();
#ifdef _WIN32
    if (type == END_QUOTA) SND(sQuota); else SND(sLose);
#endif
}

// ============================================================================
//  Update logic
// ============================================================================
static int pickItemType() {
    for (int i = 0; i < 4; i++) if (!sched[i].done && gElapsed >= sched[i].t) { sched[i].done = true; return sched[i].type; }
    float r = frand(0, 1);
    if (r < 0.07f) return IT_LAUGH;
    if (r < 0.52f) return IT_CANISTER;
    if (r < 0.90f) return frand(0, 1) < 0.5f ? IT_SOCK : IT_DUCK;
    return frand(0, 1) < 0.5f ? IT_SUIT : IT_SPEED;
}
static bool spawnClear(float x) {
    for (size_t i = 0; i < items.size(); i++)
        if (fabsf(items[i].x - x) < 52 && items[i].y > DOOR_TOP - DOOR_H - 100) return false;
    return true;
}
static void spawnItem(Door& d) {
    Item it; it.type = d.forced >= 0 ? d.forced : pickItemType();
    float hw, hh; itemHalf(it.type, hw, hh);
    it.x = d.x; it.y = DOOR_TOP - DOOR_H - hh - 2; it.angle = 0;
    it.spin = (frand(0, 1) < 0.5f ? -1.f : 1.f) * frand(200, 320); it.phase = frand(0, 6); it.alive = true;
    items.push_back(it);
}

static void updateDoors(float dt, bool playing) {
    float pr = progress();
    for (int i = 0; i < 4; i++) {
        Door& d = doors[i];
        if (d.flash > 0) d.flash -= dt;
        switch (d.state) {
        case D_IDLE: {
            float nx = d.x + d.vx * (1.f + 0.6f * pr) * dt;
            float minX = DOOR_W / 2 + 26, maxX = W - DOOR_W / 2 - 26;
            if (nx < minX) { nx = minX; d.vx = fabsf(d.vx); }
            if (nx > maxX) { nx = maxX; d.vx = -fabsf(d.vx); }
            bool blocked = false;
            for (int j = 0; j < 4; j++)
                if (j != i && fabsf(nx - doors[j].x) < DOOR_W + 30 && (doors[j].x - d.x) * d.vx > 0) blocked = true;
            if (blocked) d.vx = -d.vx; else d.x = nx;
        } break;
        case D_WARN:
            d.t += dt; if (d.t >= WARN_T) { d.state = D_OPENING; d.t = 0; } break;
        case D_OPENING:
            d.t += dt; d.open = std::min(1.f, d.t / OPEN_T);
            if (d.t >= OPEN_T) { d.state = D_OPEN; d.t = 0; d.dropped = !playing; } break;
        case D_OPEN:
            d.t += dt;
            if (!d.dropped) { if (spawnClear(d.x) || d.t > 0.7f) { spawnItem(d); d.dropped = true; d.t = 0; } }
            else if (d.t >= HOLD_T) { d.state = D_CLOSING; d.t = 0; }
            break;
        case D_CLOSING:
            d.t += dt; d.open = 1.f - std::min(1.f, d.t / OPEN_T);
            if (d.t >= OPEN_T) { d.state = D_IDLE; d.open = 0; d.forced = -1; } break;
        }
    }
    gNextDrop -= dt;
    if (gNextDrop <= 0) {
        int idle[4], n = 0;
        for (int i = 0; i < 4; i++) if (doors[i].state == D_IDLE) idle[n++] = i;
        if (n) {
            Door& d = doors[idle[rand() % n]]; d.state = D_WARN; d.t = 0;
            gNextDrop = (playing ? lerpf(1.45f, 0.45f, pr) : 2.2f) * frand(0.75f, 1.25f);
        } else gNextDrop = 0.2f;
    }
}

static void updatePlayer(float dt) {
    int input = (kD || kR ? 1 : 0) - (kA || kL ? 1 : 0);
    if (P.frozen > 0) { P.frozen -= dt; input = 0; gJumpQueued = false; }
    float spd = 270.f * (P.speed > 0 ? 1.75f : 1.f);
    if (input) P.dir = input;
    P.moving = input != 0;
    if (P.moving) P.walk += dt * 13.f;
    P.x += (input * spd + P.knock) * dt;
    P.knock *= powf(0.01f, dt); if (fabsf(P.knock) < 5) P.knock = 0;
    if (gJumpQueued && P.onGround) { P.vy = 660; P.onGround = false; }
    gJumpQueued = false;
    P.vy -= 1750.f * dt; P.y += P.vy * dt;
    if (P.y <= FLOOR_Y) { if (!P.onGround) P.squash = 1.f; P.y = FLOOR_Y; P.vy = 0; P.onGround = true; }
    P.x = clampf(P.x, 32, W - 32);
    if (P.squash > 0) P.squash = std::max(0.f, P.squash - dt * 5);
    if (P.suit > 0) P.suit -= dt;
    if (P.speed > 0) P.speed -= dt;
    P.blinkT -= dt; if (P.blinkT < -0.15f) P.blinkT = frand(2, 4.5f);
    if (P.frozen > 0 && frand(0, 1) < 0.5f) {   // decontamination foam
        Particle p; p.x = P.x + frand(-30, 30); p.y = P.y + frand(5, 80); p.vx = frand(-15, 15); p.vy = frand(20, 60);
        p.life = p.maxLife = 0.6f; p.r = 0.9f; p.g = 1.f; p.b = 1.f; p.size = frand(4, 8); parts.push_back(p);
    }
}

static void updateRival(float dt) {
    Rv.speed = 95.f + 55.f * progress();
    Rv.x += Rv.dir * Rv.speed * dt; Rv.walk += dt * 9.f;
    if (Rv.x > W - 46) { Rv.x = W - 46; Rv.dir = -1; }
    if (Rv.x < 46) { Rv.x = 46; Rv.dir = 1; }
    // camouflage state machine: visible -> fade out -> hidden -> fade in
    Rv.camoTimer -= dt;
    switch (Rv.camoState) {
    case 0: Rv.camo = 0; if (Rv.camoTimer <= 0) { Rv.camoState = 1; Rv.camoTimer = 0.7f; } break;
    case 1: Rv.camo = 1 - std::max(0.f, Rv.camoTimer) / 0.7f; if (Rv.camoTimer <= 0) { Rv.camoState = 2; Rv.camoTimer = 2.2f; } break;
    case 2: Rv.camo = 1; if (Rv.camoTimer <= 0) { Rv.camoState = 3; Rv.camoTimer = 0.7f; } break;
    case 3: Rv.camo = std::max(0.f, Rv.camoTimer) / 0.7f; if (Rv.camoTimer <= 0) { Rv.camoState = 0; Rv.camoTimer = frand(4.f, 6.5f); } break;
    }
    // collision with the player -> push the player back (no life lost)
    float px0 = P.x - 24, px1 = P.x + 24, py1 = P.y + 78;
    float rx0 = Rv.x - 40, rx1 = Rv.x + 40, ry1 = FLOOR_Y + 112;
    if (px1 > rx0 && px0 < rx1 && P.y < ry1 && py1 > FLOOR_Y) {
        float side = (P.x >= Rv.x) ? 1.f : -1.f;
        bool pinned = (side > 0 && P.x > W - 40) || (side < 0 && P.x < 40);
        if (pinned || P.frozen > 0) {          // player can't be moved -> rival bumps and turns back
            Rv.dir = (int)-side;
            Rv.x = clampf(P.x - side * 66, 46, W - 46);
        } else {                               // push the player back and give a little hop
            P.x = clampf(Rv.x + side * 65, 32, W - 32);
            P.knock = side * 520;
            if (P.onGround) { P.vy = 280; P.onGround = false; }
        }
    }
}

static void updateItems(float dt) {
    float mul = 1.f + 1.35f * progress();
    for (size_t i = 0; i < items.size(); i++) {
        Item& it = items[i];
        float v = 130;
        if (it.type == IT_LAUGH) v = 230; else if (isToxic(it.type)) v = 150; else if (it.type >= IT_SUIT) v = 115;
        it.y -= v * mul * dt; it.phase += dt;
        if (isToxic(it.type)) it.angle += it.spin * dt;
    }
    // keep items from overlapping each other while falling
    for (size_t i = 0; i < items.size(); i++) for (size_t j = 0; j < items.size(); j++) {
        if (i == j || items[j].y >= items[i].y) continue;
        float hwi, hhi, hwj, hhj; itemHalf(items[i].type, hwi, hhi); itemHalf(items[j].type, hwj, hhj);
        if (fabsf(items[i].x - items[j].x) < hwi + hwj + 2 && items[i].y - hhi < items[j].y + hhj + 4)
            items[i].y = items[j].y + hhj + hhi + 4;
    }
    float px0 = P.x - 26, px1 = P.x + 26, py0 = P.y, py1 = P.y + 80;
    float rx0 = Rv.x - 40, rx1 = Rv.x + 40, ry0 = FLOOR_Y, ry1 = FLOOR_Y + 112;
    for (size_t i = 0; i < items.size(); i++) {
        Item& it = items[i]; float hw, hh; itemHalf(it.type, hw, hh);
        float x0 = it.x - hw, x1 = it.x + hw, y0 = it.y - hh, y1 = it.y + hh;
        if (x1 > px0 && x0 < px1 && y1 > py0 && y0 < py1) {          // ---- player
            it.alive = false;
            switch (it.type) {
            case IT_CANISTER: gScore += 10; gCaught++; popup("+10", it.x, it.y + 20, PAL_BLUE); burst(it.x, it.y, 0.4f, 0.7f, 1, 14, 160); SND(sCatch); break;
            case IT_LAUGH: gScore += 50; gCaught++; popup("+50 LAUGH!", it.x, it.y + 20, PAL_GOLD); burst(it.x, it.y, 1, 0.85f, 0.2f, 24, 220); SND(sCatch); break;
            case IT_SUIT: P.suit = POWER_TIME; popup("HAZMAT SUIT!", it.x, it.y + 20, PAL_GOLD); burst(it.x, it.y, 1, 0.7f, 0.1f, 18, 180); SND(sPower); break;
            case IT_SPEED: P.speed = POWER_TIME; popup("SPEED BOOST!", it.x, it.y + 20, PAL_GREEN); burst(it.x, it.y, 0.3f, 1, 0.6f, 18, 180); SND(sPower); break;
            default:
                burst(it.x, it.y, 0.5f, 1, 0.3f, 14, 120, 7);
                if (P.frozen > 0) {}
                else if (P.suit > 0) popup("BLOCKED!", it.x, it.y + 20, PAL_GOLD);
                else {
                    P.lives--; P.frozen = 1.0f; gAlarm = 1.0f; P.knock = 0;
                    popup("2319!", P.x, P.y + 110, PAL_RED); SND(sAlarm);
                }
            }
        } else if (!isToxic(it.type) && x1 > rx0 && x0 < rx1 && y1 > ry0 && y0 < ry1) {   // ---- rival steals
            it.alive = false;
            if (it.type <= IT_LAUGH) gStolen++;
            popup("STOLEN!", it.x, it.y + 20, PAL_PURPLE); burst(it.x, it.y, 0.6f, 0.4f, 0.9f, 16, 150); SND(sSteal);
        } else if (y0 <= FLOOR_Y) {                                   // ---- floor
            it.alive = false;
            if (it.type <= IT_LAUGH) { gMissed++; popup("MISSED", it.x, FLOOR_Y + 30, PAL_WHITE); burst(it.x, FLOOR_Y + 4, 0.5f, 0.6f, 0.8f, 8, 90); }
            else if (isToxic(it.type)) burst(it.x, FLOOR_Y + 4, 0.4f, 0.9f, 0.2f, 12, 110, 6);
            else burst(it.x, FLOOR_Y + 4, 0.9f, 0.9f, 0.9f, 10, 90);
        }
    }
    items.erase(std::remove_if(items.begin(), items.end(), [](const Item& i) { return !i.alive; }), items.end());
}

static void updateFx(float dt) {
    for (size_t i = 0; i < parts.size(); i++) {
        Particle& p = parts[i]; p.x += p.vx * dt; p.y += p.vy * dt; p.vy -= 260 * dt; p.life -= dt;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
    for (size_t i = 0; i < popups.size(); i++) { popups[i].y += 45 * dt; popups[i].life -= dt; }
    popups.erase(std::remove_if(popups.begin(), popups.end(), [](const Popup& p) { return p.life <= 0; }), popups.end());
}

static void update(float dt) {
    gReal += dt;
    if (gState == ST_START) { gClock += dt; updateDoors(dt, false); return; }
    if (gState != ST_PLAY || gPaused) return;
    gClock += dt;
    if (gAlarm > 0) gAlarm -= dt;
    if (gEndDelay > 0) {                       // last life lost: let the alarm play out
        gEndDelay -= dt; updateFx(dt);
        if (gEndDelay <= 0) endGame(gPendingEnd);
        return;
    }
    gElapsed += dt;
    if (gStationCD > 0) gStationCD -= dt;
    updatePlayer(dt);
    updateRival(dt);
    updateDoors(dt, true);
    updateItems(dt);
    updateFx(dt);
    if (gScore >= QUOTA) endGame(END_QUOTA);
    else if (P.lives <= 0) { gEndDelay = 1.0f; gPendingEnd = END_LOSE; }
    else if (gElapsed >= SHIFT_TIME) endGame(END_SHIFT);
}

// ============================================================================
//  MODELS
// ============================================================================

// ---- Background wall (animated: chasing lights, flowing scream pipe, fans) --
static void drawBackground(float t) {
    float wb[3] = { 0.12f,0.13f,0.20f }, wt[3] = { 0.20f,0.22f,0.31f };
    quadGrad(0, FLOOR_Y, W, RAIL_Y0, wb, wt);
    for (int k = 0; k < 8; k++) {                       // wall panels
        float x = k * 125.f;
        glColor4f(1, 1, 1, 0.025f); quad(x + 6, FLOOR_Y + 14, x + 119, RAIL_Y0 - 20);
        glColor4f(0, 0, 0, 0.25f); glLineWidth(2); seg(x, FLOOR_Y, x, RAIL_Y0); glLineWidth(1);
    }
    glColor4f(0.30f, 0.33f, 0.45f, 1); quad(0, 466, W, 472);                        // trim
    for (int k = 0; k < 8; k++) {                       // chasing indicator lights
        float x = 62.5f + k * 125.f;
        bool lit = ((int)(t * 5) % 8) == k || ((int)(t * 5) + 4) % 8 == k;
        if (lit) { glColor4f(1, 0.85f, 0.3f, 0.25f); ellipse(x, 486, 13, 13); }
        if (lit) glColor4f(1, 0.85f, 0.3f, 1); else glColor4f(0.35f, 0.37f, 0.45f, 1);
        ellipse(x, 486, 6, 6, 20);
    }
    // scream energy pipe with flowing blobs
    float pb[3] = { 0.25f,0.27f,0.35f }, pt[3] = { 0.45f,0.48f,0.58f };
    quadGrad(0, 296, W, 318, pb, pt);
    glColor4f(0.10f, 0.12f, 0.18f, 1); quad(0, 302, W, 312);
    for (int k = 0; k < 9; k++) {
        float x = fmodf(t * 110.f + k * 125.f, W + 60) - 30;
        glColor4f(1, 0.85f, 0.2f, 0.3f); ellipse(x, 307, 18, 6);
        glColor4f(1, 0.92f, 0.45f, 0.9f); ellipse(x, 307, 9, 3.5f, 16);
    }
    for (int k = 0; k < 6; k++) { glColor4f(0.35f, 0.38f, 0.5f, 1); quad(k * 200.f + 90, 292, k * 200.f + 104, 322); }
    // two rotating ventilation fans
    float fx[2] = { 110, 890 };
    for (int f = 0; f < 2; f++) {
        glColor4f(0.10f, 0.11f, 0.16f, 1); ellipse(fx[f], 400, 36, 36);
        glColor4f(0.40f, 0.43f, 0.55f, 1); glLineWidth(3); ellipseLine(fx[f], 400, 36, 36); glLineWidth(1);
        glColor4f(0.33f, 0.36f, 0.47f, 1);
        for (int b = 0; b < 4; b++) {
            float a = t * 3.f * (f ? -1 : 1) + b * PI / 2;
            glBegin(GL_TRIANGLES);
            glVertex2f(fx[f], 400);
            glVertex2f(fx[f] + 31 * cosf(a), 400 + 31 * sinf(a));
            glVertex2f(fx[f] + 31 * cosf(a + 0.55f), 400 + 31 * sinf(a + 0.55f));
            glEnd();
        }
        glColor4f(0.55f, 0.58f, 0.7f, 1); ellipse(fx[f], 400, 6, 6, 16);
    }
    TextFx sign = mkText(34, PAL_WHITE); sign.alpha = 0.13f; sign.shadow = false; sign.tracking = 0.25f;
    drawText("SCARE FLOOR F", W / 2, 380, sign, 1);
}

// ---- Upper boundary: the door rail --------------------------------------
// primitives: QUADS (beam, groove, ceiling), TRIANGLES (brackets), LINES
// (edges), POINTS (rivets), POLYGON (hex end caps)
static void drawRail() {
    glColor4f(0.08f, 0.08f, 0.12f, 1); quad(0, RAIL_Y1, W, HUD_Y0);
    glColor4f(0.36f, 0.39f, 0.48f, 1);
    for (int k = 0; k < 9; k++) { float x = 60 + k * 110.f; tri(x - 12, HUD_Y0, x + 12, HUD_Y0, x, RAIL_Y1); }
    float bb[3] = { 0.42f,0.45f,0.53f }, bt[3] = { 0.70f,0.73f,0.80f };
    quadGrad(0, RAIL_Y0, W, RAIL_Y1, bb, bt);
    glColor4f(0.18f, 0.19f, 0.25f, 1); quad(0, 625, W, 629);
    glColor4f(0.88f, 0.90f, 0.96f, 1); glLineWidth(1.5f);
    glBegin(GL_LINES); glVertex2f(0, RAIL_Y1 - 1); glVertex2f(W, RAIL_Y1 - 1); glEnd();
    glColor4f(0.15f, 0.16f, 0.2f, 1);
    glBegin(GL_LINES); glVertex2f(0, RAIL_Y0); glVertex2f(W, RAIL_Y0); glEnd();
    glLineWidth(1);
    glPointSize(3.5f); glColor4f(0.25f, 0.27f, 0.33f, 1);
    glBegin(GL_POINTS); for (float x = 12; x < W; x += 24) { glVertex2f(x, 632.5f); glVertex2f(x, 621.5f); } glEnd();
    glPointSize(1);
    float ex[2] = { 12, W - 12 };
    for (int e = 0; e < 2; e++) {
        glColor4f(0.85f, 0.65f, 0.2f, 1); ellipse(ex[e], 627, 11, 11, 6);
        glColor4f(0.3f, 0.2f, 0.05f, 1); ellipse(ex[e], 627, 4, 4, 6);
    }
}

// ---- Lower boundary: the Scare Floor -------------------------------------
// primitives: QUADS (slab, hazard stripes), LINES (tile seams), POINTS (bolts),
// POLYGON (vent grates), TRIANGLES (warning signs), LINE_STRIP (sign marks)
static void drawFloor() {
    float fb[3] = { 0.20f,0.21f,0.26f }, ft[3] = { 0.33f,0.35f,0.42f };
    quadGrad(0, 0, W, 76, fb, ft);
    glColor4f(0.15f, 0.16f, 0.2f, 1); glLineWidth(2);
    glBegin(GL_LINES);
    for (float x = 0; x <= W; x += 100) { glVertex2f(x, 0); glVertex2f(x, 76); }
    glVertex2f(0, 38); glVertex2f(W, 38);
    glEnd(); glLineWidth(1);
    glPointSize(4); glColor4f(0.55f, 0.58f, 0.66f, 1);
    glBegin(GL_POINTS); for (float x = 0; x <= W; x += 100) { glVertex2f(x + 6, 44); glVertex2f(x + 6, 6); glVertex2f(x + 94, 44); glVertex2f(x + 94, 6); } glEnd();
    glPointSize(1);
    glColor4f(0.07f, 0.07f, 0.09f, 1); quad(0, 76, W, FLOOR_Y);              // hazard strip
    glColor4f(1, 0.80f, 0.10f, 1);
    glBegin(GL_QUADS);
    for (float x = -20; x < W + 20; x += 36) { glVertex2f(x, 76); glVertex2f(x + 18, 76); glVertex2f(x + 30, FLOOR_Y); glVertex2f(x + 12, FLOOR_Y); }
    glEnd();
    glColor4f(0.75f, 0.78f, 0.86f, 1); glLineWidth(2); seg(0, FLOOR_Y, W, FLOOR_Y); glLineWidth(1);
    float vx[3] = { 200, 500, 800 };
    for (int v = 0; v < 3; v++) {
        glColor4f(0.10f, 0.10f, 0.13f, 1); ellipse(vx[v], 38, 22, 14, 8);
        glColor4f(0.40f, 0.42f, 0.50f, 1);
        glBegin(GL_LINES); for (int k = -2; k <= 2; k++) { glVertex2f(vx[v] + k * 7, 28); glVertex2f(vx[v] + k * 7, 48); } glEnd();
    }
    float sx[2] = { 60, 940 };
    for (int s = 0; s < 2; s++) {
        glColor4f(1, 0.8f, 0.1f, 1); tri(sx[s] - 16, 22, sx[s] + 16, 22, sx[s], 52);
        glColor4f(0.1f, 0.1f, 0.1f, 1); glLineWidth(3);
        glBegin(GL_LINE_STRIP); glVertex2f(sx[s], 44); glVertex2f(sx[s], 33); glEnd();
        glPointSize(4); glBegin(GL_POINTS); glVertex2f(sx[s], 27); glEnd(); glPointSize(1); glLineWidth(1);
    }
}

// ---- Closet door --------------------------------------------------------
// primitives: LINES (hangers), POLYGON (wheels, panel, knob, lamp), QUADS
// (frame, room), LINE_LOOP (panel insets, hover glow), POINTS (decorations)
static const float DOOR_COL[4][3] = { {0.96f,0.47f,0.66f}, {0.30f,0.58f,0.98f}, {0.30f,0.82f,0.56f}, {0.99f,0.70f,0.22f} };
static void drawDoor(const Door& d, int idx, bool hovered) {
    float shake = (d.state == D_WARN) ? sinf(gClock * 70.f) * 2.5f : 0.f;
    float cx = d.x + shake, top = DOOR_TOP, bot = DOOR_TOP - DOOR_H, hw = DOOR_W / 2;
    // hangers and trolley wheels on the rail
    glColor4f(0.55f, 0.58f, 0.66f, 1); glLineWidth(3);
    glBegin(GL_LINES); glVertex2f(cx - 20, 627); glVertex2f(cx - 20, top + 6); glVertex2f(cx + 20, 627); glVertex2f(cx + 20, top + 6); glEnd();
    glLineWidth(1);
    for (int s = -1; s <= 1; s += 2) {
        glColor4f(0.14f, 0.15f, 0.2f, 1); ellipse(cx + s * 20, 627, 6, 6, 16);
        glColor4f(0.7f, 0.72f, 0.8f, 1); ellipse(cx + s * 20, 627, 2, 2, 8);
    }
    // frame
    glColor4f(0.24f, 0.20f, 0.22f, 1); quad(cx - hw - 6, bot - 3, cx + hw + 6, top + 6);
    // the child's dark room behind the door
    glColor4f(0.04f, 0.05f, 0.12f, 1); quad(cx - hw, bot, cx + hw, top);
    if (d.open > 0) {
        glColor4f(0.10f, 0.12f, 0.28f, d.open); quad(cx - hw + 6, bot, cx + hw - 6, bot + 18);   // child's bed
        glColor4f(0.85f, 0.85f, 0.95f, d.open); ellipse(cx - hw + 16, bot + 20, 8, 4, 14);       // pillow
        glPointSize(2.5f); glColor4f(1, 1, 0.7f, d.open);
        glBegin(GL_POINTS); glVertex2f(cx - 18, top - 18); glVertex2f(cx + 12, top - 30); glVertex2f(cx - 4, top - 50); glVertex2f(cx + 22, top - 12); glEnd();
        glPointSize(1);
        glColor4f(1, 0.95f, 0.7f, d.open); ellipse(cx + 16, top - 22, 6, 6, 16);
        glColor4f(0.04f, 0.05f, 0.12f, 1); ellipse(cx + 19, top - 20, 5, 5, 16);
    }
    // swinging panel (hinge on the left), drawn in perspective as a trapezoid
    float a = d.open * 1.35f, pw = DOOR_W * cosf(a), sk = sinf(a) * 12.f;
    float c0 = DOOR_COL[idx][0], c1 = DOOR_COL[idx][1], c2 = DOOR_COL[idx][2];
    float shade = 1.f - 0.35f * d.open;
    if (d.flash > 0) { c0 = lerpf(c0, 1, d.flash); c1 = lerpf(c1, 1, d.flash); c2 = lerpf(c2, 1, d.flash); }
    #define MAP(u, v, X, Y) { X = cx - hw + (u) * pw; float yb = bot - (u) * sk, yt = top + (u) * sk; Y = yb + (v) * (yt - yb); }
    float X, Y;
    glColor4f(c0 * shade, c1 * shade, c2 * shade, 1);
    glBegin(GL_POLYGON);
    MAP(0, 0, X, Y); glVertex2f(X, Y); MAP(1, 0, X, Y); glVertex2f(X, Y);
    MAP(1, 1, X, Y); glVertex2f(X, Y); MAP(0, 1, X, Y); glVertex2f(X, Y);
    glEnd();
    if (pw > 6) {
        glColor4f(c0 * shade * 0.6f, c1 * shade * 0.6f, c2 * shade * 0.6f, 1); glLineWidth(2);
        float ins[2][4] = { {0.15f,0.55f,0.85f,0.90f}, {0.15f,0.10f,0.85f,0.45f} };
        for (int k = 0; k < 2; k++) {
            glBegin(GL_LINE_LOOP);
            MAP(ins[k][0], ins[k][1], X, Y); glVertex2f(X, Y); MAP(ins[k][2], ins[k][1], X, Y); glVertex2f(X, Y);
            MAP(ins[k][2], ins[k][3], X, Y); glVertex2f(X, Y); MAP(ins[k][0], ins[k][3], X, Y); glVertex2f(X, Y);
            glEnd();
        }
        glLineWidth(1);
        // decorations: each door has its own pattern of dots
        glPointSize(4.f);
        glBegin(GL_POINTS);
        for (int k = 0; k < 6; k++) {
            float u = 0.3f + 0.4f * ((k * 37 + idx * 13) % 10) / 10.f, v = 0.6f + 0.25f * ((k * 53 + idx * 7) % 10) / 10.f;
            if (idx == 0) glColor4f(1, 1, 0.6f, 1); else if (idx == 1) glColor4f(1, 0.95f, 0.4f, 1);
            else if (idx == 2) glColor4f(0.9f, 1, 0.9f, 1); else glColor4f(0.95f, 0.35f, 0.25f, 1);
            MAP(u, v, X, Y); glVertex2f(X, Y);
        }
        glEnd(); glPointSize(1);
        glColor4f(0.95f, 0.80f, 0.30f, 1); MAP(0.86f, 0.48f, X, Y); ellipse(X, Y, 3.6f * cosf(a) + 1, 3.6f, 14);
    }
    #undef MAP
    // warning lamp on top of the frame (blinks red while warning)
    bool warnOn = d.state == D_WARN && ((int)(d.t * 10) % 2 == 0);
    if (warnOn) { glColor4f(1, 0.2f, 0.15f, 0.35f); ellipse(cx, top + 6, 14, 14); glColor4f(1, 0.25f, 0.2f, 1); }
    else if (d.state != D_IDLE) glColor4f(0.3f, 1, 0.45f, 1);
    else glColor4f(0.4f, 0.32f, 0.3f, 1);
    glBegin(GL_POLYGON); for (int i = 0; i <= 12; i++) { float an = PI * i / 12; glVertex2f(cx + 7 * cosf(an), top + 6 + 7 * sinf(an)); } glEnd();
    if (warnOn) {                              // warning bars at the door sides
        glColor4f(1, 0.25f, 0.2f, 1); glLineWidth(3);
        glBegin(GL_LINES);
        for (int k = 0; k < 3; k++) {
            float o = 12 + k * 6;
            glVertex2f(cx - hw - o, bot + 25); glVertex2f(cx - hw - o, top - 25);
            glVertex2f(cx + hw + o, bot + 25); glVertex2f(cx + hw + o, top - 25);
        }
        glEnd(); glLineWidth(1);
    }
    if (hovered) {                             // Door Station hover highlight
        bool ready = gStationCD <= 0;
        float hr = ready ? 1.f : 0.9f, hg = ready ? 0.85f : 0.35f, hb = ready ? 0.2f : 0.3f;
        float pulse = 0.5f + 0.5f * sinf(gReal * 8);
        for (int g = 3; g >= 1; g--) {
            glColor4f(hr, hg, hb, (0.18f + 0.12f * pulse) * (4 - g) / 3.f); glLineWidth(g * 3.f);
            roundRectLine(cx - hw - 10, bot - 7, cx + hw + 10, top + 10, 6);
        }
        glColor4f(hr, hg, hb, 1); glLineWidth(2.5f); roundRectLine(cx - hw - 10, bot - 7, cx + hw + 10, top + 10, 6);
        glLineWidth(1);
    }
}

// ---- Player: round green one-eyed monster --------------------------------
// primitives: POLYGON (body, eye, iris, pupil, mouth, feet), TRIANGLES (horns,
// teeth, claws), QUADS (legs), LINE_STRIP (arms), LINES (fingers),
// LINE_LOOP (eye outline, suit bubble), POINTS (eye highlight)
static void drawMike(float walk, bool moving, float squash, bool blink, int mood, bool suit, bool speed, float t) {
    float b0 = 0.56f, b1 = 0.80f, b2 = 0.20f;            // Mike green
    if (suit)       { b0 = 1.00f; b1 = 0.80f; b2 = 0.16f; }
    else if (speed) { b0 = 0.25f; b1 = 0.85f; b2 = 0.98f; }
    float d0 = b0 * 0.7f, d1 = b1 * 0.7f, d2 = b2 * 0.7f;
    if (speed) {                                          // speed streaks behind
        glLineWidth(3);
        glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) {
            float y = 30 + k * 14, o = fmodf(t * 300 + k * 37, 30);
            glColor4f(0.4f, 1, 1, 0.75f); glVertex2f(-36 - o, y); glColor4f(0.4f, 1, 1, 0); glVertex2f(-70 - o, y);
        }
        glEnd(); glLineWidth(1);
    }
    float sw = moving ? sinf(walk) : 0.f;
    // legs (quads) + feet (polygon) + toe claws (triangles)
    for (int s = -1; s <= 1; s += 2) {
        float hx = s * 10.f, fx = hx + (moving ? s * sw * 7.f : 0.f), lift = moving ? std::max(0.f, sinf(walk + (s > 0 ? PI : 0))) * 4 : 0;
        col(d0, d1, d2);
        glBegin(GL_QUADS); glVertex2f(hx - 3.5f, 28); glVertex2f(hx + 3.5f, 28); glVertex2f(fx + 3.5f, 4 + lift); glVertex2f(fx - 3.5f, 4 + lift); glEnd();
        col(b0, b1, b2); ellipse(fx + 3, 4 + lift, 9, 4.5f, 18);
        col(0.80f, 0.78f, 0.70f);
        glBegin(GL_TRIANGLES);
        for (int c = 0; c < 2; c++) { float y = 1.f + lift + c * 3.f; glVertex2f(fx + 11, y); glVertex2f(fx + 11, y + 2.f); glVertex2f(fx + 14, y + 1.f); }
        glEnd();
    }
    // horns (behind body)
    col(0.88f, 0.86f, 0.78f);
    tri(-18, 74, -9, 78, -16, 92);
    tri(9, 78, 18, 74, 16, 92);
    // body
    float sx = 1.f + squash * 0.12f, sy = 1.f - squash * 0.12f;
    glPushMatrix(); glTranslatef(0, 50 * sy, 0); glScalef(sx, sy, 1);
    col(d0, d1, d2); ellipse(0, 0, 31, 31, 40);
    col(b0, b1, b2); ellipse(1.5f, 1.5f, 29, 29, 40);
    col(1, 1, 1, 0.18f); ellipse(-12, 15, 8, 5, 16);
    // eye
    float ex = 3, ey = 6;
    col(1, 1, 1); ellipse(ex, ey, 16, 16, 36);
    col(d0 * 0.7f, d1 * 0.7f, d2 * 0.7f); glLineWidth(1.6f); ellipseLine(ex, ey, 16, 16, 36); glLineWidth(1);
    if (blink) {
        col(b0, b1, b2); ellipse(ex, ey, 17, 17, 36);
        col(d0 * 0.6f, d1 * 0.6f, d2 * 0.6f); glLineWidth(2);
        glBegin(GL_LINE_STRIP); for (int i = 0; i <= 10; i++) { float an = PI + PI * i / 10; glVertex2f(ex + 15 * cosf(an), ey + 4 * sinf(an)); } glEnd();
        glLineWidth(1);
    } else {
        float ir = mood == 1 ? 7.f : 9.f, pr = mood == 1 ? 2.8f : 4.3f;
        col(0.15f, 0.62f, 0.52f); ellipse(ex + 5, ey, ir, ir, 28);
        col(0.40f, 0.85f, 0.70f); ellipse(ex + 5, ey, ir * 0.62f, ir * 0.62f, 24);
        col(0.02f, 0.03f, 0.03f); ellipse(ex + 6.5f, ey, pr, pr, 20);
        col(1, 1, 1); glPointSize(3.5f); glBegin(GL_POINTS); glVertex2f(ex + 9, ey + 4); glEnd();
        glPointSize(2); glBegin(GL_POINTS); glVertex2f(ex + 3, ey - 3); glEnd(); glPointSize(1);
    }
    // mouth
    float mx = 4, my = -12;
    col(0.30f, 0.06f, 0.12f);
    if (mood == 1) {                                  // shocked "O"
        ellipse(mx, my - 4, 8, 7.5f, 24);
        col(1, 1, 1); tri(mx - 5, my + 2, mx - 1, my + 2, mx - 3, my - 2); tri(mx + 1, my + 2, mx + 5, my + 2, mx + 3, my - 2);
    } else {
        float rx = mood == 2 ? 16.f : 13.f, ry = mood == 2 ? 11.f : 8.f;
        glBegin(GL_POLYGON); for (int i = 0; i <= 16; i++) { float an = PI + PI * i / 16; glVertex2f(mx + rx * cosf(an), my + ry * sinf(an)); } glEnd();
        col(0.95f, 0.45f, 0.55f); ellipse(mx + 2, my - ry * 0.7f, rx * 0.45f, ry * 0.25f, 16);
        col(1, 1, 1);
        tri(mx - 8, my, mx - 3, my, mx - 5.5f, my - 4); tri(mx + 3, my, mx + 8, my, mx + 5.5f, my - 4);
    }
    glPopMatrix();
    // arms (line strips) + fingers (lines)
    for (int s = -1; s <= 1; s += 2) {
        float swing = moving ? sinf(walk + (s > 0 ? 0 : PI)) * 6 : sinf(t * 2) * 1.5f;
        if (mood == 2) swing = 0;
        float hx = s * 42.f + swing, hy = mood == 2 ? 74.f : 26.f;
        col(d0, d1, d2); glLineWidth(4.5f);
        glBegin(GL_LINE_STRIP); glVertex2f(s * 26.f, 46); glVertex2f(s * 36.f + swing * 0.5f, mood == 2 ? 62.f : 36.f); glVertex2f(hx, hy); glEnd();
        glLineWidth(2.5f);
        glBegin(GL_LINES);
        for (int f = -1; f <= 1; f++) { glVertex2f(hx, hy); glVertex2f(hx + s * 3 + f * 3, hy + (mood == 2 ? 6.f : -6.f)); }
        glEnd(); glLineWidth(1);
    }
    if (suit) {                                       // hazmat bubble helmet
        col(0.85f, 0.95f, 1, 0.14f); ellipse(0, 52, 45, 50, 40);
        col(0.75f, 0.92f, 1, 0.85f); glLineWidth(2.5f); ellipseLine(0, 52, 45, 50, 40);
        col(1, 1, 1, 0.8f); glLineWidth(3);
        glBegin(GL_LINE_STRIP); for (int i = 0; i <= 8; i++) { float an = 1.9f + 0.7f * i / 8; glVertex2f(38 * cosf(an), 52 + 43 * sinf(an)); } glEnd();
        glLineWidth(1);
    }
}

// ---- Rival: huge teal furry monster with purple spots --------------------
// primitives: POLYGON (body, head, spots, feet, nose, eyes), TRIANGLES (fur,
// horns, tail, claws), QUADS (legs, arm), LINES (brows), LINE_STRIP (mouth),
// POINTS (eye highlights)
static void drawSulley(float walk, bool moving) {
    float b0 = 0.25f, b1 = 0.72f, b2 = 0.80f, d0 = 0.16f, d1 = 0.52f, d2 = 0.62f;
    float s0 = 0.50f, s1 = 0.30f, s2 = 0.64f;
    float sw = moving ? sinf(walk) : 0.f;
    // tail
    col(d0, d1, d2); tri(-30, 52, -34, 30, -72, 20 + sw * 4);
    col(s0, s1, s2); tri(-58, 25 + sw * 3, -60, 20 + sw * 3, -72, 20 + sw * 4);
    // legs (back leg darker)
    for (int l = 0; l < 2; l++) {
        float lx = l == 0 ? -16.f : 16.f, ph = l == 0 ? 0.f : PI;
        float lift = moving ? std::max(0.f, sinf(walk + ph)) * 7 : 0, fwd = moving ? cosf(walk + ph) * 5 : 0;
        if (l == 0) col(d0, d1, d2); else col(b0, b1, b2);
        glBegin(GL_QUADS); glVertex2f(lx - 10, 36); glVertex2f(lx + 10, 36); glVertex2f(lx + 10 + fwd, 6 + lift); glVertex2f(lx - 10 + fwd, 6 + lift); glEnd();
        ellipse(lx + 4 + fwd, 6 + lift, 15, 6.5f, 20);
        col(0.96f, 0.93f, 0.82f);
        glBegin(GL_TRIANGLES);
        for (int c = 0; c < 3; c++) { float y = 2 + lift + c * 4; glVertex2f(lx + 17 + fwd, y); glVertex2f(lx + 17 + fwd, y + 3.5f); glVertex2f(lx + 23 + fwd, y + 1.7f); }
        glEnd();
    }
    // fur tufts around the outline
    col(b0, b1, b2);
    glBegin(GL_TRIANGLES);
    for (int i = 0; i < 14; i++) {
        float a0 = (100 + i * 15) * PI / 180, a1 = a0 + 0.13f, am = a0 + 0.065f;
        glVertex2f(43 * cosf(a0), 62 + 41 * sinf(a0)); glVertex2f(43 * cosf(a1), 62 + 41 * sinf(a1)); glVertex2f(50 * cosf(am), 62 + 48 * sinf(am));
    }
    for (int i = 0; i < 8; i++) {
        float a0 = (30 + i * 16) * PI / 180, a1 = a0 + 0.14f, am = a0 + 0.07f;
        glVertex2f(16 + 26 * cosf(a0), 96 + 21 * sinf(a0)); glVertex2f(16 + 26 * cosf(a1), 96 + 21 * sinf(a1)); glVertex2f(16 + 32 * cosf(am), 96 + 27 * sinf(am));
    }
    glEnd();
    // body + spots
    ellipse(0, 62, 43, 41, 44);
    col(s0, s1, s2);
    float sp[8][3] = { {-24,74,8},{-8,88,5},{-30,50,6},{-6,50,9},{14,64,6},{-14,32,6},{22,42,5},{-34,66,4} };
    for (int i = 0; i < 8; i++) ellipse(sp[i][0], sp[i][1], sp[i][2], sp[i][2] * 0.85f, 20);
    // horns
    col(0.86f, 0.78f, 0.62f);
    tri(2, 110, 11, 113, -4, 128);
    tri(17, 114, 26, 113, 14, 130);
    // head
    col(b0, b1, b2); ellipse(16, 96, 26, 21, 36);
    col(s0, s1, s2); ellipse(0, 106, 5, 4, 16);
    col(0.55f, 0.88f, 0.93f); ellipse(32, 89, 13, 9, 24);                   // muzzle
    col(0.22f, 0.10f, 0.22f); ellipse(41, 94, 5.5f, 4, 18);                  // nose
    col(1, 1, 1); ellipse(23, 102, 4.5f, 5, 16); ellipse(34, 103, 4.5f, 5, 16);
    col(0.05f, 0.05f, 0.1f); ellipse(24.5f, 102, 2.5f, 3, 12); ellipse(35.5f, 103, 2.5f, 3, 12);
    col(1, 1, 1); glPointSize(2); glBegin(GL_POINTS); glVertex2f(25.5f, 104); glVertex2f(36.5f, 105); glEnd(); glPointSize(1);
    col(0.28f, 0.12f, 0.34f); glLineWidth(2.5f);
    glBegin(GL_LINES); glVertex2f(18, 108.5f); glVertex2f(27, 110); glVertex2f(30, 110.5f); glVertex2f(38, 109); glEnd();
    glLineWidth(2);
    glBegin(GL_LINE_STRIP); glVertex2f(25, 84); glVertex2f(30, 82); glVertex2f(36, 82.5f); glVertex2f(41, 86); glEnd();
    glLineWidth(1);
    // front arm + hand + claws
    float as = moving ? -sw * 6 : 0;
    col(0.21f, 0.64f, 0.73f);
    ellipse(10, 76, 10, 9, 20);
    glBegin(GL_QUADS); glVertex2f(1, 76); glVertex2f(19, 76); glVertex2f(21 + as, 46); glVertex2f(5 + as, 46); glEnd();
    ellipse(13 + as, 44, 10, 9, 20);
    col(s0, s1, s2); ellipse(9 + as * 0.5f, 62, 3.5f, 3, 12);
    col(0.96f, 0.93f, 0.82f);
    glBegin(GL_TRIANGLES);
    for (int c = 0; c < 3; c++) { float x = 7 + as + c * 5; glVertex2f(x, 37); glVertex2f(x + 3.5f, 37); glVertex2f(x + 1.7f, 31); }
    glEnd();
}

// ---- Scream canister / laugh canister ------------------------------------
// primitives: QUADS (caps, glass, liquid), LINE_STRIP (liquid surface),
// LINE_LOOP (glass outline), LINES (glare, rays), POINTS (bubbles),
// TRIANGLES (valve), POLYGON (laugh glow)
static void drawCanister(bool laugh, float ph) {
    float l0 = 0.20f, l1 = 0.55f, l2 = 1.0f, c0 = 0.62f, c1 = 0.66f, c2 = 0.74f;
    if (laugh) {
        l0 = 1.0f; l1 = 0.85f; l2 = 0.20f; c0 = 0.95f; c1 = 0.72f; c2 = 0.22f;
        glColor4f(1, 0.9f, 0.3f, 0.18f + 0.08f * sinf(ph * 8)); ellipse(0, 0, 30, 30);
        glColor4f(1, 0.9f, 0.3f, 1); glLineWidth(2.5f);
        glBegin(GL_LINES);
        for (int k = 0; k < 8; k++) {
            float a = ph * 2.5f + k * PI / 4, r0 = 25 + 2 * sinf(ph * 10), r1 = 33 + 3 * sinf(ph * 10);
            glVertex2f(r0 * cosf(a), r0 * sinf(a)); glVertex2f(r1 * cosf(a), r1 * sinf(a));
        }
        glEnd(); glLineWidth(1);
    }
    glColor4f(1, 1, 1, 0.22f); quad(-10, -14, 10, 14);
    float lvl = 5 + sinf(ph * 4) * 1.5f;
    glColor4f(l0, l1, l2, 1); quad(-10, -14, 10, lvl);
    glColor4f(lerpf(l0, 1, 0.5f), lerpf(l1, 1, 0.5f), lerpf(l2, 1, 0.5f), 1); glLineWidth(2);
    glBegin(GL_LINE_STRIP); for (int i = 0; i <= 8; i++) { float x = -10 + i * 2.5f; glVertex2f(x, lvl + sinf(ph * 9 + i * 1.2f) * 1.2f); } glEnd();
    glPointSize(3); glColor4f(1, 1, 1, 0.85f);
    glBegin(GL_POINTS); for (int k = 0; k < 3; k++) glVertex2f(-5.f + k * 5, -13 + fmodf(ph * 16 + k * 7, lvl + 13)); glEnd();
    glPointSize(1);
    glColor4f(1, 1, 1, 0.6f); glLineWidth(2); seg(-6, -11, -6, 11); glLineWidth(1);
    glColor4f(0.12f, 0.14f, 0.2f, 1); glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP); glVertex2f(-10, -14); glVertex2f(10, -14); glVertex2f(10, 14); glVertex2f(-10, 14); glEnd(); glLineWidth(1);
    glColor4f(c0, c1, c2, 1); quad(-12, 14, 12, 20); quad(-12, -20, 12, -14);
    glColor4f(c0 * 0.6f, c1 * 0.6f, c2 * 0.6f, 1); tri(-4, 20, 4, 20, 0, 24);
}

// ---- Toxic items: stinky sock and toxic rubber duck ----------------------
// sock: POLYGON (foot, glow), QUADS (leg, stripes), LINE_STRIP (stink)
// duck: POLYGON (body, head, glow), TRIANGLES (beak), POINTS (eye), LINE_STRIP (wing)
static void drawSock(float ph) {
    glColor4f(0.4f, 1, 0.3f, 0.16f); ellipse(0, 0, 22, 22);
    glColor4f(0.95f, 0.95f, 0.9f, 1); quad(-7, -6, 7, 16); ellipse(2, -8, 13, 7, 24);
    glColor4f(0.90f, 0.20f, 0.25f, 1); quad(-7, 8, 7, 10.5f); quad(-7, 12.5f, 7, 15);
    glColor4f(0.65f, 0.75f, 0.55f, 1); ellipse(11, -9, 4, 4, 12);
    glColor4f(0.45f, 1, 0.35f, 0.9f); glLineWidth(2);
    for (int k = -1; k <= 1; k++) {
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 6; i++) glVertex2f(k * 6 + sinf(ph * 8 + i) * 2, 19 + i * 2.2f);
        glEnd();
    }
    glLineWidth(1);
}
static void drawDuck(float ph) {
    glColor4f(0.4f, 1, 0.3f, 0.16f); ellipse(0, 0, 22, 22);
    glColor4f(0.75f, 0.85f, 0.15f, 1); ellipse(-2, -4, 15, 10, 28);
    ellipse(7, 7, 8, 8, 24);
    glColor4f(1, 0.5f, 0.1f, 1); tri(13, 9, 21, 6.5f, 13, 4);
    glColor4f(0, 0, 0, 1); glPointSize(3.5f); glBegin(GL_POINTS); glVertex2f(9, 9); glEnd(); glPointSize(1);
    glColor4f(0.45f, 0.6f, 0.05f, 1); glLineWidth(2);
    glBegin(GL_LINE_STRIP); glVertex2f(-10, -2); glVertex2f(-4, -7); glVertex2f(4, -4); glEnd();
    glColor4f(0.3f, 1, 0.2f, 0.9f); glPointSize(4);
    glBegin(GL_POINTS); glVertex2f(-6, -14 - fmodf(ph * 10, 6)); glVertex2f(3, -14 - fmodf(ph * 10 + 3, 6)); glEnd();
    glPointSize(1); glLineWidth(1);
}

// ---- Power-ups: hazmat suit & speed sneaker ------------------------------
// suit:  POLYGON (glow, helmet), LINES (dashed ring), QUADS (torso, arms),
//        LINE_LOOP (helmet rim), TRIANGLES (badge), POINTS (sparkles)
// speed: POLYGON (glow, toe), LINES (dashed ring, streaks), QUADS (shoe, sole),
//        TRIANGLES (lightning bolt), POINTS (sparkles)
static void dashedRing(float r, float rot, float cr, float cg, float cb) {
    glColor4f(cr, cg, cb, 1); glLineWidth(2.5f);
    glBegin(GL_LINES);
    for (int k = 0; k < 14; k++) {
        float a0 = rot + k * 2 * PI / 14, a1 = a0 + PI / 14;
        glVertex2f(r * cosf(a0), r * sinf(a0)); glVertex2f(r * cosf(a1), r * sinf(a1));
    }
    glEnd(); glLineWidth(1);
}
static void drawSuitPU(float ph) {
    glColor4f(1, 0.7f, 0.1f, 0.2f); ellipse(0, 0, 22, 22);
    dashedRing(22, ph * 2, 1, 0.72f, 0.15f);
    glColor4f(1, 0.82f, 0.18f, 1); quad(-8, -14, 8, 2); quad(-13, -10, -8, 0); quad(8, -10, 13, 0);
    glColor4f(0.75f, 0.92f, 1, 0.9f); ellipse(0, 8, 8.5f, 8.5f, 24);
    glColor4f(0.3f, 0.5f, 0.7f, 1); glLineWidth(1.5f); ellipseLine(0, 8, 8.5f, 8.5f, 24); glLineWidth(1);
    glColor4f(0.9f, 0.2f, 0.2f, 1); tri(-4, -2, 4, -2, 0, -9);
    glColor4f(1, 1, 1, 1); glPointSize(3);
    glBegin(GL_POINTS); glVertex2f(-3, 11); glVertex2f(16 * cosf(ph * 3), 16 * sinf(ph * 3)); glEnd(); glPointSize(1);
}
static void drawSpeedPU(float ph) {
    glColor4f(0.2f, 1, 0.6f, 0.2f); ellipse(0, 0, 22, 22);
    dashedRing(22, -ph * 2, 0.3f, 0.95f, 0.6f);
    glColor4f(0.2f, 0.8f, 0.5f, 1); quad(-11, -10, 4, -1); ellipse(5, -6, 7, 4.5f, 18); quad(-11, -1, -4, 9);
    glColor4f(0.1f, 0.25f, 0.2f, 1); quad(-12, -13, 12, -10);
    glColor4f(1, 0.9f, 0.2f, 1);
    tri(2, 16, 10, 16, 3, 6); tri(1, 9, 8, 9, -1, -3);
    glColor4f(0.6f, 1, 0.8f, 1); glLineWidth(2);
    glBegin(GL_LINES); for (int k = 0; k < 3; k++) { glVertex2f(-19, -8 + k * 6.f); glVertex2f(-14, -8 + k * 6.f); } glEnd();
    glLineWidth(1);
    glColor4f(1, 1, 1, 1); glPointSize(3);
    glBegin(GL_POINTS); glVertex2f(-16 * cosf(ph * 3), 16 * sinf(ph * 3)); glEnd(); glPointSize(1);
}

static void drawItem(const Item& it) {
    glPushMatrix(); glTranslatef(it.x, it.y, 0);
    switch (it.type) {
    case IT_CANISTER: case IT_LAUGH:          // collectables: pendulum swing
        glRotatef(sinf(it.phase * 5) * 14, 0, 0, 1); drawCanister(it.type == IT_LAUGH, it.phase); break;
    case IT_SOCK: glRotatef(it.angle, 0, 0, 1); drawSock(it.phase); break;    // obstacles: spin
    case IT_DUCK: glRotatef(it.angle, 0, 0, 1); drawDuck(it.phase); break;
    default: {                                  // power-ups: pulse in size
        float s = 1.f + 0.12f * sinf(it.phase * 7); glScalef(s, s, 1);
        if (it.type == IT_SUIT) drawSuitPU(it.phase); else drawSpeedPU(it.phase);
    }
    }
    glPopMatrix();
}

// ============================================================================
//  HUD
// ============================================================================
static void drawHUD() {
    float hb[3] = { 0.05f,0.05f,0.09f }, ht[3] = { 0.13f,0.13f,0.21f };
    quadGrad(0, HUD_Y0, W, H, hb, ht);
    glColor4f(1, 0.75f, 0.1f, 1); glLineWidth(2); seg(0, HUD_Y0, W, HUD_Y0); glLineWidth(1);
    // health: five little eyeballs (polygon + points); a lost life is a shut eye (lines)
    for (int i = 0; i < MAX_LIVES; i++) {
        float x = 26 + i * 30.f, y = 676;
        if (i < P.lives) {
            glColor4f(0.56f, 0.80f, 0.20f, 1); ellipse(x, y, 12, 12, 24);
            glColor4f(1, 1, 1, 1); ellipse(x, y, 8.5f, 8.5f, 24);
            glColor4f(0.15f, 0.62f, 0.52f, 1); ellipse(x + 1.5f, y, 4.5f, 4.5f, 16);
            glColor4f(0, 0, 0, 1); glPointSize(3.5f); glBegin(GL_POINTS); glVertex2f(x + 2, y); glEnd();
            glColor4f(1, 1, 1, 1); glPointSize(2); glBegin(GL_POINTS); glVertex2f(x + 3.5f, y + 2.5f); glEnd(); glPointSize(1);
        } else {
            glColor4f(0.25f, 0.27f, 0.33f, 1); ellipse(x, y, 12, 12, 24);
            glColor4f(0.55f, 0.15f, 0.2f, 1); glLineWidth(2.5f);
            glBegin(GL_LINES); glVertex2f(x - 6, y - 6); glVertex2f(x + 6, y + 6); glVertex2f(x - 6, y + 6); glVertex2f(x + 6, y - 6); glEnd();
            glLineWidth(1);
        }
    }
    // score
    char buf[64]; sprintf(buf, "SCORE %d", gScore);
    TextFx st = mkText(17, PAL_GOLD); st.tracking = 0.12f;
    drawText(buf, 182, 668, st);
    // scream meter: canister icon + bar (quads, line loop, lines)
    float mx0 = 360, mx1 = 600, my0 = 667, my1 = 686;
    glPushMatrix(); glTranslatef(343, 676.5f, 0); glScalef(0.5f, 0.5f, 1); drawCanister(false, gReal); glPopMatrix();
    glColor4f(0.08f, 0.09f, 0.14f, 1); quad(mx0, my0, mx1, my1);
    float fill = clampf(gScore / (float)QUOTA, 0, 1);
    float fb[3] = { 1.0f,0.55f,0.05f }, ftc[3] = { 1.0f,0.92f,0.35f };
    if (fill > 0) quadGrad(mx0, my0, mx0 + (mx1 - mx0) * fill, my1, fb, ftc);
    glColor4f(1, 1, 1, 0.25f); if (fill > 0) quad(mx0, my1 - 5, mx0 + (mx1 - mx0) * fill, my1 - 3);
    glColor4f(0, 0, 0, 0.45f);
    glBegin(GL_LINES); for (int k = 1; k < 10; k++) { float x = mx0 + (mx1 - mx0) * k / 10; glVertex2f(x, my0); glVertex2f(x, my0 + 5); } glEnd();
    glColor4f(0.85f, 0.87f, 0.95f, 1); glLineWidth(2);
    glBegin(GL_LINE_LOOP); glVertex2f(mx0, my0); glVertex2f(mx1, my0); glVertex2f(mx1, my1); glVertex2f(mx0, my1); glEnd();
    glLineWidth(1);
    TextFx ml = mkText(9, PAL_WHITE); ml.shadow = false; ml.tracking = 0.3f;
    drawText("SCREAM METER", (mx0 + mx1) / 2, 671.5f, ml, 1);
    // Door Station status: mini door + recharge pie (triangles) + rim (line loop)
    float sx = 650, sy = 676;
    bool ready = gStationCD <= 0;
    glColor4f(0.24f, 0.2f, 0.22f, 1); quad(sx - 9, sy - 13, sx + 9, sy + 13);
    if (ready) glColor4f(0.3f, 0.85f, 0.55f, 1); else glColor4f(0.45f, 0.45f, 0.5f, 1);
    quad(sx - 6.5f, sy - 11, sx + 6.5f, sy + 11);
    glColor4f(1, 0.85f, 0.3f, 1); glPointSize(3); glBegin(GL_POINTS); glVertex2f(sx + 3.5f, sy); glEnd(); glPointSize(1);
    float gx = 682, gr = 11;
    float frac = ready ? 1.f : 1.f - gStationCD / STATION_COOLDOWN;
    glColor4f(0.12f, 0.12f, 0.18f, 1); ellipse(gx, sy, gr, gr, 24);
    if (ready) glColor4f(0.3f, 1, 0.5f, 0.6f + 0.4f * sinf(gReal * 6)); else glColor4f(1, 0.45f, 0.25f, 1);
    glBegin(GL_TRIANGLES);
    int segs = (int)(frac * 32);
    for (int i = 0; i < segs; i++) {
        float a0 = PI / 2 - 2 * PI * i / 32, a1 = PI / 2 - 2 * PI * (i + 1) / 32;
        glVertex2f(gx, sy); glVertex2f(gx + gr * cosf(a0), sy + gr * sinf(a0)); glVertex2f(gx + gr * cosf(a1), sy + gr * sinf(a1));
    }
    glEnd();
    glColor4f(0.85f, 0.87f, 0.95f, 1); glLineWidth(1.5f); ellipseLine(gx, sy, gr, gr, 24); glLineWidth(1);
    TextFx sl = mkText(11, ready ? PAL_GREEN : PAL_RED); sl.tracking = 0.15f;
    drawText(ready ? "STATION READY" : "RECHARGING", 702, 671, sl);
    // shift timer: clock icon (polygon + lines) + text
    float rem = std::max(0.f, SHIFT_TIME - gElapsed);
    int secs = (int)ceilf(rem);
    float cxk = 880;
    glColor4f(0.9f, 0.92f, 1, 1); ellipse(cxk, sy, 11, 11, 24);
    glColor4f(0.1f, 0.1f, 0.2f, 1); glLineWidth(2);
    float ha = PI / 2 - 2 * PI * (gElapsed / SHIFT_TIME), ma = PI / 2 - 2 * PI * fmodf(gElapsed, 10.f) / 10.f;
    glBegin(GL_LINES); glVertex2f(cxk, sy); glVertex2f(cxk + 5 * cosf(ha), sy + 5 * sinf(ha)); glVertex2f(cxk, sy); glVertex2f(cxk + 8 * cosf(ma), sy + 8 * sinf(ma)); glEnd();
    glLineWidth(1);
    sprintf(buf, "%02d:%02d", secs / 60, secs % 60);
    TextFx tt = mkText(19, rem < 10 && ((int)(gReal * 4) % 2) ? PAL_RED : PAL_WHITE); tt.shear = 0.1f;
    drawText(buf, 985, 666, tt, 2);
}

// ============================================================================
//  Scenes
// ============================================================================
static void drawWorld(bool play) {
    drawBackground(gClock);
    drawRail();
    for (int i = 0; i < 4; i++) drawDoor(doors[i], i, play && gHover == i && !gPaused);
    drawFloor();
    if (!play) return;
    // rival
    gCamo = Rv.camo;
    glPushMatrix(); glTranslatef(Rv.x, FLOOR_Y, 0); glScalef((float)Rv.dir, 1, 1);
    drawSulley(Rv.walk, true);
    glPopMatrix();
    if (Rv.camo > 0.6f) {                 // faint shimmer so the camouflage reads on screen
        glColor4f(0.6f, 0.7f, 1, 0.18f * Rv.camo); glLineWidth(1.5f);
        glPushMatrix(); glTranslatef(Rv.x, FLOOR_Y, 0); ellipseLine(0, 62, 44 + sinf(gClock * 6) * 2, 42, 40); glPopMatrix();
        glLineWidth(1);
    }
    gCamo = 0;
    for (size_t i = 0; i < items.size(); i++) drawItem(items[i]);
    // player
    float shiver = P.frozen > 0 ? sinf(gClock * 80) * 2 : 0;
    glPushMatrix(); glTranslatef(P.x + shiver, P.y, 0); glScalef((float)P.dir, 1, 1);
    drawMike(P.walk, P.moving && P.onGround, P.squash, P.blinkT < 0, P.frozen > 0 ? 1 : 0,
             P.suit > 0 && (P.suit > 1.5f || (int)(gClock * 8) % 2), P.speed > 0 && (P.speed > 1.5f || (int)(gClock * 8) % 2), gClock);
    glPopMatrix();
    // particles + popups
    for (size_t i = 0; i < parts.size(); i++) {
        const Particle& p = parts[i];
        glPointSize(p.size); glColor4f(p.r, p.g, p.b, p.life / p.maxLife);
        glBegin(GL_POINTS); glVertex2f(p.x, p.y); glEnd();
    }
    glPointSize(1);
    for (size_t i = 0; i < popups.size(); i++) {
        TextFx f = mkText(popups[i].text == "2319!" ? 30.f : 15.f, popups[i].pal);
        f.alpha = clampf(popups[i].life * 2, 0, 1);
        drawText(popups[i].text.c_str(), popups[i].x, popups[i].y, f, 1);
    }
}

static void drawOverlay(float a) { glColor4f(0.02f, 0.02f, 0.06f, a); quad(0, 0, W, H); }

static void drawStartScreen() {
    drawWorld(false);
    drawOverlay(0.62f);
    // the two monsters, standing on the floor either side
    glPushMatrix(); glTranslatef(130, FLOOR_Y, 0); glScalef(1.5f, 1.5f, 1);
    drawMike(gReal * 3, false, 0, fmodf(gReal, 3.5f) < 0.15f, 2, false, false, gReal); glPopMatrix();
    glPushMatrix(); glTranslatef(875, FLOOR_Y, 0); glScalef(-1.35f, 1.35f, 1); drawSulley(0, false); glPopMatrix();
    // title
    TextFx t = mkText(76, PAL_GREEN); t.wave = 5; t.tracking = 0.1f; t.weight = 1.25f;
    drawText("SCREAM SHIFT", W / 2, 520, t, 1, gReal);
    TextFx sub = mkText(17, PAL_TEAL); sub.tracking = 0.35f;
    drawText("A MONSTERS ON THE SCARE FLOOR ADVENTURE", W / 2, 478, sub, 1);
    // controls panel
    glColor4f(0.08f, 0.09f, 0.17f, 0.92f); roundRect(270, 250, 730, 455, 18);
    glColor4f(1, 0.75f, 0.1f, 0.9f); glLineWidth(2.5f); roundRectLine(270, 250, 730, 455, 18); glLineWidth(1);
    TextFx h = mkText(18, PAL_GOLD); h.tracking = 0.25f;
    drawText("HOW TO WORK YOUR SHIFT", W / 2, 424, h, 1);
    const char* lines[4][2] = {
        {"A / D  OR  ARROWS", "MOVE"}, {"W / SPACE / UP", "JUMP"},
        {"CLICK A DOOR", "DOOR STATION"}, {"P / ESC", "PAUSE / QUIT"} };
    for (int i = 0; i < 4; i++) {
        TextFx k = mkText(14, PAL_WHITE), v = mkText(14, PAL_TEAL);
        drawText(lines[i][0], 488, 388 - i * 32.f, k, 2);
        glColor4f(1, 0.75f, 0.1f, 1); ellipse(500, 394 - i * 32.f, 3, 3, 10);
        drawText(lines[i][1], 512, 388 - i * 32.f, v, 0);
    }
    // legend row of collectables / hazards / power-ups
    struct L { int type; const char* txt; int pal; };
    L lg[6] = { {IT_CANISTER,"+10",PAL_BLUE}, {IT_LAUGH,"+50",PAL_GOLD}, {IT_SOCK,"2319!",PAL_RED},
                {IT_DUCK,"2319!",PAL_RED}, {IT_SUIT,"IMMUNE",PAL_GOLD}, {IT_SPEED,"FAST",PAL_GREEN} };
    for (int i = 0; i < 6; i++) {
        Item it; it.type = lg[i].type; it.x = 300 + i * 80.f; it.y = 210; it.angle = gReal * 120; it.phase = gReal; it.spin = 0; it.alive = true;
        drawItem(it);
        TextFx f = mkText(11, lg[i].pal); f.tracking = 0.12f;
        drawText(lg[i].txt, it.x, 168, f, 1);
    }
    TextFx go = mkText(21, PAL_GOLD); go.tracking = 0.18f; go.alpha = 0.55f + 0.45f * sinf(gReal * 4);
    drawText("PRESS ANY KEY OR CLICK TO CLOCK IN", W / 2, 112, go, 1);
}

static void drawEndScreen() {
    float c[3][3] = { {0.10f,0.42f,0.24f}, {0.45f,0.08f,0.12f}, {0.10f,0.15f,0.30f} };
    float* bc = c[gEnd];
    float top[3] = { bc[0] * 1.3f, bc[1] * 1.3f, bc[2] * 1.3f }, bot[3] = { bc[0] * 0.55f, bc[1] * 0.55f, bc[2] * 0.55f };
    quadGrad(0, 0, W, H, bot, top);
    glColor4f(1, 1, 1, 0.05f);                 // moving diagonal stripes
    glBegin(GL_QUADS);
    float off = fmodf(gReal * 40, 80);
    for (float x = -H; x < W + 80; x += 80) { glVertex2f(x + off, 0); glVertex2f(x + off + 36, 0); glVertex2f(x + off + 36 + H, H); glVertex2f(x + off + H, H); }
    glEnd();
    const char* title = gEnd == END_QUOTA ? "QUOTA REACHED!" : gEnd == END_LOSE ? "GAME LOSE" : "SHIFT OVER";
    int pal = gEnd == END_QUOTA ? PAL_GREEN : gEnd == END_LOSE ? PAL_RED : PAL_BLUE;
    TextFx t = mkText(70, pal); t.wave = 4; t.weight = 1.25f;
    drawText(title, W / 2, 520, t, 1, gReal);
    char buf[96];
    sprintf(buf, "SCORE  %d", gScore);
    TextFx s = mkText(36, PAL_GOLD); s.tracking = 0.15f;
    drawText(buf, W / 2, 440, s, 1);
    // stats panel
    glColor4f(0, 0, 0, 0.3f); roundRect(250, 300, 750, 400, 18);
    glColor4f(1, 1, 1, 0.5f); glLineWidth(2); roundRectLine(250, 300, 750, 400, 18); glLineWidth(1);
    const char* lab[3] = { "CAUGHT", "MISSED", "STOLEN" };
    int val[3] = { gCaught, gMissed, gStolen };
    int pals[3] = { PAL_BLUE, PAL_WHITE, PAL_PURPLE };
    for (int i = 0; i < 3; i++) {
        float x = 335 + i * 165.f;
        sprintf(buf, "%d", val[i]);
        drawText(buf, x, 352, mkText(30, pals[i]), 1);
        TextFx l = mkText(12, PAL_WHITE); l.tracking = 0.3f;
        drawText(lab[i], x, 318, l, 1);
    }
    // monsters react
    glPushMatrix(); glTranslatef(500, 110, 0); glScalef(1.5f, 1.5f, 1);
    drawMike(gReal * 6, gEnd == END_QUOTA, gEnd == END_QUOTA ? fabsf(sinf(gReal * 5)) * 0.6f : 0, false,
             gEnd == END_QUOTA ? 2 : 1, false, false, gReal);
    glPopMatrix();
    glPushMatrix(); glTranslatef(820, 110, 0); glScalef(-1.2f, 1.2f, 1); drawSulley(0, false); glPopMatrix();
    glColor4f(0, 0, 0, 0.25f); quad(0, 0, W, 110);
    TextFx r = mkText(18, PAL_WHITE); r.tracking = 0.2f; r.alpha = 0.6f + 0.4f * sinf(gReal * 4);
    drawText("PRESS R FOR A NEW SHIFT   -   ESC TO QUIT", W / 2, 45, r, 1);
}

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    if (gState == ST_START) drawStartScreen();
    else if (gState == ST_END) drawEndScreen();
    else {
        drawWorld(true);
        drawHUD();
        if (gAlarm > 0) {                     // 2319 alarm: screen flashes red / yellow
            bool red = ((int)(gAlarm * 10)) % 2 == 0;
            if (red) glColor4f(1, 0.1f, 0.1f, 0.35f); else glColor4f(1, 0.9f, 0.1f, 0.30f);
            quad(0, 0, W, H);
            TextFx a = mkText(64, PAL_RED); a.weight = 1.3f; a.wave = 3;
            drawText("2319!", W / 2 + sinf(gClock * 60) * 4, 370, a, 1, gClock * 3);
            TextFx b = mkText(18, PAL_GOLD); b.tracking = 0.3f;
            drawText("DECONTAMINATING...", W / 2, 330, b, 1);
        }
        if (gPaused) {
            drawOverlay(0.6f);
            TextFx p = mkText(72, PAL_GOLD); p.wave = 4; p.weight = 1.2f;
            drawText("PAUSED", W / 2, 360, p, 1, gReal);
            TextFx s = mkText(18, PAL_WHITE); s.tracking = 0.25f; s.alpha = 0.6f + 0.4f * sinf(gReal * 4);
            drawText("PRESS P TO GET BACK TO WORK", W / 2, 300, s, 1);
        }
    }
    glutSwapBuffers();
}

// ============================================================================
//  GLUT callbacks
// ============================================================================
static void updateHover() {
    gHover = -1;
    for (int i = 0; i < 4; i++)
        if (gMouseX > doors[i].x - DOOR_W / 2 - 6 && gMouseX < doors[i].x + DOOR_W / 2 + 6 &&
            gMouseY > DOOR_TOP - DOOR_H - 3 && gMouseY < DOOR_TOP + 6) gHover = i;
}
static void toScene(int x, int y) { gMouseX = x * W / winW; gMouseY = (winH - y) * H / winH; updateHover(); }

static void onTimer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = clampf((now - gLastMs) / 1000.f, 0, 0.05f);
    gLastMs = now;
    update(dt);
    if (gState == ST_PLAY) updateHover();
    glutPostRedisplay();
    glutTimerFunc(16, onTimer, 0);
}

static void quit() { Audio::shutdown(); exit(0); }

static void setKey(unsigned char k, bool down) {
    k = (unsigned char)tolower(k);
    if (k == 'a') kA = down;
    if (k == 'd') kD = down;
    if (down && (k == 'w' || k == ' ')) gJumpQueued = true;
}
static void onKey(unsigned char k, int, int) {
    if (k == 27) quit();
    if (gState == ST_START) { startGame(); return; }
    if (gState == ST_END) { if (tolower(k) == 'r') startGame(); return; }
    if (tolower(k) == 'p') { gPaused = !gPaused; Audio::pauseMusic(gPaused); kA = kD = kL = kR = false; return; }
    if (!gPaused) setKey(k, true);
}
static void onKeyUp(unsigned char k, int, int) { setKey(k, false); }
static void onSpecial(int k, int, int) {
    if (gState == ST_START) { startGame(); return; }
    if (gState != ST_PLAY || gPaused) return;
    if (k == GLUT_KEY_LEFT) kL = true;
    if (k == GLUT_KEY_RIGHT) kR = true;
    if (k == GLUT_KEY_UP) gJumpQueued = true;
}
static void onSpecialUp(int k, int, int) {
    if (k == GLUT_KEY_LEFT) kL = false;
    if (k == GLUT_KEY_RIGHT) kR = false;
}
static void onMouse(int btn, int state, int x, int y) {
    toScene(x, y);
    if (state != GLUT_DOWN) return;
    if (gState == ST_START) { startGame(); return; }
    if (gState != ST_PLAY || gPaused || btn != GLUT_LEFT_BUTTON || gEndDelay > 0) return;
    if (gHover < 0) return;
    Door& d = doors[gHover];
    if (gStationCD > 0) { popup("RECHARGING...", d.x, DOOR_TOP - DOOR_H - 20, PAL_RED); return; }
    if (d.state != D_IDLE && d.state != D_WARN) { popup("DOOR BUSY", d.x, DOOR_TOP - DOOR_H - 20, PAL_WHITE); return; }
    d.state = D_OPENING; d.t = 0; d.forced = IT_CANISTER; d.flash = 0.4f;   // open immediately
    gStationCD = STATION_COOLDOWN;
}
static void onMotion(int x, int y) { toScene(x, y); }

static void reshape(int w, int h) {
    winW = w; winH = h > 0 ? h : 1;
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, W, 0, H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    srand((unsigned)time(0));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1000, 700);
    glutInitWindowPosition(std::max(0, (glutGet(GLUT_SCREEN_WIDTH) - 1000) / 2), std::max(0, (glutGet(GLUT_SCREEN_HEIGHT) - 700) / 2 - 20));
    glutCreateWindow("Scream Shift");
    glClearColor(0.05f, 0.05f, 0.08f, 1);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POINT_SMOOTH); glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    GLfloat lr[2] = { 1, 1 }; glGetFloatv(GL_LINE_WIDTH_RANGE, lr);
    gLineMax = clampf(lr[1], 1.f, 8.f);
    resetGame();
    Audio::init();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(onKey);
    glutKeyboardUpFunc(onKeyUp);
    glutSpecialFunc(onSpecial);
    glutSpecialUpFunc(onSpecialUp);
    glutMouseFunc(onMouse);
    glutPassiveMotionFunc(onMotion);
    glutMotionFunc(onMotion);
    glutIgnoreKeyRepeat(1);
    gLastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, onTimer, 0);
    glutMainLoop();
    return 0;
}
