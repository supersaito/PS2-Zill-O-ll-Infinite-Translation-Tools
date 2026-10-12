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
    int swizzle; // mode=4 then swizzle off -- mode=8 then unknow 
} QTX_HEADER;

const unsigned int HEADER_SIZE = 0x200;
const unsigned int PAD_SIZE = 0x80;

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
    uint8 *readbuf, *pxbuf, *palette, *palettebuf;
    int fsize, width, height, pxlen;
    int colormode;
    uint8 *pxout;
    uint8 *pixels_rgba;
    uint8 shift = 0;

    TGAHDR tgahdr;

    if (argc < 2)
    {
        printf("\nusage:\n   %s <input.qxp> [index]\n", argv[0]);
        return 1;
    }

    infile = fopen(argv[1], "rb");
    if (infile == NULL)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }


    fseek(infile, 0, SEEK_END);
    fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    readbuf = (uint8 *)malloc(fsize);
    fread(readbuf, 1, fsize, infile);
    fclose(infile);

    //set up TGA header
    tgahdr.id_len = 0;
    tgahdr.colortype = 0;
    tgahdr.imgtype = 2;
    tgahdr.palette_firstidx = 0;
    tgahdr.palette_numcolors = 0;
    tgahdr.palette_bpp = 0;
    tgahdr.origin_x = 0;
    tgahdr.origin_y = 0;
    //tgahdr.width = width;
    //tgahdr.height = height;
    tgahdr.img_bpp = 32;
    tgahdr.descriptor = 8;

    // read index   
    uint16 total_files;
    int offset;
    total_files = *(int*)(readbuf);
    printf("Total files inside container: %d\n", total_files);
    
    QTX_HEADER *qtx;
    qtx=calloc(total_files,sizeof(QTX_HEADER));

    for(int i=0; i<total_files; i++) {
        offset = *(uint32*)(readbuf+0x10+i*(sizeof(uint32)));
        width = *(int*)(readbuf + 0x30 + offset); // 32bits, 4 bytes
        height = *(int*)(readbuf + 0x34 + offset); 

        qtx[i].offset=offset;
        qtx[i].width=width;
        qtx[i].height=height;
        strcpy(qtx[i].name, readbuf + 0x60 + offset);
        strcpy(qtx[i].name2, readbuf + 0xA0 + offset);
        qtx[i].colormode=readbuf[offset+0xf6];
        qtx[i].canvas_size=width*height;
        qtx[i].canvas_bytes=*(int*)(readbuf + 0x108 + offset)-128; // magic---
        // lets guess, maybe, who knows...
        if (qtx[i].canvas_bytes==qtx[i].canvas_size/2) {
            qtx[i].mode=4; // 4 bits per pixel
            qtx[i].palette_size=64;
        } else {
            qtx[i].mode=8; // 8 bits per pixel
            qtx[i].palette_size=1024;
        }

        //printf("%d: %d %s %dx%d \n", i, qtx[i].offset, qtx[i].name, qtx[i].width, qtx[i].height ); 

    }   

    int index,first_item;
    if (argc>=3) {
        first_item= atoi(argv[2]); // only one extraction
        total_files=first_item+1;
    } else {
        first_item= 0; // all of then
    }
    
    for (index=first_item;index<total_files;index++){
        /** from here we create the tga file
         * 
         */

        width=qtx[index].width;
        height=qtx[index].height;
        offset=qtx[index].offset;
        pxlen = qtx[index].canvas_size; // canvas
        pxbuf = readbuf + offset + HEADER_SIZE; // pxbuf points to buffer of pixels (pointer)
        palettebuf = readbuf + offset + HEADER_SIZE + qtx[index].canvas_bytes + PAD_SIZE; 

        tgahdr.width = width;
        tgahdr.height = height;

        printf("%d: offset=%d %s %dx%d size=%d palette=%d ", 
            index, qtx[index].offset, qtx[index].name, qtx[index].width, qtx[index].height, 
            qtx[index].canvas_size, offset + HEADER_SIZE + qtx[index].canvas_bytes + PAD_SIZE);  

        pxout = (uint8 *)calloc(pxlen,sizeof(uint8));
        //unSwizzle32to8(pxbuf,pxout,width,height);
        
        if (qtx[index].mode==4) {
            // 1 unpack 4 bits to 8 bits canvas
            // 2 grow palette from 64 to 1024 
            for (int i=0;i<qtx[index].canvas_bytes;i++){
                uint8 b1;
                b1 = *(uint8 *)(pxbuf+i);
                pxout[(i*2)]=   (b1 & 0x0F);
                pxout[(i*2)+1]= (b1 & 0xF0)>>4;
            }

            palette= calloc(1024, sizeof(uint8));
            memcpy(palette, palettebuf, 64);

        } else {
            memcpy(pxout, pxbuf, qtx[index].canvas_bytes);
            palette = palettebuf;
        }

        flipY(pxout, width, height);

        if (qtx[index].mode==8) clut_swizzle(palette);
        clut_swapcolor(palette);
        
        pixels_rgba = (uint8 *)calloc(pxlen * 4, sizeof(uint8));
        makeTargaRaw(pxout, pixels_rgba, palette, width, height);

        char filename[64]; 
        sprintf(filename, "%s.tga", (char*)(readbuf+offset+0xa0));
        printf("-> %s ", filename);

        outfile = fopen(filename, "wb");
        if (outfile != NULL)
        {
            fwrite(&tgahdr, 1, sizeof(TGAHDR), outfile); // 1 - cabecera: 18 bytes
            fwrite(pixels_rgba, 1, pxlen*4, outfile); // width * height (raw) 
            fclose(outfile);
            printf("\n");
        }
        else
        {
            printf(" >> error\n");
        };
        
        free(pxout);
        free(pixels_rgba);
    }; // for index
        

    // cleanup that mess
    free(readbuf);
    exit(0);

    // 2022-A-1
}
