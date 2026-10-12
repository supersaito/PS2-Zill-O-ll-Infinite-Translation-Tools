#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct
{
    char id_len; //zero
    char colortype; //0=no palette, 1=palette
    char imgtype; //1=palettized, 2=raw
    short palette_firstidx; //zero
    short palette_numcolors;
    char palette_bpp; //32
    short origin_x; //zero
    short origin_y; //zero
    short width;
    short height;
    char img_bpp; //8 for palettized
    char descriptor; // 0x20 for upside down 
} TGAHDR; // 18 bytes
#pragma pack(pop)

typedef int int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

char SCALELIST[] = {' ','.',':','-','=','+','*','#','%','@',0};

void makeTargaRaw(uint8 *pixels_indexed, uint8 *pixels_rgba, uint8 *palette, int width, int height){
  int x, y, pos, alfa, average;
  int idx;

  for(y=0; y<height; y++){
    for(x=0; x<width; x++){
        pos = x+(y*width); 
        idx = pixels_indexed[pos] *4;
        pos= pos*4;
        pixels_rgba[pos] = palette[idx];    
        pixels_rgba[pos+1] = palette[idx+1];
        pixels_rgba[pos+2] = palette[idx+2];
        pixels_rgba[pos+3] = palette[idx+3];

        average = (pixels_rgba[pos] + pixels_rgba[pos+1]+ pixels_rgba[pos+2])/3; // 0..255
        average = average/25.5;

        //printf("%s",SCALELIST[average]);
        //printf("%i",average);
    }
    //printf("\n");
  }
}

static void clut_swizzle(unsigned char *clut)
{
    unsigned int *newclut = (unsigned int*)clut;
    unsigned int origclut[256]; // 4 char (32 bytes)= 1 int (32 bytes)
    unsigned int i, j, k;
    memcpy(origclut, clut, 1024); // dest, source, size
    for (k = 0; k < 8; k++)
    for (j = 0; j < 2; j++)
    for (i = 0; i < 8; i++)
    {
        newclut[k * 32 + j * 16 + i] =     origclut[k * 32 + 8 * j + i];
        newclut[k * 32 + j * 16 + i + 8] = origclut[k * 32 + 8 * j + 16 + i];
    }
}
static void clut_swapcolor(unsigned char *buf)
{
    int i, a;
    unsigned char t;

    for (i = 0; i < 256; i++)
    {
        //swap red and blue
        t = buf[2];
        buf[2] = buf[0];
        buf[0] = t;

        //scale up alpha (0-128) to (0-255)
        a = buf[3] << 1;
        if (a > 255) { a = 255; }
        buf[3] = a;

        buf += 4;
    }
}

static void flipY(unsigned char *buf, int width, int height)
{
    int i, j;
    unsigned char *temp;
    unsigned char *ptemp, *pbuf;

    temp = (unsigned char*)malloc(width * height * 1);
    memcpy(temp, buf, width * height * 1);

    ptemp = (unsigned char*)temp + width * (height - 1);
    //ptemp = (unsigned char*)temp + width * (height );
    pbuf = (unsigned char*)buf;

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            pbuf[j] = ptemp[j];
        }
        ptemp -= width;
        pbuf += width;
    }

    free(temp);
}
void unSwizzle32to8(uint8 *pSwizTexels, uint8 *pTexels, const int32 width, const int32 height)
{
   int x, y;

   for(y=0; y<height; y++)
      for(x=0; x<width; x++)
      {
         const int32 block_location = (y&(~0xf))*width + (x&(~0xf))*2; 
         const uint32 swap_selector = (((y+2)>>2)&0x1)*4;
         const int32 posY = (((y&(~3))>>1) + (y&1))&0x7;
         const int32 column_location = posY*width*2 + ((x+swap_selector)&0x7)*4;

         const int32 byte_num = ((y>>1)&1) + ((x>>2)&2);     // 0,1,2,3

         pTexels[y*width+x] = pSwizTexels[block_location + column_location + byte_num]; //unSwizzle
      }
}

/*
main
*/
int main(int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;
    uint8 *readbuf, *pxbuf, *palette;
    int fsize, width, height, pxlen;
    int colormode;

    TGAHDR tgahdr;

    if (argc < 3)
    {
        printf("\nusage:\n   %s <input.qtx> <output.tga> [shift 1|2|3] \n", argv[0]);
        return 1;
    }

    infile = fopen(argv[1], "rb");
    if (infile == NULL)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }

    uint8 shift = 0;
    
    if (argc >= 4)
    {
        shift = atoi(argv[3]);
        printf("override shift = %d\n", shift);
    }

    printf("version 2.\n");

    fseek(infile, 0, SEEK_END);
    fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    readbuf = (uint8 *)malloc(fsize);
    fread(readbuf, 1, fsize, infile);
    fclose(infile);

    // offset
    char magic[5];
    int offset;
    offset =0;
    magic[4]=0;
    
    if (offset==0){
        for(int i=0; i<fsize-4; i++){
            magic[0]=*(char*) (readbuf+i);
            magic[1]=*(char*) (readbuf+i+1);
            magic[2]=*(char*) (readbuf+i+2);
            magic[3]=*(char*) (readbuf+i+3);

            if (strcmp(magic,"MFZQ")==0) { 
                offset=i; 
                printf("autodetect offset = %d\n", offset);
                break;
            };
        }
    }

    //properties
    printf("internal name: %s\n",readbuf+offset+0x60);

    width = *(int*)(readbuf + offset + 0x30); // width in pos 0x30, 0x31
    height = *(int*)(readbuf + offset + 0x34); // height in pos 0x34, 0x35
    
    colormode = readbuf[0xF6+offset];
    printf("auto-detect colormode = %d\n", colormode);

    printf("auto-detect dimensions = %dx%d\n", width, height);

    width = width >> shift;
    height = height << shift;

    printf("shifted dimensions = %dx%d\n", width, height);
    
    pxlen = width * height;
    pxbuf = readbuf + offset + 0x200; // pxbuf offset 512 , bloque de pixels
    palette = pxbuf + pxlen + 0x80; // despues de la cabecera y de los pixels y 128 bytes más tarde está la paleta

    //set up TGA header
    tgahdr.id_len = 0;
    tgahdr.colortype = 0;
    tgahdr.imgtype = 2;
    tgahdr.palette_firstidx = 0;
    tgahdr.palette_numcolors = 0;
    tgahdr.palette_bpp = 0;
    tgahdr.origin_x = 0;
    tgahdr.origin_y = 0;
    tgahdr.width = width;
    tgahdr.height = height;
    tgahdr.img_bpp = 32;
    tgahdr.descriptor = 8;

    uint8 *pxout;
    pxout = (uint8 *)malloc(pxlen);
    unSwizzle32to8(pxbuf,pxout,width,height);
    flipY(pxout, width, height);

    clut_swizzle(palette);
    clut_swapcolor(palette);

    uint8 *pixels_rgba;
    pixels_rgba = (uint8 *)calloc(pxlen * 4, sizeof(uint8));
    makeTargaRaw(pxout, pixels_rgba, palette, width, height);

    /* in qtx
     512 bytes = header
     w*h bytes = pixels 8bits/pixels
     128 bytes = ¿?
    1024 bytes = palette
     128 bytes = footer ¿?

    */

    /* in mqt
     256 bytes = pre-header ??
     512 bytes = header
     w*h bytes = pixels 8bits/pixel
     128 bytes = ¿?
    1024 bytes = palette
     128 bytes = footer ¿?

    */

    /* nice data offsets
    0x00 header
    0x30 width
    0x34 heigth
    0x60 name.qtx
    0xa0 name (without extension)
    0xf6 colormode
    0x200 pixels

    */


    /* out
    tga
    18 bytes = header
    w*h*4 bytes = pixels color raw
    */
    
    outfile = fopen(argv[2], "wb");
    if (outfile != NULL)
    {
        fwrite(&tgahdr, 1, sizeof(TGAHDR), outfile); // 1 - cabecera: 18 bytes
        //fwrite(palette, 1, 1024, outfile);
        //fwrite(pxout, 1, width*height, outfile);
        fwrite(pixels_rgba, 1, pxlen*4, outfile); // width * height (raw) 
        fclose(outfile);
        printf("file created: %s\n", argv[2]);
    }
    else
    {
        printf("error creating %s\n", argv[2]);
    }




leave:
    free(readbuf);
    return 0;
}
