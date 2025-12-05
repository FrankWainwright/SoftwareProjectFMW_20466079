#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> //Including standard C library to better parse text file
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

//#define DEBUG
#define bdrate 115200               /* 115200 baud */
#define MAXMOVEMENTS 20     //Upper bound for movements per character
#define MAXCHARS     128    //ASCII range (0–127)
#define MAXWIDTH 100     //Maximum writing width in mm

struct Instructions     //Structure of pen movement instructions
{   
    int x;     //X position from origin
    int y;     //Y position from origin
    unsigned int pen;   //Pen state: 0 = up, 1 = down
};

struct FontData     //Structure of the font header with nested structure of instructions      
 {
    unsigned int ascii_code;                        //ASCII value of the character
    unsigned int num_movements;                     //Number of movements
    struct Instructions movements[MAXMOVEMENTS];    //Array of movements
};
struct FontData FontSet[MAXCHARS];      //Populating the structure with spaces for every ascii character
int *TextInput = NULL;      //Array of ascii values in text file
int TextLength;         //Length of the text in the TextArray
int NextWord[64];          //Array of ascii values comprising the next word to be translated into commands
int WordLength;         //Length of text in file to appropriately allocate TextInput array size
int WordPosition = 0;   //Current location of the start/end of a word
unsigned int TrailingSpaces;        //How many spaces are after the current word      
unsigned int LineSpacing = 5;   //Spacing between successive lines in mm
int XOffset = 0;        //Offset applied from origin in X direction
unsigned int YOffset = 0;        //Offset applied from origin in Y direction


void SendCommands (char *buffer );
int FontRead(const char *fontfilename, unsigned int FontHeight);
int Initialisation(const char *FontFileName, const char *TextFileName);
int TextRead(const char *textfilename, int **AsciiArray, int *outLength);
int TextParse(const int *TextArray, int TextLength,int *WordPosition, int *NextWord, int *WordLength,unsigned int *TrailingSpaces);
int GenerateGCode(const int *NextWord, int WordLength,unsigned int TrailingSpaces);

int main()
{

    //char mode[]= {'8','N','1',0};
    char buffer[100];

    // If we cannot open the port then give up immediately
    if ( CanRS232PortBeOpened() == -1 )
    {
        printf ("\nUnable to open the COM port (specified in serial.h) ");
        exit (0);
    }

    // Time to wake up the robot
    printf ("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf (buffer, "\n");
     // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    Sleep(100);

    Initialisation("SingleStrokeFont.txt", "Test.txt");         //Initialse program for font data
    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf ("\nThe robot is now ready to draw\n");

        //These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf (buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf (buffer, "M3\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);

    #ifdef DEBUG        //Debug value tests
        printf("Character: H (ASCII %u)\n",FontSet[72].ascii_code );
        printf("Number of movements: %u\n", FontSet[72].num_movements);

            for (unsigned int j = 0; j < FontSet[72].num_movements; j++) 
            {
                printf("  Movement %u: x=%u, y=%u, pen=%u\n",
                       j,
                       FontSet[72].movements[j].x,
                       FontSet[72].movements[j].y,
                       FontSet[72].movements[j].pen);
            }
        printf("Read %d ASCII values:\n", TextLength);
        for (int i = 0; i < TextLength; i++)
        {
            printf("%d ", TextInput[i]);
        }
    #endif
    
    while (TextParse(TextInput, TextLength, &WordPosition, NextWord, &WordLength,&TrailingSpaces)) 
    {
    
        if (WordLength > 0)     //If TextParse returns 1 when a word or CR/LF was successfully extracted
        {
        
            if (GenerateGCode(NextWord, WordLength,TrailingSpaces))        //Generate G-code for this word and send commands
            {
            //Show debug info
            #ifdef DEBUG
            printf("Processed word of length %d at position %d\n", WordLength, WordPosition);
            #endif
            }
            else 
            {
            printf("Error generating G-code for word at position %d\n", WordPosition);
            }
        }
    }

// After finishing all words, ensure pen is up and return to origin
sprintf(buffer, "S0 G0 X0 Y0\n");   // Pen up, move to (0,0)
SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");
    free(TextInput);        //Free up allocated memory
    TextInput = NULL;       //Remove pointer address
    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
   
    //printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
}

int FontRead(const char *fontfilename, unsigned int FontHeight) //Function to load fontfile, and populate structure 
{
    FILE *fontfile = fopen(fontfilename, "r"); //Opens specified file
    if (!fontfile)
     {
        return 0; //Return Failure
    }

    int marker, ascii, n; //Initialising values required to read file
    int count = 0;

    while (fscanf(fontfile, "%d %d %d", &marker, &ascii, &n) == 3) //Continues to read through file until its end or until its formating is malformed
    {
        if (marker != 999) {
            // ignore lines that do not contain 999 for defining new character
            continue;
        }

        if (count >= MAXCHARS) //Checking if more definitions than specified ascii set (standard 128)
         {
            printf("Warning: exceeded MAXCHARS limit.\n");
            break;
        }

        FontSet[ascii].ascii_code = ascii;  //Setting ascii identifier in header of structure from header of font character definition
        FontSet[ascii].num_movements = n;   //Setting number of movements in header of structure from header of font character definition

        for (int i = 0; i < n && i < MAXMOVEMENTS; i++) //Iterate through movements until specified movements in font header are finished or maximum limit is reached
        {
            int BeforeScaleY = 0; //Initilaise a temporary variable so Y commands can be modified by the user specified font height
            int BeforeScaleX = 0; //Initilaise a temporary variable so X commands can be modified by the user specified font height
            fscanf(fontfile, "%d %d %u",
                   &BeforeScaleX,
                   &BeforeScaleY,
                   &FontSet[ascii].movements[i].pen); //Scan file for the three variables specified in the formatting of the file and assign them to the structure at that step
                   FontSet[ascii].movements[i].x = (int)(((double)FontHeight/ 18.0) * BeforeScaleX + 0.5);  //Modifies X values by the specified height ensuring non integer values are rounded up
                   FontSet[ascii].movements[i].y = (int)(((double)FontHeight/ 18.0) * BeforeScaleY + 0.5);  //Modifies Y values by the specified height ensuring non integer values are rounded up
        }

        count++;
    }

    fclose(fontfile);
    return 1;  // return success
}
int TextRead(const char *TextFileName, int **AsciiArray, int *TextLength)       //Function to read ascii values from .txt, put them in an array and format the array for ease of use
{
    FILE *TextFile = fopen(TextFileName, "r");      //Open text file
    if (!TextFile) {
        return 0;   //Return failure
    }

    
    fseek(TextFile, 0, SEEK_END);     //Seek the end of the file
    long fileSize = ftell(TextFile);      //Assign filesize based on end of file
    rewind(TextFile);

    
    *AsciiArray = malloc(fileSize * sizeof(int));       //Create Array based on the size of the file
    if (!*AsciiArray) {
        fclose(TextFile);
        return 0;       //Return failure
    }

    int count = 0;
    int ch;
    while ((ch = fgetc(TextFile)) != EOF)     //Iterating through all characters
    {
        (*AsciiArray)[count++] = ch;        //Inputting ascii characters into the array
    }
    fclose(TextFile);       //Close file stream

    
    while (count > 0 && isspace((*AsciiArray)[count - 1]))      //Removing empty space from array to make parsing easier
    {
        count--;
    }

    int *resized = realloc(*AsciiArray, count * sizeof(int));       //Freeing unused memory
    if (resized) {
        *AsciiArray = resized;
    }

    *TextLength = count;
    return 1;   //Return success
}
int Initialisation(const char *FontFileName, const char *TextFileName) //Function to handle all file read related operations at the start of the program and trap errors
{
    unsigned int FontHeight = 0;        //User specified value for the height they want letters to be written at
    char input[32];     //Buffer for user input, ensuring it doesnt break input loop
   for (;;)     //Run forever until correct input is given
   {
        printf("Please input a height (in mm) between 4 and 10 for the text to be drawn at: ");
        if (fgets(input, sizeof(input), stdin))     //Process user input
         {
            if (sscanf(input, "%u", &FontHeight) == 1)  //Test if the value contains an unsigned integer
            {
                if (FontHeight >= 4 && FontHeight <= 10)    //Test if the integer value is within specified limits
                {
                    break;  //End infinite loop 
                }
            }
        }
        printf("Invalid input. Please enter a whole number between 4 and 10.\n");
    }
    YOffset -= FontHeight;       //Ensure text starts below Y 0
    LineSpacing += FontHeight;      //Set Linespacing to be the distance between lines and the height of a line

    if (FontRead(FontFileName,FontHeight))      //Read font instructions and populate structure with it
    {
        printf("Font File Processed \n");
        
    }
    else
    {
        printf("Failed to read Font File \n");
        exit(1);
    }

    if (TextRead(TextFileName, &TextInput, &TextLength))     //Read text and populate array with it
    {
        printf("Text file processed \n"); //Confirm Sucess
    }
    else       
    {
        printf("Failed to process text file.\n"); //Confirm Error
        exit(1);
    }
    return 1;
    
}

int TextParse(const int *TextArray, int TextLength,int *WordPosition, int *NextWord, int *WordLength, unsigned int *TrailingSpaces)       //Function to seperate TextArray into words that can be processed into GCode commands
{
    if (*WordPosition >= TextLength)      //Check if end of text has been reached  
    {
        *WordLength = 0;
        *TrailingSpaces = 0;       
        return 0;  //Return no word / Failure
    }

    int count = 0;  //Counter to track how many ascii values have been counted, therefore the word length
    *TrailingSpaces = 0;    //Reseting trailing spaces for each word

    if (TextArray[*WordPosition] == 13)     //If CR is in word
    { 
    NextWord[count++] = 13; //Store CR
    (*WordPosition)++;
    if (*WordPosition < TextLength && TextArray[*WordPosition] == 10) //If LF follows
    { 
        NextWord[count++] = 10; //Store LF
        (*WordPosition)++;
    }
    *WordLength = count;    
    return 1; //Return new word / Success
    }
    else if (TextArray[*WordPosition] == 10) //If just LF is in word
    { 
    NextWord[count++] = 10; //Store LF
    (*WordPosition)++;
    *WordLength = count;
    return 1;   //Return new word / Success
    }

    while (*WordPosition < TextLength &&       //If ascii value is part of a word process it  
           TextArray[*WordPosition] != 32 &&   //Space
           TextArray[*WordPosition] != 13 &&   //CR
           TextArray[*WordPosition] != 10)     //LF
    {   
        NextWord[count++] = TextArray[*WordPosition];       //Saving TextArray value at that point to the NextWord array
        (*WordPosition)++;
    }
    
    while (*WordPosition < TextLength && TextArray[*WordPosition] == 32)    //Count the spaces left after a word
    {
        (*TrailingSpaces)++;
        (*WordPosition)++;
    }


    if (*WordPosition < TextLength &&     //Skip past special characters after they have been identified earlier  
        (TextArray[*WordPosition] == 32 ||
         TextArray[*WordPosition] == 13 ||
         TextArray[*WordPosition] == 10)) 
    {
        (*WordPosition)++;
    }

    *WordLength = count;        //Set WordLength to the amount of ascii values processed in current word
    return 1;  //Return new word / Success
}

int GenerateGCode(const int *NextWord, int WordLength, unsigned int TrailingSpaces)
{
    char buffer[128];

  
    if (NextWord[0] == 13 || NextWord[0] == 10)        //Processing new line / return carriage ascii values
    {
        
        XOffset = 0;                //Move X offset back to the origin (return carriage)
        YOffset -= LineSpacing;    //Move Y down a line
        return 1; //Return Success
    }

    
    int WordWidth = 0;
    for (int i = 0; i < WordLength; i++)        //Iterate across word
    {
        int ascii = NextWord[i];
        if (ascii < 0 || ascii >= MAXCHARS) continue;       //Error handling invalid inputs
        struct FontData letter = FontSet[ascii];            //Pulls structure data for the current letter and gives it a local label "letter"
        if (letter.num_movements > 0)
        {
            WordWidth += letter.movements[letter.num_movements - 1].x;       //Width based on position of the last movement
        }
    }

    
    if (XOffset + WordWidth > MAXWIDTH)     //If word exceeds width limit
    {
        
        XOffset = 0;        //Send carriage back                
        YOffset -= LineSpacing;  // negative Y direction per spec
    }

    
    for (int i = 0; i < WordLength; i++)        //Generate G-code for each letter in the word
    {
        int ascii = NextWord[i];
        if (ascii < 0 || ascii >= MAXCHARS) continue;       //Error handling invalid inputs

        struct FontData letter = FontSet[ascii];    //Pulls structure data for the current letter and gives it a local label "letter"

        for (unsigned int j = 0; j < letter.num_movements; j++)     //Iterate through each movement for the ascii value
        {
            struct Instructions move = letter.movements[j];
            if ((XOffset + move.x)>= MAXWIDTH)
            {
                XOffset = 0;        //Send carriage back                
                YOffset -= LineSpacing;  // negative Y direction per spec
            }

            int TargetX = XOffset + move.x;     //Set X coordinate based on font instruction and continuing offset
            int TargetY = YOffset + move.y;     //Set Y coordinate based on font instruction and continuing offset

            if (move.pen == 1)      //If pen down is specified in font
            {
                sprintf(buffer, "S1000 G1 X%d Y%d\n", TargetX, TargetY);        //Set spindle on and movement to steady while moving to position
            } else {
                sprintf(buffer, "S0 G0 X%d Y%d\n", TargetX, TargetY);           //Set spindle off and movement to rapid while moving to position
            }
            SendCommands(buffer);   //Send Commands
        }

        
        if (letter.num_movements > 0)       //If there are movements
        {
            XOffset += letter.movements[letter.num_movements - 1].x;        //Update XOffset for next letter
        }
    }
    struct FontData Space = FontSet[32];    //Pulling the movement data of space as it has been scaled 
    XOffset += TrailingSpaces*Space.movements[0].x;     //Increasing the offset by the spaces between words
    return 1;       //Return Success
}


