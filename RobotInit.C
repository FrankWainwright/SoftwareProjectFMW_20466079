#include <stdio.h>
#include <stdlib.h>
#include "RobotInit.h"

int FontRead(const char *fontfilename, struct FontData *FontSet) {
    FILE *fontfile = fopen(fontfilename, "r");
    if (!fontfile) {
        printf("Error opening file: %s\n", fontfilename);
        return 0;
    }

    int marker, ascii, n;
    int count = 0;

    // Loop until EOF
    while (fscanf(fontfile, "%d %d %d", &marker, &ascii, &n) == 3) {
        if (marker != 999) {
            // skip malformed lines
            continue;
        }

        if (count >= MAXCHARS) {
            printf("Warning: exceeded MAXCHARS limit.\n");
            break;
        }

        FontSet[count].ascii_code = ascii;
        FontSet[count].num_movements = n;

        for (int i = 0; i < n && i < MAXMOVEMENTS; i++) {
            fscanf(fontfile, "%d %d %d",
                   &FontSet[count].movements[i].x,
                   &FontSet[count].movements[i].y,
                   &FontSet[count].movements[i].pen);
        }

        count++;
    }

    fclose(fontfile);
    return count;  // number of characters read
}

int Initialisation() {
    int numChars = FontRead("SingleStrokeFont.txt", FontSet);
    printf("Loaded %d characters from font file.\n", numChars);
    return numChars;
}