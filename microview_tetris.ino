// MicroView Arduino-compatible C port by Richard Birkby
// Original JavaScript implementation - Jake Gordon - https://github.com/jakesgordon/javascript-tetris
// MIT licenced

#include <avr/pgmspace.h>
#include <MicroView.h>
#include "Tetris.h"
#include "Playtune.h"
#include "Bounce2.h"

Playtune pt;

enum DIR {
  UP,
  RIGHT,
  DOWN,
  LEFT,
  MIN = UP,
  MAX = LEFT
};

const int nx = 10; // width of tetris court (in blocks)
const int ny = 12; // height of tetris court (in blocks)

const int rotateButton = 5;
const int leftButton = 3;
const int rightButton = 2;

const int audioPin1 = A3;
const int audioPin2 = A4;

//-------------------------------------------------------------------------
// game variables (initialized during reset)
//-------------------------------------------------------------------------

const int dx = (uView.getLCDWidth() - 24) / nx;   // pixel size of a single tetris block
const int dy = uView.getLCDHeight() / ny;
int blocks[nx][ny];                               // 2 dimensional array (nx*ny) representing tetris court - either empty block or occupied by a 'piece'
bool playing = true;                              // true|false - game is in progress
bool lost = false;
Piece current, next;                              // the current and next piece
unsigned int score = 0;                           // the current score
int dir = 0;

//-------------------------------------------------------------------------
// tetris pieces
//
// blocks: each element represents a rotation of the piece (0, 90, 180, 270)
//         each element is a 16 bit integer where the 16 bits represent
//         a 4x4 set of blocks, e.g. j.blocks[0] = 0x44C0
//
//             0100 = 0x4 << 3 = 0x4000
//             0100 = 0x4 << 2 = 0x0400
//             1100 = 0xC << 1 = 0x00C0
//             0000 = 0x0 << 0 = 0x0000
//                               ------
//                               0x44C0
//
//-------------------------------------------------------------------------

unsigned int i[] = {0x0F00, 0x2222, 0x00F0, 0x4444};
unsigned int j[] = {0x44C0, 0x8E00, 0x6440, 0x0E20};
unsigned int l[] = {0x4460, 0x0E80, 0xC440, 0x2E00};
unsigned int o[] = {0xCC00, 0xCC00, 0xCC00, 0xCC00};
unsigned int s[] = {0x06C0, 0x8C40, 0x6C00, 0x4620};
unsigned int t[] = {0x0E40, 0x4C40, 0x4E00, 0x4640};
unsigned int z[] = {0x0C60, 0x4C80, 0xC600, 0x2640};

Bounce btn1 = Bounce();
Bounce btn2 = Bounce();
Bounce btn3 = Bounce();

//------------------------------------------------
// do the bit manipulation and iterate through each
// occupied block (x,y) for a given piece
//------------------------------------------------
void getPositionedBlocks(unsigned int type[], int x, int y, int dir, Block* positionedBlocks) {
 unsigned int bit;
 int blockIndex = 0, row = 0, col = 0, blocks = type[dir];

 for(bit = 0x8000; bit > 0 ; bit = bit >> 1) {
   if (blocks & bit) {
     Block block = {x+col, y+row};

     positionedBlocks[blockIndex++] = block;
   }
   if (++col == 4) {
     col = 0;
     ++row;
   }
 }
}

//-----------------------------------------------------
// check if a piece can fit into a position in the grid
//-----------------------------------------------------
bool occupied(unsigned int type[], int x, int y, int dir) {
 bool result = false;

 Block positionedBlocks[4];
 getPositionedBlocks(type, x, y, dir, positionedBlocks);

 for (int i = 0; i < 4; i++) {
   Block block = positionedBlocks[i];

   if ((block.x < 0) || (block.x >= nx) || (block.y < 0) || (block.y >= ny) || getBlock(block.x, block.y))
     result = true;
 }

 return result;
}

bool unoccupied(unsigned int type[], int x, int y, int dir) {
 return !occupied(type, x, y, dir);
}

unsigned int *pieces[] = {i,j,l,o,s,t,z};

Piece randomPiece() {
 Piece current = {pieces[random(0, 6)], 3, 0, UP};
 return current;
}

void setup() {
 uView.begin();
 uView.clear(ALL);
 uView.display();

 randomSeed(millis());

 pt.tune_initchan(audioPin1);
 pt.tune_initchan(audioPin2);

 reset();

 btn1.attach(leftButton, INPUT);
 btn1.interval(25);
 btn2.attach(rightButton, INPUT);
 btn2.interval(25);
 btn3.attach(rotateButton, INPUT);
 btn3.interval(25);
}

void loop() {
 //ButtonManager::instance()->checkButtons();
 int time = millis();

 btn1.update();
 btn2.update();
 btn3.update();
 if (btn1.rose()) { onLeftButton(); }
 if (btn2.rose()) { onRightButton(); }
 if (btn3.rose()) { onRotateButton(); }

 if (time % 60 == 0) {
   draw();

   if (playing && (time%300 == 0)) {
     if (!pt.tune_playing) {
       pt.tune_playscore(tetrisScore);
     }

     drop();

     if (lost) {
       draw();
       delay(1000);
     }
   }
 }
}

void onLeftButton() {
 if (lost) {
   reset();
 }
 
 move(LEFT);
}

void onRightButton() {
 if (lost) {
   reset();
 }
 
 move(RIGHT);
}

void onRotateButton() {
 if (lost) {
   reset();
 }

 rotate();
}

//-------------------------------------------------------------------------
// GAME LOGIC
//-------------------------------------------------------------------------

void lose() { playing = false; lost = true; pt.tune_stopscore(); }

void addScore(int n) { score = score + n; }
void clearScore() { score = 0; }
int getBlock(int x, int y) { return (blocks[x] ? blocks[x][y] : NULL); }
void setBlock(int x, int y, int type) { blocks[x][y] = type; }
void clearBlocks() { for(int x = 0; x < nx; x++) { for(int y = 0; y < ny; y++) { blocks[x][y] = NULL; } } }
void setCurrentPiece(Piece piece) {current = piece;}
void setNextPiece(Piece piece) {next = piece;}

void reset() {
 lost = false;
 playing = true;
 clearBlocks();
 clearScore();
 setCurrentPiece(randomPiece());
 setNextPiece(randomPiece());
}

bool move(int dir) {
 int x = current.x, y = current.y;
 switch(dir) {
   case RIGHT: x = x + 1; break;
   case LEFT:  x = x - 1; break;
   case DOWN:  y = y + 1; break;
 }
 if (unoccupied(current.type, x, y, current.dir)) {
   current.x = x;
   current.y = y;
   return true;
 }
 else {
   return false;
 }
}

void rotate() {
  int newdir = (current.dir == MAX ? MIN : current.dir + 1);
  if (unoccupied(current.type, current.x, current.y, newdir)) {
    current.dir = newdir;
  }
}

void drop() {
 if (!move(DOWN)) {
   addScore(10);
   dropPiece();
   removeLines();
   setCurrentPiece(next);
   setNextPiece(randomPiece());

   if (occupied(current.type, current.x, current.y, current.dir)) {
     lose();
   }
 }
}

void dropPiece() {
 Block positionedBlocks[4];
 getPositionedBlocks(current.type, current.x, current.y, current.dir, positionedBlocks);

 for (int i = 0; i < 4; i++) {
   setBlock(positionedBlocks[i].x, positionedBlocks[i].y, -1);
 }
}

void removeLines() {
 int x, y, n = 0;
 bool complete;

 for (y = ny; y > 0; --y) {
   complete = true;

   for(x = 0; x< nx; ++x) {
     if (!getBlock(x, y)) complete = false;
   }

   if (complete) {
     removeLine(y);
     y = y + 1; // recheck same line
     n++;
   }
 }
 if (n > 0) {
   addScore(100*pow(2, n-1)); // 1: 100, 2: 200, 3: 400, 4: 800
 }
}

void removeLine(int n) {
 int x, y;
 for(y = n; y >= 0; --y) {
   for(x = 0; x < nx; ++x) {
     setBlock(x, y, (y==0) ? NULL : getBlock(x, y-1));
   }
 }
}

//-------------------------------------------------------------------------
// RENDERING
//-------------------------------------------------------------------------

void draw() {
 drawCourt();
 drawNext();
 drawScore();

 removeLines();
 uView.display();
}

void drawCourt() {
 uView.clear(PAGE);
 if (playing)
   drawPiece(current.type, current.x + 6, current.y, current.dir);

 int x, y;
 for(y = 0 ; y < ny ; y++) {
   for (x = 0 ; x < nx ; x++) {
     if (getBlock(x, y))
       drawBlock(x + 6, y);
   }
 }

 if (lost) {
   uView.setCursor(0, 20);
   uView.print(" Game Over ");
 }
}

void drawNext() {
 if (playing)
   drawPiece(next.type, 0, 3, next.dir);
}

void drawScore() {
 uView.setCursor(0, 0);
 uView.print(score);
}

void drawPiece(unsigned int type[], int x, int y, int dir) {
 Block positionedBlocks[4];
 getPositionedBlocks(type, x, y, dir, positionedBlocks);

 for (int i = 0; i < 4; i++) {
   drawBlock(positionedBlocks[i].x, positionedBlocks[i].y);
 }
}

void drawBlock(int x, int y) {
 uView.rect(x*dx, y*dy, dx, dy);
}
