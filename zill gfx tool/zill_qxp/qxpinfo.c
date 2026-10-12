#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

typedef struct
{
    int offset;
    int width;
    int height;
    char name[64]; // with extension
    char name2[64]; // without extension
    int colormode;
    int canvas_size; 
    int canvas_bytes; // from header, may be different from canvas
    int palette_size; // 1024 for 8bit or 64 for 4bit
    int mode; // 8 or 4
    int swizzle; // mode=4 then swizzle off -- mode=8 palette swizzle, pixels unknow 
} QTX_HEADER;


int main(int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;
    uint8 *readbuf, *pxbuf, *palette;
    int fsize, width, height, pxlen;
    int colormode;
    char *name, *name2;

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
    
    //printf("file;name;offset;width;height;colormode\n");

    fseek(infile, 0, SEEK_END);
    fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    readbuf = (uint8 *)malloc(fsize);
    fread(readbuf, 1, fsize, infile);
    fclose(infile);
    
    // search
    int offset;
    
    // read index   
    uint16 total_files;
    total_files = *(int*)(readbuf);
    printf("Total files inside container: %d\n", total_files);
    
    QTX_HEADER *qtx;
    qtx=calloc(total_files,sizeof(QTX_HEADER));

    for(int i=0; i<total_files; i++) {
        offset = *(uint32*)(readbuf+0x10+i*(sizeof(uint32)));

        width = *(int*)(readbuf + 0x30 + offset); // 32bits, 4 bytes
        height = *(int*)(readbuf + 0x34 + offset); 
        name = readbuf + 0x60 + offset;
        name2= readbuf + 0xA0 + offset;

        qtx[i].offset=offset;
        qtx[i].width=width;
        qtx[i].height=height;
        strcpy(qtx[i].name, name);
        strcpy(qtx[i].name2, name2);
        qtx[i].colormode=readbuf[offset+0xf6];
        qtx[i].canvas_size=width*height;
        qtx[i].canvas_bytes=*(int*)(readbuf + 0x108 + offset)-128;
        // lets guess
        if (qtx[i].canvas_bytes==qtx[i].canvas_size/2) {
            qtx[i].mode=4; // 4 bits per pixel
            qtx[i].palette_size=64;
        } else {
            qtx[i].mode=8; // 8 bits per pixel
            qtx[i].palette_size=1024;
        }

        printf("%d: %d %s %dx%d \n", i, qtx[i].offset, qtx[i].name, qtx[i].width, qtx[i].height ); 

    }   
    
    int key;
    key=0;
    do{ 
        printf(":: Choose one to see details (-1 to exit): ");
        scanf("%d", &key);
        if ((key!=-1)&&(key<=total_files)) {
            printf("name= %s \n", qtx[key].name );
            printf("w= %d \n", qtx[key].width );
            printf("h= %d \n", qtx[key].height );
            printf("colormode= %d \n", qtx[key].colormode );
            printf("canvas_size= %d \n", qtx[key].canvas_size );
            printf("canvas_bytes= %d \n", qtx[key].canvas_bytes );
            printf("bytes per pixel= %d \n", qtx[key].mode );
            printf("palette_size= %d \n", qtx[key].palette_size );

        };
        if (key>total_files){
            for(int i=0; i<total_files; i++) {
                printf("%d: %d %s %dx%d \n", i, qtx[i].offset, qtx[i].name, qtx[i].width, qtx[i].height ); 
            }
        }
    } while (key!=-1);

    free(readbuf);
    return 0;
}
