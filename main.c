#include <stdio.h>
#include <stdlib.h>
#include <ctype.h> //Including standard C library to better parse text file
//#include <conio.h>
//#include <windows.h>
#include "rs232.h"
#include "serial.h"

#define DEBUG
#define bdrate 115200               /* 115200 baud */
#define MAXMOVEMENTS 20     // upper bound for movements per character
#define MAXCHARS     128    // ASCII range (0–127)

struct Instructions     //Structure of pen movement instructions
{   
    int x;     // X offset from origin
    int y;     // Y offset from origin
    unsigned int pen;   // Pen state: 0 = up, 1 = down
};

struct FontData     //Structure of the font header with nested structure of instructions      
 {
    unsigned int ascii_code;                        // ASCII value of the character
    unsigned int num_movements;                     // Number of movements
    struct Instructions movements[MAXMOVEMENTS];    // Array of movements
};
struct FontData FontSet[MAXCHARS];      //Populating the structure with spaces for every ascii character
int *TextInput = NULL;      //Array of ascii values in text file
int TextLength = 0;         //Length of text in file to appropriately allocate TextInput array size

void SendCommands (char *buffer );
int FontRead(const char *fontfilename, unsigned int FontHeight);
int Initialisation(const char *FontFile);
int TextParse(const char *textfilename, int **AsciiArray, int *outLength);

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

    Initialisation("SingleStrokeFont.txt"); // Initialse program for font data
    printf("Character: H (ASCII %u)\n",FontSet[72].ascii_code );
            printf("Number of movements: %u\n", FontSet[72].num_movements);

            for (unsigned int j = 0; j < FontSet[72].num_movements; j++) {
                printf("  Movement %u: x=%u, y=%u, pen=%u\n",
                       j,
                       FontSet[72].movements[j].x,
                       FontSet[72].movements[j].y,
                       FontSet[72].movements[j].pen);
            }
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

    if (TextParse("test.txt", &TextInput, &TextLength)) {
        printf("Read %d ASCII values:\n", TextLength);
        for (int i = 0; i < TextLength; i++) {
            printf("%d ", TextInput[i]);
        }
        printf("\n");
    } else {
        printf("Failed to parse text file.\n");
    }

    // These are sample commands to draw out some information - these are the ones you will be generating.
    sprintf (buffer, "G0 X-13.41849 Y0.000\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y-4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41849 Y0.0000\n");
    SendCommands(buffer);
    sprintf (buffer, "G1 X-13.41089 Y4.28041\n");
    SendCommands(buffer);
    sprintf (buffer, "S0\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X-7.17524 Y0\n");
    SendCommands(buffer);
    sprintf (buffer, "S1000\n");
    SendCommands(buffer);
    sprintf (buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands (char *buffer )
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer (&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
    // getch(); // Omit this once basic testing with emulator has taken place
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

        FontSet[count].ascii_code = ascii;  //Setting ascii identifier in header of structure from header of font character definition
        FontSet[count].num_movements = n;   //Setting number of movements in header of structure from header of font character definition

        for (int i = 0; i < n && i < MAXMOVEMENTS; i++) //Iterate through movements until specified movements in font header are finished or maximum limit is reached
        {
            int BeforeScaleY = 0; //Initilaise a temporary variable so Y commands can be modified by the user specified font height
            fscanf(fontfile, "%d %d %u",
                   &FontSet[count].movements[i].x,
                   &BeforeScaleY,
                   &FontSet[count].movements[i].pen); //Scan file for the three variables specified in the formatting of the file and assign them to the structure at that step
                   FontSet[count].movements[i].y = (int)(((double)BeforeScaleY / 18.0) * FontHeight + 0.5); //Modifies Y values by the specified height ensuring non integer values are rounded up
        }

        count++;
    }

    fclose(fontfile);
    return 1;  // return success
}

int Initialisation(const char *FileName) {
    unsigned int FontHeight = 0;
    while (FontHeight < 4 || FontHeight > 10 )
    {
        printf("Please input a height (in mm) between 4 and 10 for the text to be drawn at: ");
        scanf("%u",&FontHeight);
    }
    
    int Success = FontRead(FileName,FontHeight);
    if (Success)
    {
        printf("Font File Processed \n");
        return 1;
    }
    else
    {
        printf("Failed to read Font File \n");
        return 0;
    }
    
}

// Function: TextParse
// Reads characters from filename, stores ASCII values in caller-provided array.
// Returns 1 on success, 0 on failure.
int TextParse(const char *textfilename, int **AsciiArray, int *outLength) {
    FILE *TextFile = fopen(textfilename, "r");
    if (!TextFile) {
        return 0;   // Return failure
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

    *outLength = count;
    return 1;   //Return success
}


