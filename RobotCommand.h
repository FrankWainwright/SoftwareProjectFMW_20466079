#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> //Including standard C library to better parse text file

#ifndef ROBOTCOMMANDS_H
#define ROBOTCOMMANDS_H

#define MAXWIDTH 100     //Maximum writing width in mm
extern int NextWord[64];          //Array of ascii values comprising the next word to be translated into commands
extern int WordLength;         //Length of text in file to appropriately allocate TextInput array size
extern int WordPosition;   //Current location of the start/end of a word
extern unsigned int TrailingSpaces;        //How many spaces are after the current word

int TextParse(const int *TextArray, int TextLength,int *WordPosition, int *NextWord, int *WordLength,unsigned int *TrailingSpaces);
int GenerateGCode(int *NextWord, int WordLength,unsigned int TrailingSpaces);
void SendCommands (char *buffer );

#endif