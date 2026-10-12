#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

int main(int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;
    uint8 *readbuf, *pxbuf, *palette;
    int fsize, width, height, pxlen;
    int colormode;
    char *name;

    if (argc < 2)
    {
        printf("\nusage:\n   %s <input.qtx> \n", argv[0]);
        return 1;
    }

    infile = fopen(argv[1], "rb");
    if (infile == NULL)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }
    //printf("version 0.1\n");

    fseek(infile, 0, SEEK_END);
    fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    readbuf = (uint8 *)malloc(fsize);
    fread(readbuf, 1, fsize, infile);
    fclose(infile);

    //properties
    width = *(int*)(readbuf + 0x30); // 32bits, 4 bytes
    height = *(int*)(readbuf + 0x34); 
    name = readbuf + 0x60;

    pxlen = width * height;
    pxbuf = readbuf + 0x200; // pxbuf offset 512 , bloque de pixels
    palette = pxbuf + pxlen + 0x80; // despues de la cabecera y de los pixels y 128 bytes más tarde está la paleta

    colormode = readbuf[0xF6]; //
    
    printf("%s;%s;%i;%i;%i\n", argv[1], name, width, height,colormode);

    free(readbuf);
    return 0;
}
