#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> //Including standard C library to better parse text file

#ifndef ROBOTINIT_H
#define ROBOTINIT_H

#define MAXMOVEMENTS 20     //Upper bound for movements per character
#define MAXCHARS     128    //ASCII range (0–127)

struct Instructions     //Structure of pen movement instructions
{   
    double x;     //X position from origin
    double y;     //Y position from origin
    unsigned int pen;   //Pen state: 0 = up, 1 = down
};
struct FontData     //Structure of the font header with nested structure of instructions      
 {
    unsigned int ascii_code;                        //ASCII value of the character
    unsigned int num_movements;                     //Number of movements
    struct Instructions movements[MAXMOVEMENTS];    //Array of movements
};
extern struct FontData FontSet[MAXCHARS];  // Global font set
extern double XOffset;                        // Offset applied from origin in X direction
extern double YOffset;               // Offset applied from origin in Y direction
extern int *TextInput;      //Array of ascii values in text file
extern int TextLength;         //Length of the text in the TextArray
extern unsigned int LineSpacing;   //Spacing between successive lines in mm

int FontRead(const char *fontfilename, unsigned int FontHeight);
int Initialisation(const char *FontFileName, const char *TextFileName);
int TextRead(const char *textfilename, int **AsciiArray, int *outLength);

#endif

