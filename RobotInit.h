#ifndef ROBOTINIT_H
#define ROBOTINIT_H
int FontRead(const char *fontfilename, struct FontData *FontSet);
int Initialisation();
#define MAXMOVEMENTS 50     // upper bound for movements per character
#define MAXCHARS     128    // ASCII range (0–127)

struct Instructions {
    int x;     // X offset from origin
    int y;     // Y offset from origin
    int pen;   // Pen state: 0 = up, 1 = down
};

struct FontData {
    int ascii_code;                   // ASCII value of the character
    int num_movements;                // Number of movements
    struct Instructions movements[MAXMOVEMENTS];  // Array of movements
};

extern struct FontData FontSet[MAXCHARS];    // Structure of all characters
#endif