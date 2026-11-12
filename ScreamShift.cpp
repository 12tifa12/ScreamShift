// ============================================================================
//  SCREAM SHIFT - DMET B402 Computer Graphics, Assignment 1 (Winter 2026)
//
//  A 2D side-view Scare Floor game. Catch scream canisters falling from the
//  closet doors, dodge the toxic children's items, and watch out for the
//  rival monster who steals canisters.
//
//  Controls:
//    A / D        : move left / right
//    W / SPACE    : jump
//    Mouse        : hover over a door and left-click it (Door Station)
//    P            : pause / resume
//    R            : start a new shift (on the end screen)
//    ESC          : quit
// ============================================================================

//  Includes
#define _CRT_SECURE_NO_WARNINGS   // lets sprintf compile in Visual Studio
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <windows.h>              // needed for the sound functions
#include <mmsystem.h>
#include <glut.h>
#pragma comment(lib, "winmm.lib") // links the Windows sound library
//-----------------

//  Constants
#define PI 3.14159265f
const float DT = 0.016f;          // time of one frame in seconds (timer runs every 16 ms)
const float FLOOR_Y = 90;         // top of the Scare Floor
const float RAIL_Y = 618;         // bottom of the door rail
const float HUD_Y = 652;          // bottom of the HUD bar
const float DOOR_W = 70, DOOR_H = 106, DOOR_TOP = 610;
const float SHIFT_TIME = 90;      // length of a shift in seconds
const int   QUOTA = 300;          // scream points needed to fill the meter
const float STATION_COOLDOWN = 10;
const float POWER_TIME = 6;       // how long a power-up lasts

// item types
const int ITEM_CANISTER = 0, ITEM_LAUGH = 1, ITEM_SOCK = 2, ITEM_DUCK = 3, ITEM_SUIT = 4, ITEM_SPEED = 5;
// door states
const int DOOR_CLOSED = 0, DOOR_WARNING = 1, DOOR_OPENING = 2, DOOR_OPEN = 3, DOOR_CLOSING = 4;
// game states
const int STATE_START = 0, STATE_PLAYING = 1, STATE_END = 2;
// how the game ended
const int END_QUOTA = 0, END_LOSE = 1, END_SHIFT = 2;
//-----------------

//  Types
struct Door  { float x, speed, timer, open; int state; bool forced, dropped; };
struct Item  { bool active; int type; float x, y, angle, phase; };
struct Popup { bool active; char text[20]; float x, y, life, r, g, b; };
//-----------------

//  Global Variables
int gameState = STATE_START, endType = END_SHIFT;
bool paused = false;
float animTime = 0;               // drives all animations (stops while paused)
float gameTime = 0;               // seconds since the shift started
int score = 0, caught = 0, missed = 0, stolen = 0;
float stationTimer = 0;           // Door Station recharge time left (0 = ready)
float alarmTime = 0;              // 2319 alarm time left
float nextDropTime = 1;           // time until the next door opens by itself
int nextPowerup = 0;              // index into powerupTimes
float powerupTimes[4] = { 9, 24, 44, 62 };   // guarantees 2 power-ups of each type

// player
float playerX, playerY, playerVY, pushSpeed, playerWalk;
int playerDir, lives;
bool onGround, playerMoving;
float frozenTime, suitTime, speedTime;

// rival
float rivalX, rivalWalk, camo, camoTimer;
int rivalDir;

Door doors[4];
Item items[30];
Popup popups[10];

// input
bool keyLeft = false, keyRight = false, jumpPressed = false;
float mouseX = -100, mouseY = -100;
int hoverDoor = -1;               // index of the door under the mouse (-1 = none)

// camouflage: setColor() mixes every colour with the wall colour by this amount
float camoAmount = 0;
//-----------------


// ============================================================================
//  SOUND (bonus)
//  All sounds are generated here as arrays of samples, so no sound files are
//  needed. The music and the effects use two separate output devices, so they
//  can play at the same time.
// ============================================================================
const int RATE = 22050;           // samples per second
short musicSound[RATE * 15];
short catchSound[RATE / 4], powerSound[RATE / 2], alarmSound[RATE];
short stealSound[RATE / 2], winSound[RATE * 3 / 2], loseSound[RATE * 2];
int musicLength;
HWAVEOUT musicDevice, effectDevice;
WAVEHDR musicHeader, effectHeader;

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

// frequency of a piano key number (69 = A4 = 440 Hz)
float noteFreq(int note) { return 440 * powf(2, (note - 69) / 12.0f); }

// adds a tone that slides from freq1 to freq2 and fades out
void addTone(short* buf, int len, float start, float dur, float freq1, float freq2, float vol, bool square) {
	int first = (int)(start * RATE), count = (int)(dur * RATE);
	float phase = 0;
	for (int i = 0; i < count && first + i < len; i++) {
		float freq = freq1 + (freq2 - freq1) * i / count;
		phase += freq / RATE;
		float wave = sinf(2 * PI * phase);
		if (square) wave = wave > 0 ? 0.6f : -0.6f;
		float fade = 1 - (float)i / count;          // fade out
		if (i < 100) fade *= i / 100.0f;            // short fade in (avoids clicks)
		int value = buf[first + i] + (int)(wave * fade * vol * 12000);
		if (value > 32000) value = 32000;
		if (value < -32000) value = -32000;
		buf[first + i] = (short)value;
	}
}

void createSounds() {
	// background music: walking bass + melody, 8 bars of 4 beats
	float beat = 60 / 132.0f;
	musicLength = (int)(32 * beat * RATE);
	int bass[32] = { 50,53,57,60, 55,59,62,59, 60,57,55,52, 57,61,64,61,
	                 50,52,53,57, 58,57,56,55, 57,52,55,61, 50,57,50,49 };
	int melody[64] = { 74,72,69,0,65,69,69,0,  71,74,77,77,0,74,71,0,
	                   76,76,72,67,64,64,67,0, 73,76,79,76,73,73,73,0,
	                   74,72,69,0,65,69,69,0,  74,77,82,82,80,77,0,0,
	                   76,73,69,67,73,73,76,0, 74,74,74,0,69,62,62,0 };
	for (int i = 0; i < 32; i++)
		addTone(musicSound, musicLength, i * beat, beat * 0.9f, noteFreq(bass[i]), noteFreq(bass[i]), 0.35f, false);
	for (int i = 0; i < 64; i++)
		if (melody[i] > 0)
			addTone(musicSound, musicLength, i * beat / 2, beat * 0.45f, noteFreq(melody[i]), noteFreq(melody[i]), 0.12f, true);

	// catching a canister / power-up: two quick high blips
	addTone(catchSound, LENGTH(catchSound), 0, 0.08f, 880, 880, 0.5f, true);
	addTone(catchSound, LENGTH(catchSound), 0.08f, 0.14f, 1320, 1320, 0.5f, true);
	// power-up: rising arpeggio
	int arp[4] = { 72, 76, 79, 84 };
	for (int i = 0; i < 4; i++) addTone(powerSound, LENGTH(powerSound), i * 0.08f, 0.15f, noteFreq(arp[i]), noteFreq(arp[i]), 0.8f, false);
	// 2319 alarm: siren switching between two pitches
	for (int i = 0; i < 8; i++) addTone(alarmSound, LENGTH(alarmSound), i * 0.125f, 0.12f, i % 2 ? 700.0f : 980.0f, i % 2 ? 700.0f : 980.0f, 0.6f, true);
	// rival steals: falling "wah"
	addTone(stealSound, LENGTH(stealSound), 0, 0.45f, 520, 140, 0.7f, true);
	// quota reached: fanfare
	int fan[4] = { 67, 72, 76, 79 };
	for (int i = 0; i < 4; i++) addTone(winSound, LENGTH(winSound), i * 0.11f, 0.11f, noteFreq(fan[i]), noteFreq(fan[i]), 0.5f, true);
	addTone(winSound, LENGTH(winSound), 0.45f, 0.9f, noteFreq(84), noteFreq(84), 0.6f, false);
	addTone(winSound, LENGTH(winSound), 0.45f, 0.9f, noteFreq(76), noteFreq(76), 0.4f, false);
	// game lose / shift over: sad descending notes
	int sad[4] = { 63, 62, 61, 60 };
	for (int i = 0; i < 4; i++) addTone(loseSound, LENGTH(loseSound), i * 0.45f, i == 3 ? 0.8f : 0.4f, noteFreq(sad[i]), noteFreq(sad[i]), 0.5f, true);
}

// plays an array of samples on a device (stops whatever that device was playing)
void playOn(HWAVEOUT device, WAVEHDR* header, short* data, int length, bool loop) {
	waveOutReset(device);
	waveOutUnprepareHeader(device, header, sizeof(WAVEHDR));
	ZeroMemory(header, sizeof(WAVEHDR));
	header->lpData = (LPSTR)data;
	header->dwBufferLength = length * 2;            // 2 bytes per sample
	waveOutPrepareHeader(device, header, sizeof(WAVEHDR));
	if (loop) { header->dwFlags |= WHDR_BEGINLOOP | WHDR_ENDLOOP; header->dwLoops = 100000; }
	waveOutWrite(device, header, sizeof(WAVEHDR));
}
void playEffect(short* data, int length) { playOn(effectDevice, &effectHeader, data, length, false); }

void initSound() {
	createSounds();
	WAVEFORMATEX format = { WAVE_FORMAT_PCM, 1, RATE, RATE * 2, 2, 16, 0 };   // mono, 16-bit
	waveOutOpen(&musicDevice, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
	waveOutOpen(&effectDevice, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
}


// ============================================================================
//  DRAWING HELPERS
// ============================================================================
void setColor(float r, float g, float b, float a = 1) {
	// mix with the wall colour when camouflaged, and fade out
	r += (0.17f - r) * camoAmount;
	g += (0.19f - g) * camoAmount;
	b += (0.27f - b) * camoAmount;
	glColor4f(r, g, b, a * (1 - 0.85f * camoAmount));
}

void drawRect(float x1, float y1, float x2, float y2) {       // GL_QUADS
	glBegin(GL_QUADS);
	glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
	glEnd();
}

void drawRectLine(float x1, float y1, float x2, float y2) {   // GL_LINE_LOOP
	glBegin(GL_LINE_LOOP);
	glVertex2f(x1, y1); glVertex2f(x2, y1); glVertex2f(x2, y2); glVertex2f(x1, y2);
	glEnd();
}

void drawEllipse(float cx, float cy, float rx, float ry) {    // GL_POLYGON
	glBegin(GL_POLYGON);
	for (int i = 0; i < 30; i++) {
		float a = 2 * PI * i / 30;
		glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a));
	}
	glEnd();
}
void drawCircle(float cx, float cy, float r) { drawEllipse(cx, cy, r, r); }

void drawCircleLine(float cx, float cy, float r) {            // GL_LINE_LOOP
	glBegin(GL_LINE_LOOP);
	for (int i = 0; i < 30; i++) {
		float a = 2 * PI * i / 30;
		glVertex2f(cx + r * cosf(a), cy + r * sinf(a));
	}
	glEnd();
}

void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3) {   // GL_TRIANGLES
	glBegin(GL_TRIANGLES);
	glVertex2f(x1, y1); glVertex2f(x2, y2); glVertex2f(x3, y3);
	glEnd();
}

// Text with the GLUT stroke font, drawn 4 times to look chunky:
// shadow -> dark outline -> coloured fill -> thin white shine.
// align: 0 = left, 1 = centre, 2 = right.
void drawText(const char* text, float x, float y, float size, float r, float g, float b, int align = 0) {
	float scale = size / 120;             // stroke font capitals are about 120 units high
	float spacing = 20;                   // extra space between letters (font units)
	float width = 0;
	for (int i = 0; text[i]; i++) width += glutStrokeWidth(GLUT_STROKE_ROMAN, text[i]) + spacing;
	width *= scale;
	if (align == 1) x -= width / 2;
	if (align == 2) x -= width;

	float thick = 1.5f + size * 0.09f;    // line width of the fill in pixels
	if (thick > 5) thick = 5;             // graphics cards limit how wide a line can be
	float bold = size > 30 ? size * 0.03f : 0;   // big text: also draw copies shifted by this much
	float offX[5] = { 0, bold, -bold, 0, 0 }, offY[5] = { 0, 0, 0, bold, -bold };

	for (int pass = 0; pass < 4; pass++) {
		float dx = 0, dy = 0;
		if (pass == 0) { glColor4f(0, 0, 0, 0.5f); glLineWidth(thick + 4); dx = 2 + size * 0.04f; dy = -dx; }
		if (pass == 1) { glColor3f(0.12f, 0.05f, 0.2f); glLineWidth(thick + 4); }
		if (pass == 2) { glColor3f(r, g, b); glLineWidth(thick); }
		if (pass == 3) { glColor4f(1, 1, 1, 0.45f); glLineWidth(thick * 0.35f); dy = thick * 0.25f + bold; }
		int copies = (pass == 3 || bold == 0) ? 1 : 5;
		for (int c = 0; c < copies; c++) {
			glPushMatrix();
			glTranslatef(x + dx + offX[c], y + dy + offY[c], 0);
			glScalef(scale, scale, 1);
			for (int i = 0; text[i]; i++) {
				glutStrokeCharacter(GLUT_STROKE_ROMAN, text[i]);   // moves right by the letter width
				glTranslatef(spacing, 0, 0);
			}
			glPopMatrix();
		}
	}
	glLineWidth(1);
}


// ============================================================================
//  MODELS
// ============================================================================

// ---- Background wall (animated chasing lights and spinning fans) ----------
void drawBackground() {
	glBegin(GL_QUADS);                          // wall with a vertical colour gradient
	glColor3f(0.12f, 0.13f, 0.20f); glVertex2f(0, FLOOR_Y); glVertex2f(1000, FLOOR_Y);
	glColor3f(0.20f, 0.22f, 0.31f); glVertex2f(1000, RAIL_Y); glVertex2f(0, RAIL_Y);
	glEnd();
	glColor3f(0.10f, 0.11f, 0.17f);             // panel seams
	glLineWidth(2);
	glBegin(GL_LINES);
	for (int x = 125; x < 1000; x += 125) { glVertex2f(x, FLOOR_Y); glVertex2f(x, RAIL_Y); }
	glEnd();
	glLineWidth(1);
	glColor3f(0.30f, 0.33f, 0.45f);
	drawRect(0, 466, 1000, 472);                // trim

	// row of lights: two lights at a time travel along the row
	int lit = (int)(animTime * 5) % 8;
	for (int k = 0; k < 8; k++) {
		float x = 62.5f + k * 125;
		if (k == lit || k == (lit + 4) % 8) {
			glColor4f(1, 0.85f, 0.3f, 0.25f); drawCircle(x, 486, 13);
			glColor3f(1, 0.85f, 0.3f);
		} else glColor3f(0.35f, 0.37f, 0.45f);
		drawCircle(x, 486, 6);
	}

	// two ventilation fans that spin in opposite directions
	float fanX[2] = { 110, 890 };
	for (int f = 0; f < 2; f++) {
		glColor3f(0.10f, 0.11f, 0.16f); drawCircle(fanX[f], 400, 36);
		glColor3f(0.40f, 0.43f, 0.55f); glLineWidth(3); drawCircleLine(fanX[f], 400, 36); glLineWidth(1);
		glPushMatrix();
		glTranslatef(fanX[f], 400, 0);
		glRotatef(animTime * (f == 0 ? 170 : -170), 0, 0, 1);
		glColor3f(0.33f, 0.36f, 0.47f);
		for (int b = 0; b < 4; b++) {
			glRotatef(90, 0, 0, 1);
			drawTriangle(0, 0, 31, 0, 26, 16);
		}
		glPopMatrix();
		glColor3f(0.55f, 0.58f, 0.7f); drawCircle(fanX[f], 400, 6);
	}
}

// ---- Upper boundary: the door rail -----------------------------------------
// primitives: QUADS, TRIANGLES, LINES, POINTS, POLYGON
void drawRail() {
	glColor3f(0.08f, 0.08f, 0.12f);
	drawRect(0, 636, 1000, HUD_Y);              // ceiling
	glColor3f(0.36f, 0.39f, 0.48f);
	for (int k = 0; k < 9; k++) {               // brackets holding the rail
		float x = 60 + k * 110;
		drawTriangle(x - 12, HUD_Y, x + 12, HUD_Y, x, 636);
	}
	glBegin(GL_QUADS);                          // steel beam
	glColor3f(0.42f, 0.45f, 0.53f); glVertex2f(0, RAIL_Y); glVertex2f(1000, RAIL_Y);
	glColor3f(0.70f, 0.73f, 0.80f); glVertex2f(1000, 636); glVertex2f(0, 636);
	glEnd();
	glColor3f(0.18f, 0.19f, 0.25f);
	drawRect(0, 625, 1000, 629);                // track groove
	glColor3f(0.15f, 0.16f, 0.2f);
	glBegin(GL_LINES);                          // edges
	glVertex2f(0, RAIL_Y); glVertex2f(1000, RAIL_Y);
	glVertex2f(0, 636); glVertex2f(1000, 636);
	glEnd();
	glPointSize(3.5f);
	glColor3f(0.25f, 0.27f, 0.33f);
	glBegin(GL_POINTS);                         // rivets
	for (int x = 12; x < 1000; x += 24) { glVertex2f(x, 632.5f); glVertex2f(x, 621.5f); }
	glEnd();
	glPointSize(1);
	glColor3f(0.85f, 0.65f, 0.2f);              // hexagon bolts at both ends
	for (int e = 0; e < 2; e++) {
		glBegin(GL_POLYGON);
		for (int i = 0; i < 6; i++) glVertex2f((e ? 988 : 12) + 11 * cosf(i * PI / 3), 627 + 11 * sinf(i * PI / 3));
		glEnd();
	}
}

// ---- Lower boundary: the Scare Floor ---------------------------------------
// primitives: QUADS, LINES, POINTS, POLYGON, TRIANGLES
void drawFloor() {
	glBegin(GL_QUADS);                          // floor slab
	glColor3f(0.20f, 0.21f, 0.26f); glVertex2f(0, 0); glVertex2f(1000, 0);
	glColor3f(0.33f, 0.35f, 0.42f); glVertex2f(1000, 76); glVertex2f(0, 76);
	glEnd();
	glColor3f(0.15f, 0.16f, 0.2f);
	glLineWidth(2);
	glBegin(GL_LINES);                          // tile seams
	for (int x = 0; x <= 1000; x += 100) { glVertex2f(x, 0); glVertex2f(x, 76); }
	glVertex2f(0, 38); glVertex2f(1000, 38);
	glEnd();
	glLineWidth(1);
	glPointSize(4);
	glColor3f(0.55f, 0.58f, 0.66f);
	glBegin(GL_POINTS);                         // bolts
	for (int x = 0; x < 1000; x += 100) { glVertex2f(x + 6, 44); glVertex2f(x + 94, 44); glVertex2f(x + 6, 6); glVertex2f(x + 94, 6); }
	glEnd();
	glPointSize(1);
	glColor3f(0.07f, 0.07f, 0.09f);
	drawRect(0, 76, 1000, FLOOR_Y);             // hazard strip
	glColor3f(1, 0.8f, 0.1f);
	glBegin(GL_QUADS);                          // yellow slanted stripes
	for (int x = -20; x < 1020; x += 36) {
		glVertex2f(x, 76); glVertex2f(x + 18, 76); glVertex2f(x + 30, FLOOR_Y); glVertex2f(x + 12, FLOOR_Y);
	}
	glEnd();
	for (int v = 0; v < 3; v++) {               // vent grates
		float x = 200 + v * 300;
		glColor3f(0.10f, 0.10f, 0.13f);
		glBegin(GL_POLYGON);
		for (int i = 0; i < 8; i++) glVertex2f(x + 22 * cosf(i * PI / 4), 38 + 14 * sinf(i * PI / 4));
		glEnd();
		glColor3f(0.40f, 0.42f, 0.50f);
		glBegin(GL_LINES);
		for (int k = -2; k <= 2; k++) { glVertex2f(x + k * 7, 28); glVertex2f(x + k * 7, 48); }
		glEnd();
	}
	for (int s = 0; s < 2; s++) {               // warning signs
		float x = s ? 940 : 60;
		glColor3f(1, 0.8f, 0.1f);
		drawTriangle(x - 16, 22, x + 16, 22, x, 52);
		glColor3f(0.1f, 0.1f, 0.1f);
		glLineWidth(3);
		glBegin(GL_LINES); glVertex2f(x, 44); glVertex2f(x, 33); glEnd();
		glLineWidth(1);
		drawCircle(x, 27, 2);
	}
}

// ---- Closet door -----------------------------------------------------------
// primitives: LINES, POLYGON, QUADS, LINE_LOOP, POINTS
float doorColors[4][3] = { {0.96f,0.47f,0.66f}, {0.30f,0.58f,0.98f}, {0.30f,0.82f,0.56f}, {0.99f,0.70f,0.22f} };

void drawDoor(int i) {
	Door& d = doors[i];
	float shake = (d.state == DOOR_WARNING) ? sinf(animTime * 70) * 2.5f : 0;   // warning shake
	float x = d.x + shake;
	float left = x - DOOR_W / 2, right = x + DOOR_W / 2, top = DOOR_TOP, bottom = DOOR_TOP - DOOR_H;

	glColor3f(0.55f, 0.58f, 0.66f);             // hangers
	glLineWidth(3);
	glBegin(GL_LINES);
	glVertex2f(x - 20, 627); glVertex2f(x - 20, top + 6);
	glVertex2f(x + 20, 627); glVertex2f(x + 20, top + 6);
	glEnd();
	glLineWidth(1);
	glColor3f(0.14f, 0.15f, 0.2f);              // wheels on the rail
	drawCircle(x - 20, 627, 6);
	drawCircle(x + 20, 627, 6);

	glColor3f(0.24f, 0.20f, 0.22f);
	drawRect(left - 6, bottom - 3, right + 6, top + 6);   // frame
	glColor3f(0.04f, 0.05f, 0.12f);
	drawRect(left, bottom, right, top);                   // the dark child's room

	if (d.open > 0) {                           // inside the room: stars, moon and a bed
		glColor3f(1, 1, 0.7f);
		glPointSize(2.5f);
		glBegin(GL_POINTS);
		glVertex2f(x - 18, top - 18); glVertex2f(x + 12, top - 30); glVertex2f(x - 4, top - 50); glVertex2f(x + 22, top - 12);
		glEnd();
		glPointSize(1);
		drawCircle(x + 16, top - 22, 6);
		glColor3f(0.04f, 0.05f, 0.12f);
		drawCircle(x + 19, top - 20, 5);
		glColor3f(0.10f, 0.12f, 0.28f);
		drawRect(left + 6, bottom, right - 6, bottom + 18);
	}

	// the door panel gets narrower (towards its hinge on the left) as it opens
	float pw = DOOR_W * (1 - 0.85f * d.open);
	float shade = 1 - 0.35f * d.open;
	glColor3f(doorColors[i][0] * shade, doorColors[i][1] * shade, doorColors[i][2] * shade);
	drawRect(left, bottom, left + pw, top);
	glColor3f(doorColors[i][0] * shade * 0.6f, doorColors[i][1] * shade * 0.6f, doorColors[i][2] * shade * 0.6f);
	glLineWidth(2);
	drawRectLine(left + pw * 0.15f, bottom + DOOR_H * 0.55f, left + pw * 0.85f, bottom + DOOR_H * 0.9f);
	drawRectLine(left + pw * 0.15f, bottom + DOOR_H * 0.1f, left + pw * 0.85f, bottom + DOOR_H * 0.45f);
	glLineWidth(1);
	glColor3f(1, 1, 0.6f);
	glPointSize(4);
	glBegin(GL_POINTS);                         // little decorations
	for (int k = 0; k < 5; k++) glVertex2f(left + pw * (0.3f + 0.1f * k), bottom + DOOR_H * (0.65f + 0.05f * ((k + i) % 4)));
	glEnd();
	glPointSize(1);
	glColor3f(0.95f, 0.80f, 0.30f);
	drawCircle(left + pw * 0.86f, bottom + DOOR_H * 0.48f, 3.5f);   // knob

	// lamp on top of the frame: blinks red while warning, green while open
	bool blinkOn = d.state == DOOR_WARNING && (int)(d.timer * 10) % 2 == 0;
	if (blinkOn) glColor3f(1, 0.25f, 0.2f);
	else if (d.state != DOOR_CLOSED) glColor3f(0.3f, 1, 0.45f);
	else glColor3f(0.4f, 0.32f, 0.3f);
	glBegin(GL_POLYGON);
	for (int k = 0; k <= 12; k++) glVertex2f(x + 7 * cosf(PI * k / 12), top + 6 + 7 * sinf(PI * k / 12));
	glEnd();
	if (blinkOn) {                              // red warning bars beside the door
		glLineWidth(3);
		glBegin(GL_LINES);
		for (int k = 0; k < 3; k++) {
			float o = 12 + k * 6;
			glVertex2f(left - o, bottom + 25); glVertex2f(left - o, top - 25);
			glVertex2f(right + o, bottom + 25); glVertex2f(right + o, top - 25);
		}
		glEnd();
		glLineWidth(1);
	}

	if (i == hoverDoor && gameState == STATE_PLAYING && !paused) {   // Door Station highlight
		if (stationTimer <= 0) glColor3f(1, 0.85f, 0.2f); else glColor3f(0.9f, 0.35f, 0.3f);
		glLineWidth(4);
		drawRectLine(left - 10, bottom - 7, right + 10, top + 10);
		glLineWidth(1);
	}
}

// ---- Player monster: green, round, one big eye (faces right) --------------
// primitives: POLYGON, TRIANGLES, QUADS, LINE_STRIP, LINES, LINE_LOOP, POINTS
void drawPlayerMonster(float walk, bool walking, bool shocked, bool suit, bool fast) {
	float r = 0.56f, g = 0.80f, b = 0.20f;                 // normal green
	if (suit) { r = 1.0f; g = 0.80f; b = 0.16f; }          // yellow hazmat colour
	else if (fast) { r = 0.25f; g = 0.85f; b = 0.98f; }    // electric blue
	float swing = walking ? sinf(walk) : 0;

	if (fast) {                                            // speed streaks behind
		glColor4f(0.4f, 1, 1, 0.7f);
		glLineWidth(3);
		glBegin(GL_LINES);
		for (int k = 0; k < 4; k++) {
			float o = (float)((int)(animTime * 300 + k * 37) % 30);
			glVertex2f(-36 - o, 30 + k * 14); glVertex2f(-60 - o, 30 + k * 14);
		}
		glEnd();
		glLineWidth(1);
	}

	for (int s = -1; s <= 1; s += 2) {                     // legs, feet and toe claws
		float hip = s * 10.0f, foot = hip + s * swing * 7;
		glColor3f(r * 0.7f, g * 0.7f, b * 0.7f);
		glBegin(GL_QUADS);
		glVertex2f(hip - 3.5f, 28); glVertex2f(hip + 3.5f, 28); glVertex2f(foot + 3.5f, 4); glVertex2f(foot - 3.5f, 4);
		glEnd();
		glColor3f(r, g, b);
		drawEllipse(foot + 3, 4, 9, 4.5f);
		glColor3f(0.8f, 0.78f, 0.7f);
		drawTriangle(foot + 11, 1, foot + 11, 3, foot + 14, 2);
		drawTriangle(foot + 11, 4, foot + 11, 6, foot + 14, 5);
	}

	glColor3f(0.88f, 0.86f, 0.78f);                        // horns
	drawTriangle(-18, 74, -9, 78, -16, 92);
	drawTriangle(9, 78, 18, 74, 16, 92);

	glColor3f(r * 0.7f, g * 0.7f, b * 0.7f); drawCircle(0, 50, 31);      // body (shadow + body)
	glColor3f(r, g, b); drawCircle(1.5f, 51.5f, 29);
	glColor4f(1, 1, 1, 0.18f); drawEllipse(-12, 65, 8, 5);              // shine

	glColor3f(1, 1, 1); drawCircle(3, 56, 16);                          // eye
	glColor3f(r * 0.5f, g * 0.5f, b * 0.5f); glLineWidth(1.6f); drawCircleLine(3, 56, 16); glLineWidth(1);
	float iris = shocked ? 7.0f : 9.0f;                                 // looks forward
	glColor3f(0.15f, 0.62f, 0.52f); drawCircle(8, 56, iris);
	glColor3f(0.40f, 0.85f, 0.70f); drawCircle(8, 56, iris * 0.62f);
	glColor3f(0, 0, 0); drawCircle(9.5f, 56, shocked ? 2.8f : 4.3f);
	glColor3f(1, 1, 1);
	glPointSize(3.5f);
	glBegin(GL_POINTS); glVertex2f(12, 60); glEnd();
	glPointSize(1);

	glColor3f(0.30f, 0.06f, 0.12f);                                     // mouth
	if (shocked) {
		drawEllipse(4, 34, 8, 7.5f);
		glColor3f(1, 1, 1);
		drawTriangle(-1, 40, 3, 40, 1, 36);
		drawTriangle(5, 40, 9, 40, 7, 36);
	} else {
		glBegin(GL_POLYGON);                                            // half-ellipse smile
		for (int k = 0; k <= 16; k++) glVertex2f(4 + 13 * cosf(PI + PI * k / 16), 38 + 8 * sinf(PI + PI * k / 16));
		glEnd();
		glColor3f(0.95f, 0.45f, 0.55f); drawEllipse(6, 32.5f, 6, 2);    // tongue
		glColor3f(1, 1, 1);
		drawTriangle(-4, 38, 1, 38, -1.5f, 34);
		drawTriangle(7, 38, 12, 38, 9.5f, 34);
	}

	for (int s = -1; s <= 1; s += 2) {                                  // arms and fingers
		float handX = s * 42 + (walking ? s * swing * 6 : 0), handY = 26;
		glColor3f(r * 0.7f, g * 0.7f, b * 0.7f);
		glLineWidth(4.5f);
		glBegin(GL_LINE_STRIP);
		glVertex2f(s * 26.0f, 46); glVertex2f(s * 36.0f, 36); glVertex2f(handX, handY);
		glEnd();
		glLineWidth(2.5f);
		glBegin(GL_LINES);
		for (int f = -1; f <= 1; f++) { glVertex2f(handX, handY); glVertex2f(handX + s * 3 + f * 3, handY - 6); }
		glEnd();
		glLineWidth(1);
	}

	if (suit) {                                                         // hazmat bubble helmet
		glColor4f(0.85f, 0.95f, 1, 0.14f); drawEllipse(0, 52, 45, 50);
		glColor4f(0.75f, 0.92f, 1, 0.85f);
		glLineWidth(2.5f);
		glBegin(GL_LINE_LOOP);
		for (int k = 0; k < 30; k++) glVertex2f(45 * cosf(2 * PI * k / 30), 52 + 50 * sinf(2 * PI * k / 30));
		glEnd();
		glLineWidth(1);
	}
}

// ---- Rival monster: huge teal furry monster with purple spots (faces right) --
// primitives: POLYGON, TRIANGLES, QUADS, LINES, LINE_STRIP, POINTS
// Uses setColor() so the whole body can fade into the wall (camouflage).
void drawRivalMonster(float walk) {
	float swing = sinf(walk);

	setColor(0.16f, 0.52f, 0.62f);                         // tail
	drawTriangle(-30, 52, -34, 30, -72, 20 + swing * 4);

	for (int l = 0; l < 2; l++) {                          // legs, feet and claws
		float x = l == 0 ? -16.0f : 16.0f;
		float lift = sinf(walk + l * PI) > 0 ? sinf(walk + l * PI) * 7 : 0;
		float step = cosf(walk + l * PI) * 5;
		if (l == 0) setColor(0.16f, 0.52f, 0.62f); else setColor(0.25f, 0.72f, 0.80f);
		glBegin(GL_QUADS);
		glVertex2f(x - 10, 36); glVertex2f(x + 10, 36); glVertex2f(x + 10 + step, 6 + lift); glVertex2f(x - 10 + step, 6 + lift);
		glEnd();
		drawEllipse(x + 4 + step, 6 + lift, 15, 6.5f);
		setColor(0.96f, 0.93f, 0.82f);
		for (int c = 0; c < 3; c++) {
			float y = 2 + lift + c * 4;
			drawTriangle(x + 17 + step, y, x + 17 + step, y + 3.5f, x + 23 + step, y + 1.7f);
		}
	}

	setColor(0.25f, 0.72f, 0.80f);
	glBegin(GL_TRIANGLES);                                 // fur tufts around the back
	for (int k = 0; k < 14; k++) {
		float a = (100 + k * 15) * PI / 180;
		glVertex2f(43 * cosf(a), 62 + 41 * sinf(a));
		glVertex2f(43 * cosf(a + 0.13f), 62 + 41 * sinf(a + 0.13f));
		glVertex2f(50 * cosf(a + 0.065f), 62 + 48 * sinf(a + 0.065f));
	}
	glEnd();
	drawEllipse(0, 62, 43, 41);                            // body

	setColor(0.50f, 0.30f, 0.64f);                         // purple spots
	float spots[8][3] = { {-24,74,8},{-8,88,5},{-30,50,6},{-6,50,9},{14,64,6},{-14,32,6},{22,42,5},{-34,66,4} };
	for (int k = 0; k < 8; k++) drawCircle(spots[k][0], spots[k][1], spots[k][2]);

	setColor(0.86f, 0.78f, 0.62f);                         // horns
	drawTriangle(2, 110, 11, 113, -4, 128);
	drawTriangle(17, 114, 26, 113, 14, 130);

	setColor(0.25f, 0.72f, 0.80f); drawEllipse(16, 96, 26, 21);         // head
	setColor(0.50f, 0.30f, 0.64f); drawCircle(0, 106, 4.5f);
	setColor(0.55f, 0.88f, 0.93f); drawEllipse(32, 89, 13, 9);          // muzzle
	setColor(0.22f, 0.10f, 0.22f); drawEllipse(41, 94, 5.5f, 4);        // nose
	setColor(1, 1, 1); drawEllipse(23, 102, 4.5f, 5); drawEllipse(34, 103, 4.5f, 5);   // eyes
	setColor(0.05f, 0.05f, 0.1f); drawEllipse(24.5f, 102, 2.5f, 3); drawEllipse(35.5f, 103, 2.5f, 3);
	setColor(1, 1, 1);
	glPointSize(2);
	glBegin(GL_POINTS); glVertex2f(25.5f, 104); glVertex2f(36.5f, 105); glEnd();
	glPointSize(1);
	setColor(0.28f, 0.12f, 0.34f);
	glLineWidth(2.5f);
	glBegin(GL_LINES);                                     // eyebrows
	glVertex2f(18, 108.5f); glVertex2f(27, 110); glVertex2f(30, 110.5f); glVertex2f(38, 109);
	glEnd();
	glLineWidth(2);
	glBegin(GL_LINE_STRIP);                                // smile
	glVertex2f(25, 84); glVertex2f(30, 82); glVertex2f(36, 82.5f); glVertex2f(41, 86);
	glEnd();
	glLineWidth(1);

	float armSwing = -swing * 6;                           // front arm, hand and claws
	setColor(0.21f, 0.64f, 0.73f);
	drawEllipse(10, 76, 10, 9);
	glBegin(GL_QUADS);
	glVertex2f(1, 76); glVertex2f(19, 76); glVertex2f(21 + armSwing, 46); glVertex2f(5 + armSwing, 46);
	glEnd();
	drawEllipse(13 + armSwing, 44, 10, 9);
	setColor(0.96f, 0.93f, 0.82f);
	for (int c = 0; c < 3; c++) {
		float x = 7 + armSwing + c * 5;
		drawTriangle(x, 37, x + 3.5f, 37, x + 1.7f, 31);
	}
}

// ---- Scream canister (blue) and laugh canister (gold with rays) -----------
// primitives: QUADS, LINE_STRIP, POINTS, LINES, LINE_LOOP, TRIANGLES (+ POLYGON glow)
void drawCanister(bool laugh, float phase) {
	float lr = 0.2f, lg = 0.55f, lb = 1.0f;                // liquid colour
	float cr = 0.62f, cg = 0.66f, cb = 0.74f;              // cap colour
	if (laugh) {
		lr = 1.0f; lg = 0.85f; lb = 0.2f; cr = 0.95f; cg = 0.72f; cb = 0.22f;
		glColor4f(1, 0.9f, 0.3f, 0.2f); drawCircle(0, 0, 30);           // glow
		glColor3f(1, 0.9f, 0.3f);
		glLineWidth(2.5f);
		glBegin(GL_LINES);                                              // rotating rays
		for (int k = 0; k < 8; k++) {
			float a = phase * 2.5f + k * PI / 4;
			glVertex2f(25 * cosf(a), 25 * sinf(a)); glVertex2f(33 * cosf(a), 33 * sinf(a));
		}
		glEnd();
		glLineWidth(1);
	}
	glColor4f(1, 1, 1, 0.22f); drawRect(-10, -14, 10, 14);             // glass
	float level = 5 + sinf(phase * 4) * 1.5f;                          // sloshing liquid
	glColor3f(lr, lg, lb); drawRect(-10, -14, 10, level);
	glColor3f((lr + 1) / 2, (lg + 1) / 2, (lb + 1) / 2);
	glLineWidth(2);
	glBegin(GL_LINE_STRIP);                                             // wavy surface
	for (int k = 0; k <= 8; k++) glVertex2f(-10 + k * 2.5f, level + sinf(phase * 9 + k) * 1.2f);
	glEnd();
	glColor3f(1, 1, 1);
	glPointSize(3);
	glBegin(GL_POINTS);                                                 // rising bubbles
	for (int k = 0; k < 3; k++) glVertex2f(-5.0f + k * 5, -13 + fmodf(phase * 16 + k * 7, level + 13));
	glEnd();
	glPointSize(1);
	glBegin(GL_LINES); glVertex2f(-6, -11); glVertex2f(-6, 11); glEnd();   // glass glare
	glColor3f(0.12f, 0.14f, 0.2f);
	glLineWidth(1.5f); drawRectLine(-10, -14, 10, 14); glLineWidth(1);  // glass outline
	glColor3f(cr, cg, cb);
	drawRect(-12, 14, 12, 20);                                          // caps
	drawRect(-12, -20, 12, -14);
	glColor3f(cr * 0.6f, cg * 0.6f, cb * 0.6f);
	drawTriangle(-4, 20, 4, 20, 0, 24);                                 // valve
}

// ---- Toxic items -----------------------------------------------------------
// sock: POLYGON, QUADS, LINE_STRIP      duck: POLYGON, TRIANGLES, POINTS, LINE_STRIP
void drawSock(float phase) {
	glColor4f(0.4f, 1, 0.3f, 0.16f); drawCircle(0, 0, 22);             // toxic glow
	glColor3f(0.95f, 0.95f, 0.9f);
	drawRect(-7, -6, 7, 16);
	drawEllipse(2, -8, 13, 7);
	glColor3f(0.9f, 0.2f, 0.25f);                                       // stripes
	drawRect(-7, 8, 7, 10.5f);
	drawRect(-7, 12.5f, 7, 15);
	glColor3f(0.45f, 1, 0.35f);
	glLineWidth(2);
	for (int k = -1; k <= 1; k++) {                                     // stink lines
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i <= 6; i++) glVertex2f(k * 6 + sinf(phase * 8 + i) * 2, 19 + i * 2.2f);
		glEnd();
	}
	glLineWidth(1);
}

void drawDuck(float phase) {
	glColor4f(0.4f, 1, 0.3f, 0.16f); drawCircle(0, 0, 22);             // toxic glow
	glColor3f(0.75f, 0.85f, 0.15f);
	drawEllipse(-2, -4, 15, 10);                                        // body
	drawCircle(7, 7, 8);                                                // head
	glColor3f(1, 0.5f, 0.1f);
	drawTriangle(13, 9, 21, 6.5f, 13, 4);                               // beak
	glColor3f(0, 0, 0);
	glPointSize(3.5f);
	glBegin(GL_POINTS); glVertex2f(9, 9); glEnd();                      // eye
	glColor3f(0.3f, 1, 0.2f);
	glPointSize(4);
	glBegin(GL_POINTS); glVertex2f(-6, -14 - fmodf(phase * 10, 6)); glVertex2f(3, -14 - fmodf(phase * 10 + 3, 6)); glEnd();  // drips
	glPointSize(1);
	glColor3f(0.45f, 0.6f, 0.05f);
	glLineWidth(2);
	glBegin(GL_LINE_STRIP); glVertex2f(-10, -2); glVertex2f(-4, -7); glVertex2f(4, -4); glEnd();   // wing
	glLineWidth(1);
}

// ---- Power-ups -------------------------------------------------------------
// suit:  POLYGON, LINES, QUADS, LINE_LOOP, TRIANGLES, POINTS
// speed: POLYGON, LINES, QUADS, TRIANGLES, POINTS
void drawDashedRing(float rotation) {                   // GL_LINES
	glLineWidth(2.5f);
	glBegin(GL_LINES);
	for (int k = 0; k < 14; k++) {
		float a = rotation + k * 2 * PI / 14;
		glVertex2f(22 * cosf(a), 22 * sinf(a));
		glVertex2f(22 * cosf(a + PI / 14), 22 * sinf(a + PI / 14));
	}
	glEnd();
	glLineWidth(1);
}

void drawSuitPowerup(float phase) {
	glColor4f(1, 0.7f, 0.1f, 0.2f); drawCircle(0, 0, 22);
	glColor3f(1, 0.72f, 0.15f); drawDashedRing(phase * 2);
	glColor3f(1, 0.82f, 0.18f);
	drawRect(-8, -14, 8, 2);                            // suit body
	drawRect(-13, -10, -8, 0);                          // arms
	drawRect(8, -10, 13, 0);
	glColor4f(0.75f, 0.92f, 1, 0.9f); drawCircle(0, 8, 8.5f);           // helmet
	glColor3f(0.3f, 0.5f, 0.7f); drawCircleLine(0, 8, 8.5f);
	glColor3f(0.9f, 0.2f, 0.2f); drawTriangle(-4, -2, 4, -2, 0, -9);    // badge
	glColor3f(1, 1, 1);
	glPointSize(3);
	glBegin(GL_POINTS); glVertex2f(-3, 11); glVertex2f(16 * cosf(phase * 3), 16 * sinf(phase * 3)); glEnd();
	glPointSize(1);
}

void drawSpeedPowerup(float phase) {
	glColor4f(0.2f, 1, 0.6f, 0.2f); drawCircle(0, 0, 22);
	glColor3f(0.3f, 0.95f, 0.6f); drawDashedRing(-phase * 2);
	glColor3f(0.2f, 0.8f, 0.5f);
	drawRect(-11, -10, 4, -1);                          // sneaker
	drawRect(-11, -1, -4, 9);
	drawEllipse(5, -6, 7, 4.5f);
	glColor3f(0.1f, 0.25f, 0.2f); drawRect(-12, -13, 12, -10);          // sole
	glColor3f(1, 0.9f, 0.2f);
	drawTriangle(2, 16, 10, 16, 3, 6);                  // lightning bolt
	drawTriangle(1, 9, 8, 9, -1, -3);
	glColor3f(0.6f, 1, 0.8f);
	glLineWidth(2);
	glBegin(GL_LINES);                                  // speed lines
	for (int k = 0; k < 3; k++) { glVertex2f(-19, -8 + k * 6.0f); glVertex2f(-14, -8 + k * 6.0f); }
	glEnd();
	glLineWidth(1);
	glColor3f(1, 1, 1);
	glPointSize(3);
	glBegin(GL_POINTS); glVertex2f(-16 * cosf(phase * 3), 16 * sinf(phase * 3)); glEnd();
	glPointSize(1);
}

// draws an item with its falling animation
void drawItem(Item& it) {
	glPushMatrix();
	glTranslatef(it.x, it.y, 0);
	if (it.type == ITEM_CANISTER || it.type == ITEM_LAUGH) {
		glRotatef(sinf(it.phase * 5) * 14, 0, 0, 1);   // collectables swing like a pendulum
		drawCanister(it.type == ITEM_LAUGH, it.phase);
	} else if (it.type == ITEM_SOCK || it.type == ITEM_DUCK) {
		glRotatef(it.angle, 0, 0, 1);                   // obstacles spin
		if (it.type == ITEM_SOCK) drawSock(it.phase); else drawDuck(it.phase);
	} else {
		float s = 1 + 0.12f * sinf(it.phase * 7);       // power-ups pulse in size
		glScalef(s, s, 1);
		if (it.type == ITEM_SUIT) drawSuitPowerup(it.phase); else drawSpeedPowerup(it.phase);
	}
	glPopMatrix();
}


// ============================================================================
//  HUD: health, score, scream meter, Door Station status and time
// ============================================================================
void drawHUD() {
	glBegin(GL_QUADS);
	glColor3f(0.05f, 0.05f, 0.09f); glVertex2f(0, HUD_Y); glVertex2f(1000, HUD_Y);
	glColor3f(0.13f, 0.13f, 0.21f); glVertex2f(1000, 700); glVertex2f(0, 700);
	glEnd();
	glColor3f(1, 0.75f, 0.1f);
	glLineWidth(2);
	glBegin(GL_LINES); glVertex2f(0, HUD_Y); glVertex2f(1000, HUD_Y); glEnd();
	glLineWidth(1);

	// health: 5 little eyeballs (POLYGON + POINTS); a lost life is a crossed-out eye (LINES)
	for (int i = 0; i < 5; i++) {
		float x = 26 + i * 30.0f, y = 676;
		if (i < lives) {
			glColor3f(0.56f, 0.80f, 0.20f); drawCircle(x, y, 12);
			glColor3f(1, 1, 1); drawCircle(x, y, 8.5f);
			glColor3f(0.15f, 0.62f, 0.52f); drawCircle(x + 1.5f, y, 4.5f);
			glColor3f(0, 0, 0);
			glPointSize(3.5f);
			glBegin(GL_POINTS); glVertex2f(x + 2, y); glEnd();
			glPointSize(1);
		} else {
			glColor3f(0.25f, 0.27f, 0.33f); drawCircle(x, y, 12);
			glColor3f(0.55f, 0.15f, 0.2f);
			glLineWidth(2.5f);
			glBegin(GL_LINES);
			glVertex2f(x - 6, y - 6); glVertex2f(x + 6, y + 6);
			glVertex2f(x - 6, y + 6); glVertex2f(x + 6, y - 6);
			glEnd();
			glLineWidth(1);
		}
	}

	char text[32];
	sprintf(text, "SCORE %d", score);
	drawText(text, 182, 668, 17, 1, 0.75f, 0.2f);

	// scream meter (QUADS + LINES + LINE_LOOP) with a small canister icon
	glPushMatrix();
	glTranslatef(343, 676.5f, 0);
	glScalef(0.5f, 0.5f, 1);
	drawCanister(false, animTime);
	glPopMatrix();
	float fill = (float)score / QUOTA;
	if (fill > 1) fill = 1;
	glColor3f(0.08f, 0.09f, 0.14f); drawRect(360, 667, 600, 686);
	glBegin(GL_QUADS);
	glColor3f(1, 0.55f, 0.05f); glVertex2f(360, 667); glVertex2f(360 + 240 * fill, 667);
	glColor3f(1, 0.92f, 0.35f); glVertex2f(360 + 240 * fill, 686); glVertex2f(360, 686);
	glEnd();
	glColor3f(0, 0, 0);
	glBegin(GL_LINES);                                     // ticks every 10%
	for (int k = 1; k < 10; k++) { glVertex2f(360 + 24 * k, 667); glVertex2f(360 + 24 * k, 672); }
	glEnd();
	glColor3f(0.85f, 0.87f, 0.95f);
	glLineWidth(2); drawRectLine(360, 667, 600, 686); glLineWidth(1);

	// Door Station: mini door (QUADS) + recharge pie (TRIANGLES) + rim (LINE_LOOP)
	bool ready = stationTimer <= 0;
	glColor3f(0.24f, 0.2f, 0.22f); drawRect(641, 663, 659, 689);
	if (ready) glColor3f(0.3f, 0.85f, 0.55f); else glColor3f(0.45f, 0.45f, 0.5f);
	drawRect(643.5f, 665, 656.5f, 687);
	glColor3f(0.12f, 0.12f, 0.18f); drawCircle(682, 676, 11);
	if (ready) glColor3f(0.3f, 1, 0.5f); else glColor3f(1, 0.45f, 0.25f);
	float charged = ready ? 1 : 1 - stationTimer / STATION_COOLDOWN;
	glBegin(GL_TRIANGLES);                                 // pie slice that grows while recharging
	for (int k = 0; k < (int)(charged * 32); k++) {
		float a1 = PI / 2 - 2 * PI * k / 32, a2 = PI / 2 - 2 * PI * (k + 1) / 32;
		glVertex2f(682, 676);
		glVertex2f(682 + 11 * cosf(a1), 676 + 11 * sinf(a1));
		glVertex2f(682 + 11 * cosf(a2), 676 + 11 * sinf(a2));
	}
	glEnd();
	glColor3f(0.85f, 0.87f, 0.95f);
	glLineWidth(1.5f); drawCircleLine(682, 676, 11); glLineWidth(1);
	if (ready) drawText("STATION READY", 702, 671, 11, 0.5f, 1, 0.5f);
	else drawText("RECHARGING", 702, 671, 11, 1, 0.4f, 0.3f);

	// time left
	int secondsLeft = (int)ceilf(SHIFT_TIME - gameTime);
	if (secondsLeft < 0) secondsLeft = 0;
	sprintf(text, "%02d:%02d", secondsLeft / 60, secondsLeft % 60);
	if (secondsLeft <= 10) drawText(text, 985, 666, 19, 1, 0.35f, 0.3f, 2);
	else drawText(text, 985, 666, 19, 0.8f, 0.9f, 1, 2);
}


// ============================================================================
//  SCREENS
// ============================================================================
void drawWorld(bool withPlayers) {
	drawBackground();
	drawRail();
	for (int i = 0; i < 4; i++) drawDoor(i);
	drawFloor();
	if (!withPlayers) return;

	camoAmount = camo;                                     // rival (flipped to face its direction)
	glPushMatrix();
	glTranslatef(rivalX, FLOOR_Y, 0);
	glScalef(rivalDir, 1, 1);
	drawRivalMonster(rivalWalk);
	glPopMatrix();
	camoAmount = 0;

	for (int i = 0; i < 30; i++) if (items[i].active) drawItem(items[i]);

	float shiver = frozenTime > 0 ? sinf(animTime * 80) * 2 : 0;   // player (flipped to face its direction)
	glPushMatrix();
	glTranslatef(playerX + shiver, playerY, 0);
	glScalef(playerDir, 1, 1);
	drawPlayerMonster(playerWalk, playerMoving && onGround, frozenTime > 0, suitTime > 0, speedTime > 0);
	glPopMatrix();

	for (int i = 0; i < 10; i++)
		if (popups[i].active) drawText(popups[i].text, popups[i].x, popups[i].y, 15, popups[i].r, popups[i].g, popups[i].b, 1);
}

void drawDarkOverlay(float alpha) {
	glColor4f(0.02f, 0.02f, 0.06f, alpha);
	drawRect(0, 0, 1000, 700);
}

void drawStartScreen() {
	drawWorld(false);
	drawDarkOverlay(0.62f);

	glPushMatrix();                                        // the two monsters
	glTranslatef(130, FLOOR_Y, 0); glScalef(1.5f, 1.5f, 1);
	drawPlayerMonster(0, false, false, false, false);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(875, FLOOR_Y, 0); glScalef(-1.35f, 1.35f, 1);
	drawRivalMonster(0);
	glPopMatrix();

	drawText("SCREAM SHIFT", 500, 520 + sinf(animTime * 3) * 5, 76, 0.55f, 0.9f, 0.25f, 1);
	drawText("A MONSTERS ON THE SCARE FLOOR ADVENTURE", 500, 478, 15, 0.4f, 0.85f, 0.95f, 1);

	glColor4f(0.08f, 0.09f, 0.17f, 0.92f); drawRect(290, 230, 710, 455);
	glColor3f(1, 0.75f, 0.1f);
	glLineWidth(2.5f); drawRectLine(290, 230, 710, 455); glLineWidth(1);
	drawText("HOW TO WORK YOUR SHIFT", 500, 424, 18, 1, 0.75f, 0.2f, 1);
	const char* keys[4] = { "A / D", "W / SPACE", "CLICK A DOOR", "P  /  ESC" };
	const char* actions[4] = { "MOVE", "JUMP", "DOOR STATION", "PAUSE  /  QUIT" };
	for (int i = 0; i < 4; i++) {
		drawText(keys[i], 330, 385 - i * 33.0f, 15, 1, 1, 1);
		drawText(actions[i], 530, 385 - i * 33.0f, 15, 0.4f, 0.85f, 0.95f);
	}
	drawText("CATCH CANISTERS - DODGE TOXIC ITEMS", 500, 248, 13, 0.4f, 0.85f, 0.95f, 1);

	if ((int)(animTime * 2) % 2 == 0)                      // blinking prompt
		drawText("PRESS ANY KEY OR CLICK TO CLOCK IN", 500, 130, 20, 1, 0.75f, 0.2f, 1);
}

void drawEndScreen() {
	float colors[3][3] = { {0.10f,0.42f,0.24f}, {0.45f,0.08f,0.12f}, {0.10f,0.15f,0.30f} };   // green, red, navy
	float* c = colors[endType];
	glBegin(GL_QUADS);
	glColor3f(c[0] * 0.55f, c[1] * 0.55f, c[2] * 0.55f); glVertex2f(0, 0); glVertex2f(1000, 0);
	glColor3f(c[0] * 1.3f, c[1] * 1.3f, c[2] * 1.3f); glVertex2f(1000, 700); glVertex2f(0, 700);
	glEnd();

	if (endType == END_QUOTA) drawText("QUOTA REACHED!", 500, 520, 70, 0.55f, 0.95f, 0.3f, 1);
	if (endType == END_LOSE)  drawText("GAME LOSE", 500, 520, 70, 1, 0.3f, 0.3f, 1);
	if (endType == END_SHIFT) drawText("SHIFT OVER", 500, 520, 70, 0.4f, 0.65f, 1, 1);

	char text[40];
	sprintf(text, "SCORE  %d", score);
	drawText(text, 500, 440, 36, 1, 0.75f, 0.2f, 1);

	glColor4f(0, 0, 0, 0.3f); drawRect(250, 300, 750, 400);
	glColor4f(1, 1, 1, 0.5f);
	glLineWidth(2); drawRectLine(250, 300, 750, 400); glLineWidth(1);
	sprintf(text, "%d", caught); drawText(text, 335, 352, 30, 0.4f, 0.7f, 1, 1);
	sprintf(text, "%d", missed); drawText(text, 500, 352, 30, 1, 1, 1, 1);
	sprintf(text, "%d", stolen); drawText(text, 665, 352, 30, 0.75f, 0.55f, 1, 1);
	drawText("CAUGHT", 335, 318, 12, 1, 1, 1, 1);
	drawText("MISSED", 500, 318, 12, 1, 1, 1, 1);
	drawText("STOLEN", 665, 318, 12, 1, 1, 1, 1);

	glPushMatrix();
	glTranslatef(500, 110, 0); glScalef(1.5f, 1.5f, 1);
	drawPlayerMonster(0, false, endType != END_QUOTA, false, false);
	glPopMatrix();
	glPushMatrix();
	glTranslatef(820, 110, 0); glScalef(-1.2f, 1.2f, 1);
	drawRivalMonster(0);
	glPopMatrix();

	glColor4f(0, 0, 0, 0.25f); drawRect(0, 0, 1000, 110);
	drawText("PRESS R FOR A NEW SHIFT   -   ESC TO QUIT", 500, 45, 18, 1, 1, 1, 1);
}

void Display() {
	glClear(GL_COLOR_BUFFER_BIT);
	if (gameState == STATE_START) drawStartScreen();
	else if (gameState == STATE_END) drawEndScreen();
	else {
		drawWorld(true);
		drawHUD();
		if (alarmTime > 0) {                                // 2319 alarm: flash red and yellow
			if ((int)(alarmTime * 10) % 2 == 0) glColor4f(1, 0.1f, 0.1f, 0.35f);
			else glColor4f(1, 0.9f, 0.1f, 0.3f);
			drawRect(0, 0, 1000, 700);
			drawText("2319!", 500 + sinf(animTime * 60) * 4, 370, 64, 1, 0.3f, 0.3f, 1);
			drawText("DECONTAMINATING...", 500, 330, 18, 1, 0.75f, 0.2f, 1);
		}
		if (paused) {
			drawDarkOverlay(0.6f);
			drawText("PAUSED", 500, 360, 72, 1, 0.75f, 0.2f, 1);
			drawText("PRESS P TO GET BACK TO WORK", 500, 300, 18, 1, 1, 1, 1);
		}
	}
	glutSwapBuffers();
}


// ============================================================================
//  GAME LOGIC
// ============================================================================
float progress() { return gameTime < SHIFT_TIME ? gameTime / SHIFT_TIME : 1; }   // 0 -> 1 over the shift

// half width / half height of an item's collision box
float itemHalfW(int type) { return type <= ITEM_LAUGH ? 12.0f : 18.0f; }
float itemHalfH(int type) { return type <= ITEM_LAUGH ? 21.0f : 18.0f; }

bool boxesOverlap(float ax1, float ay1, float ax2, float ay2, float bx1, float by1, float bx2, float by2) {
	return ax2 > bx1 && ax1 < bx2 && ay2 > by1 && ay1 < by2;
}

void addPopup(const char* text, float x, float y, float r, float g, float b) {
	for (int i = 0; i < 10; i++)
		if (!popups[i].active) {
			Popup& p = popups[i];
			p.active = true; strcpy(p.text, text); p.x = x; p.y = y; p.life = 1; p.r = r; p.g = g; p.b = b;
			return;
		}
}

void resetGame() {
	playerX = 500; playerY = FLOOR_Y; playerVY = 0; pushSpeed = 0; playerWalk = 0;
	playerDir = 1; lives = 5; onGround = true; playerMoving = false;
	frozenTime = suitTime = speedTime = 0;
	rivalX = 860; rivalDir = -1; rivalWalk = 0; camo = 0; camoTimer = 0;
	for (int i = 0; i < 4; i++) {
		doors[i].x = 140 + i * 240.0f;
		doors[i].speed = (i % 2 ? -1 : 1) * (35 + rand() % 25);
		doors[i].state = DOOR_CLOSED; doors[i].timer = 0; doors[i].open = 0;
		doors[i].forced = false; doors[i].dropped = false;
	}
	for (int i = 0; i < 30; i++) items[i].active = false;
	for (int i = 0; i < 10; i++) popups[i].active = false;
	score = caught = missed = stolen = 0;
	stationTimer = alarmTime = 0; nextDropTime = 1; nextPowerup = 0;
	gameTime = 0; animTime = 0; paused = false;
}

void startGame() {
	resetGame();
	gameState = STATE_PLAYING;
	playOn(musicDevice, &musicHeader, musicSound, musicLength, true);
}

void endGame(int type) {
	gameState = STATE_END;
	endType = type;
	waveOutReset(musicDevice);
	if (type == END_QUOTA) playEffect(winSound, LENGTH(winSound));
	else playEffect(loseSound, LENGTH(loseSound));
}

int chooseItemType() {
	// scheduled power-ups: suit, speed, suit, speed
	if (nextPowerup < 4 && gameTime >= powerupTimes[nextPowerup]) {
		nextPowerup++;
		return nextPowerup % 2 == 1 ? ITEM_SUIT : ITEM_SPEED;
	}
	int r = rand() % 100;
	if (r < 7)  return ITEM_LAUGH;                          // rare
	if (r < 52) return ITEM_CANISTER;
	if (r < 90) return rand() % 2 ? ITEM_SOCK : ITEM_DUCK;
	return rand() % 2 ? ITEM_SUIT : ITEM_SPEED;
}

// true if no item is still close under the door (so dropped items don't overlap)
bool spaceUnderDoor(float x) {
	for (int i = 0; i < 30; i++)
		if (items[i].active && fabsf(items[i].x - x) < 52 && items[i].y > DOOR_TOP - DOOR_H - 100) return false;
	return true;
}

void dropItem(Door& d) {
	for (int i = 0; i < 30; i++)
		if (!items[i].active) {
			Item& it = items[i];
			it.active = true;
			it.type = d.forced ? ITEM_CANISTER : chooseItemType();
			it.x = d.x;
			it.y = DOOR_TOP - DOOR_H - itemHalfH(it.type) - 2;
			it.angle = 0;
			it.phase = (float)(rand() % 6);
			return;
		}
}

void updateDoors(bool canDrop) {
	for (int i = 0; i < 4; i++) {
		Door& d = doors[i];
		if (d.state == DOOR_CLOSED) {                       // slide along the rail
			float newX = d.x + d.speed * (1 + 0.6f * progress()) * DT;
			bool blocked = newX < 61 || newX > 939;         // rail ends
			for (int j = 0; j < 4; j++)                     // another door in the way
				if (j != i && fabsf(newX - doors[j].x) < DOOR_W + 30 && (doors[j].x - d.x) * d.speed > 0) blocked = true;
			if (blocked) d.speed = -d.speed; else d.x = newX;
		} else if (d.state == DOOR_WARNING) {
			d.timer += DT;
			if (d.timer >= 0.8f) { d.state = DOOR_OPENING; d.timer = 0; }
		} else if (d.state == DOOR_OPENING) {
			d.timer += DT;
			d.open = d.timer / 0.3f;
			if (d.open >= 1) { d.open = 1; d.state = DOOR_OPEN; d.timer = 0; d.dropped = !canDrop; }
		} else if (d.state == DOOR_OPEN) {
			d.timer += DT;
			if (!d.dropped) {
				if (spaceUnderDoor(d.x) || d.timer > 0.7f) { dropItem(d); d.dropped = true; d.timer = 0; }
			} else if (d.timer >= 0.35f) { d.state = DOOR_CLOSING; d.timer = 0; }
		} else if (d.state == DOOR_CLOSING) {
			d.timer += DT;
			d.open = 1 - d.timer / 0.3f;
			if (d.open <= 0) { d.open = 0; d.state = DOOR_CLOSED; d.forced = false; }
		}
	}

	// open a random closed door from time to time (more often as time passes)
	nextDropTime -= DT;
	if (nextDropTime <= 0) {
		int i = rand() % 4;
		if (doors[i].state == DOOR_CLOSED) {
			doors[i].state = DOOR_WARNING;
			doors[i].timer = 0;
			float wait = canDrop ? 1.45f - progress() : 2.2f;   // 1.45 s -> 0.45 s
			nextDropTime = wait * (0.75f + (rand() % 50) / 100.0f);
		} else nextDropTime = 0.1f;                     // that door is busy, try again soon
	}
}

void updatePlayer() {
	int dir = 0;
	if (keyRight) dir++;
	if (keyLeft) dir--;
	if (frozenTime > 0) { frozenTime -= DT; dir = 0; jumpPressed = false; }   // decontaminating
	float speed = speedTime > 0 ? 470.0f : 270.0f;
	playerMoving = dir != 0;
	if (playerMoving) { playerDir = dir; playerWalk += DT * 13; }
	playerX += (dir * speed + pushSpeed) * DT;
	pushSpeed *= 0.92f;                                     // push from the rival slows down
	if (jumpPressed && onGround) { playerVY = 660; onGround = false; }
	jumpPressed = false;
	playerVY -= 1750 * DT;                                  // gravity
	playerY += playerVY * DT;
	if (playerY <= FLOOR_Y) { playerY = FLOOR_Y; playerVY = 0; onGround = true; }
	if (playerX < 32) playerX = 32;                         // stay inside the game borders
	if (playerX > 968) playerX = 968;
	if (suitTime > 0) suitTime -= DT;
	if (speedTime > 0) speedTime -= DT;
}

void updateRival() {
	rivalX += rivalDir * (95 + 55 * progress()) * DT;
	rivalWalk += DT * 9;
	if (rivalX > 954) { rivalX = 954; rivalDir = -1; }      // turn back at the borders
	if (rivalX < 46) { rivalX = 46; rivalDir = 1; }

	// camouflage cycle of 8 s: visible 5 s, fade out 0.7 s, hidden 1.6 s, fade in 0.7 s
	camoTimer += DT;
	float t = fmodf(camoTimer, 8);
	if (t < 5) camo = 0;
	else if (t < 5.7f) camo = (t - 5) / 0.7f;
	else if (t < 7.3f) camo = 1;
	else camo = (8 - t) / 0.7f;

	// bumping into the rival pushes the player back (no life lost)
	if (boxesOverlap(playerX - 24, playerY, playerX + 24, playerY + 78, rivalX - 40, FLOOR_Y, rivalX + 40, FLOOR_Y + 112)) {
		float side = playerX >= rivalX ? 1.0f : -1.0f;      // which side the player is on
		bool againstWall = playerX < 40 || playerX > 960;
		if (frozenTime > 0 || againstWall) {                // player can't move: the rival turns back
			rivalDir = (int)-side;
			rivalX = playerX - side * 66;
		} else {
			playerX = rivalX + side * 65;
			pushSpeed = side * 520;
			if (onGround) { playerVY = 280; onGround = false; }
		}
	}
}

void updateItems() {
	float faster = 1 + 1.35f * progress();                  // items fall faster over time
	for (int i = 0; i < 30; i++) {
		if (!items[i].active) continue;
		Item& it = items[i];
		float fallSpeed = 130;
		if (it.type == ITEM_LAUGH) fallSpeed = 230;
		if (it.type == ITEM_SOCK || it.type == ITEM_DUCK) { fallSpeed = 150; it.angle += 260 * DT; }
		if (it.type >= ITEM_SUIT) fallSpeed = 115;
		it.y -= fallSpeed * faster * DT;
		it.phase += DT;
	}

	// a faster item waits behind a slower one so they never overlap
	for (int i = 0; i < 30; i++)
		for (int j = 0; j < 30; j++) {
			if (i == j || !items[i].active || !items[j].active || items[j].y >= items[i].y) continue;
			float minGap = itemHalfH(items[i].type) + itemHalfH(items[j].type) + 4;
			if (fabsf(items[i].x - items[j].x) < itemHalfW(items[i].type) + itemHalfW(items[j].type) && items[i].y - items[j].y < minGap)
				items[i].y = items[j].y + minGap;
		}

	for (int i = 0; i < 30; i++) {
		if (!items[i].active) continue;
		Item& it = items[i];
		float x1 = it.x - itemHalfW(it.type), x2 = it.x + itemHalfW(it.type);
		float y1 = it.y - itemHalfH(it.type), y2 = it.y + itemHalfH(it.type);
		bool toxic = it.type == ITEM_SOCK || it.type == ITEM_DUCK;

		if (boxesOverlap(x1, y1, x2, y2, playerX - 26, playerY, playerX + 26, playerY + 80)) {   // caught by the player
			it.active = false;
			if (it.type == ITEM_CANISTER) { score += 10; caught++; addPopup("+10", it.x, it.y + 20, 0.4f, 0.7f, 1); playEffect(catchSound, LENGTH(catchSound)); }
			if (it.type == ITEM_LAUGH) { score += 50; caught++; addPopup("+50", it.x, it.y + 20, 1, 0.85f, 0.2f); playEffect(catchSound, LENGTH(catchSound)); }
			if (it.type == ITEM_SUIT) { suitTime = POWER_TIME; addPopup("HAZMAT SUIT!", it.x, it.y + 20, 1, 0.8f, 0.2f); playEffect(powerSound, LENGTH(powerSound)); }
			if (it.type == ITEM_SPEED) { speedTime = POWER_TIME; addPopup("SPEED BOOST!", it.x, it.y + 20, 0.4f, 1, 0.6f); playEffect(powerSound, LENGTH(powerSound)); }
			if (toxic && frozenTime <= 0) {
				if (suitTime > 0) addPopup("BLOCKED!", it.x, it.y + 20, 1, 0.8f, 0.2f);
				else {                                       // 2319!
					lives--;
					frozenTime = 1;
					alarmTime = 1;
					pushSpeed = 0;
					playEffect(alarmSound, LENGTH(alarmSound));
				}
			}
		} else if (!toxic && boxesOverlap(x1, y1, x2, y2, rivalX - 40, FLOOR_Y, rivalX + 40, FLOOR_Y + 112)) {   // stolen by the rival
			it.active = false;
			if (it.type <= ITEM_LAUGH) stolen++;
			addPopup("STOLEN!", it.x, it.y + 20, 0.75f, 0.55f, 1);
			playEffect(stealSound, LENGTH(stealSound));
		} else if (y1 <= FLOOR_Y) {                          // hit the floor
			it.active = false;
			if (it.type <= ITEM_LAUGH) { missed++; addPopup("MISSED", it.x, FLOOR_Y + 30, 0.8f, 0.8f, 0.9f); }
		}
	}
}

void updatePopups() {
	for (int i = 0; i < 10; i++)
		if (popups[i].active) {
			popups[i].y += 45 * DT;                          // float upwards
			popups[i].life -= DT;
			if (popups[i].life <= 0) popups[i].active = false;
		}
}

// finds the door under the mouse (as in Lab 3)
void findHoverDoor() {
	hoverDoor = -1;
	for (int i = 0; i < 4; i++)
		if (mouseX > doors[i].x - DOOR_W / 2 - 6 && mouseX < doors[i].x + DOOR_W / 2 + 6 &&
			mouseY > DOOR_TOP - DOOR_H - 3 && mouseY < DOOR_TOP + 6)
			hoverDoor = i;
}

void Timer(int value) {
	if (gameState == STATE_START) {                         // doors keep moving behind the start screen
		animTime += DT;
		updateDoors(false);
	} else if (gameState == STATE_PLAYING && !paused) {
		animTime += DT;
		gameTime += DT;
		if (alarmTime > 0) alarmTime -= DT;
		if (stationTimer > 0) stationTimer -= DT;
		updatePlayer();
		updateRival();
		updateDoors(true);
		updateItems();
		updatePopups();
		findHoverDoor();
		if (score >= QUOTA) endGame(END_QUOTA);
		else if (lives <= 0) endGame(END_LOSE);
		else if (gameTime >= SHIFT_TIME) endGame(END_SHIFT);
	} else animTime += (gameState == STATE_END) ? DT : 0;   // end screen still animates

	glutPostRedisplay();
	glutTimerFunc(16, Timer, 0);
}


// ============================================================================
//  INPUT
// ============================================================================
void KeyDown(unsigned char key, int x, int y) {
	if (key == 27) exit(0);                                 // ESC
	if (gameState == STATE_START) { startGame(); return; }
	if (gameState == STATE_END) { if (key == 'r' || key == 'R') startGame(); return; }
	if (key == 'p' || key == 'P') {
		paused = !paused;
		keyLeft = keyRight = false;
		if (paused) waveOutPause(musicDevice); else waveOutRestart(musicDevice);
		return;
	}
	if (paused) return;
	if (key == 'a' || key == 'A') keyLeft = true;
	if (key == 'd' || key == 'D') keyRight = true;
	if (key == 'w' || key == 'W' || key == ' ') jumpPressed = true;
}

void KeyUp(unsigned char key, int x, int y) {
	if (key == 'a' || key == 'A') keyLeft = false;
	if (key == 'd' || key == 'D') keyRight = false;
}

void PassiveMotion(int x, int y) {
	mouseX = (float)x;
	mouseY = (float)(700 - y);                              // GLUT measures y from the top
	findHoverDoor();
}

void Mouse(int button, int state, int x, int y) {
	if (state != GLUT_DOWN) return;
	if (gameState == STATE_START) { startGame(); return; }
	if (gameState != STATE_PLAYING || paused || button != GLUT_LEFT_BUTTON) return;
	PassiveMotion(x, y);
	// Door Station: open the clicked door immediately and drop a canister
	if (hoverDoor >= 0 && stationTimer <= 0) {
		Door& d = doors[hoverDoor];
		if (d.state == DOOR_CLOSED || d.state == DOOR_WARNING) {
			d.state = DOOR_OPENING;
			d.timer = 0;
			d.forced = true;
			stationTimer = STATION_COOLDOWN;
		}
	}
}


// ============================================================================
//  MAIN
// ============================================================================
int main(int argc, char** argv) {
	srand((unsigned)time(NULL));
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);           // double buffering: no flicker
	glutInitWindowSize(1000, 700);
	glutInitWindowPosition(150, 50);
	glutCreateWindow("Scream Shift");
	glutDisplayFunc(Display);
	glutKeyboardFunc(KeyDown);
	glutKeyboardUpFunc(KeyUp);                             // so a key can be held down to walk
	glutIgnoreKeyRepeat(1);
	glutPassiveMotionFunc(PassiveMotion);
	glutMouseFunc(Mouse);
	glutTimerFunc(0, Timer, 0);

	glClearColor(0.05f, 0.05f, 0.08f, 0);
	glEnable(GL_BLEND);                                    // allows transparent colours (glColor4f)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);                              // smoother lines (makes the text look nicer)
	gluOrtho2D(0, 1000, 0, 700);

	resetGame();
	initSound();
	glutMainLoop();
	return 0;
}
