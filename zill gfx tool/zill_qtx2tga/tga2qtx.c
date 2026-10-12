#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

// --------------------------------------------------------------------------- un_swizzle functions
static void clut_unswizzle(unsigned char *clut)
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

static void clut_unswapcolor(unsigned char *buf)
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

// --------------------------------------------------------------------------- swizzle functions

static void clut_swizzle(unsigned char *clut)
{
    unsigned int *newclut = (unsigned int*)clut;
    unsigned int origclut[256]; // 4 bytes * 256 = 1024 char
    unsigned int i, j, k;
    memcpy(origclut, clut, 1024); // 1024 bytes (char)
    for (k = 0; k < 8; k++)
    for (j = 0; j < 2; j++)
    for (i = 0; i < 8; i++)
    {
        newclut[k * 32 + 8 * j + i]= origclut[k * 32 + j * 16 + i];
        newclut[k * 32 + 8 * j + 16 + i] = origclut[k * 32 + j * 16 + i + 8];
    }
}

static void clut_swapcolor(unsigned char *buf)
{
    int i;
    unsigned char t,a;

    for (i = 0; i < 256; i++)
    {
        //swap red and blue
        t = buf[2];
        buf[2] = buf[0];
        buf[0] = t;

        //scale down alpha to (0-128)
        a = buf[3] >> 1;
        
        if (a >126 ) { buf[3]=0x80; } else { buf[3] = a;}
       //buf[3] = a;

        buf += 4;
    }
}


static void flipY(unsigned char *buf, int width, int height)
{
    int i, j;
    unsigned char *temp;
    unsigned char *ptemp, *pbuf;

    temp = (unsigned char*)malloc(width * height ); // 128*64 = 8192
    memcpy(temp, buf, width * height );
    ptemp = temp + (width * (height -1)); // end of temp (copy of buf) (height-1 ?)
    pbuf = buf; // begin of buf

    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            pbuf[j] = ptemp[j];
        }
        ptemp -= width; // one step towards the begin
        pbuf += width; // one step towards the end
        //printf("(h=%i,w=%i)", i, j);
    }
    free(temp);
}

///////// swizzlers /////////

void Swizzle8to32(uint8 *pSwizTexels, uint8 *pInTexels, const int32 width, const int32 height)
{

   int x, y;

   for(y=0; y<height; y++)
      for(x=0; x<width; x++)
      {
         const uint8 uPen = pInTexels[y*width+x];

         const int32 block_location = (y&(~0xf))*width + (x&(~0xf))*2;
         const uint32 swap_selector = (((y+2)>>2)&0x1)*4;
         const int32 posY = (((y&(~3))>>1) + (y&1))&0x7;
         const int32 column_location = posY*width*2 + ((x+swap_selector)&0x7)*4;

         const int32 byte_num = ((y>>1)&1) + ((x>>2)&2);     // 0,1,2,3

        pSwizTexels[block_location + column_location + byte_num] = uPen; //swizzle
      }
}

struct RGBA {
    uint8 color1; // blue
    uint8 color2; // green
    uint8 color3; // red
    uint8 alfa;
};

void raw2indexedTarga( uint8 *t_buffer, uint8 *buffer_rgba, uint8 *buffer_palette, int width, int height){
    // t_buffer << packed pixels
    // buffer_rgba = raw image
    // buffer_palette << indexed colors (256)

    int x,y,pos,last,c;
    int idx;

    // lets make the palette
    struct RGBA p[1024]; // our max is 255 but i want to know how much we have to cut
    last=0;
/*
// build palette from buffer
// just in case...
for (c=0;c<256;c++){
    pos=c*4;
    p[c].color1=buffer_palette[pos];
    p[c].color2=buffer_palette[pos+1];
    p[c].color3=buffer_palette[pos+2];
    p[c].alfa=buffer_palette[pos+3];
    printf("%i;%02x;%02x;%02x;%02x\n", c, buffer_palette[pos],buffer_palette[pos+1],buffer_palette[pos+2],buffer_palette[pos+3]);
}
last=256;

// end build palette
*/
    for(y=0; y<height; y++){
        for(x=0; x<width; x++){
            pos = ((y*width) + x)*4;
            idx=-1;
            for(c=0;c<last;c++){
                if ((p[c].color1==buffer_rgba[pos])&&
                   (p[c].color2==buffer_rgba[pos+1])&&
                   (p[c].color3==buffer_rgba[pos+2])&&
                   (p[c].alfa==buffer_rgba[pos+3])) {
                    idx=c; // gotcha
                    break;
                   }     
            };
            if (idx==-1){
                uint8 d=10;
                // search for a neighbour with same alfa
                for(c=0;c<last;c++){
                    if (
                        abs(p[c].color1-buffer_rgba[pos])<d &&
                        abs(p[c].color2-buffer_rgba[pos+1])<d &&
                        abs(p[c].color3-buffer_rgba[pos+2])<d &&
                        abs(p[c].alfa==buffer_rgba[pos+3])) {
                          idx=c; // gotcha
                          break;
                    }     
                };
            }
            
            if (idx==-1){
                // new entry
                p[last].color1=buffer_rgba[pos];
                p[last].color2=buffer_rgba[pos+1];
                p[last].color3=buffer_rgba[pos+2];
                p[last].alfa=buffer_rgba[pos+3];
                idx=last;
                last++;
            };

            t_buffer[(y*width)+x]=idx; // indexed pixel
        }
    }

    if (last>255) {
        printf("Error: there are more than 256 indexed colors. Redo the image please.\n");
        printf("Error: We have %d diferent rgba\n", last);
        exit(0);
    };

    printf("We have %d diferent rgba. Indexing palette...\n", last);

    // lets make the indexed palette
    for(c=0;c<256;c++){
        pos = c*4;
        if (c<last) {
            buffer_palette[pos]=p[c].color1;
            buffer_palette[pos+1]=p[c].color2;
            buffer_palette[pos+2]=p[c].color3;
            buffer_palette[pos+3]=p[c].alfa;
        } else {
            buffer_palette[pos]=0x00;
            buffer_palette[pos+1]=0x00;
            buffer_palette[pos+2]=0x00;
            buffer_palette[pos+3]=0xff;            
        }
    }

}


int main(int argc, char **argv)
{
    FILE *qtxfile;
    unsigned char *qtxreadbuf,*qtxpxbuf,*qtxpalette;
    int qtxfsize, qtxwidth, qtxheight, qtxpxlen;
    int colormode;

    FILE *infile,*outfile;
    unsigned char *tgabuf, *pxbuf;
    int fsize, tgawidth, tgaheight;
    unsigned short *n;

    unsigned char temp[1024];

    printf("version 2\n");

    if (argc != 3)
    {
        printf("specify [tga file] [qtx|mqt file to insert tga into]\n");
        return 1;
    }

    // open original qtx
    //-------------------------------------------------------------------
    qtxfile = fopen(argv[2],"rb"); // read binary
    if (qtxfile == NULL) {
        printf("error opening original qtx|mqt %s\n", argv[3]);
        goto cleanup;
    }
    fseek(qtxfile, 0, SEEK_END);
    qtxfsize = ftell(qtxfile);
    fseek(qtxfile, 0, SEEK_SET);

    qtxreadbuf = (unsigned char*)malloc(qtxfsize);
    fread(qtxreadbuf, 1, qtxfsize, qtxfile); // qtx file
    fclose(qtxfile);
    printf("File read: %i bytes\n", qtxfsize);

    // offset
    char magic[5];
    int offset;
    int i;
    magic[4]=0;
    offset =0;

    for(i=0; i<qtxfsize-4; i++){
        magic[0]=*(char*) (qtxreadbuf+i);
        magic[1]=*(char*) (qtxreadbuf+i+1);
        magic[2]=*(char*) (qtxreadbuf+i+2);
        magic[3]=*(char*) (qtxreadbuf+i+3);

        if (strcmp(magic,"MFZQ")==0) { 
            offset=i; 
            printf("autodetect offset = %d (%d)\n", offset, i);
            break;
        };
    }
    
    printf("i=%d\n", i);

    //properties
    qtxwidth = *(int*)(qtxreadbuf + 0x30 + offset);
    qtxheight = *(int*)(qtxreadbuf + 0x34 + offset);
    printf("original qtx auto-detect dimensions = %dx%d\n", qtxwidth, qtxheight);

    qtxpxlen = qtxwidth * qtxheight;
    qtxpxbuf = qtxreadbuf + offset + 0x200; // pointer to image
    qtxpalette = qtxpxbuf + qtxpxlen + 0x80; // pointer to palette

    //open targa file (raw)
    //------------------------------------------------------------------------
    infile = fopen(argv[1], "rb"); // unswizzled
    if (infile == NULL)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }

    fseek(infile, 0, SEEK_END);
    fsize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    tgabuf = (unsigned char*)malloc(fsize);  // buffer to store the tga file
    fread(tgabuf, 1, fsize, infile); // tga file read
    fclose(infile); // close file

    n = (unsigned short*)tgabuf; // n= pointer to readed tga file
    tgawidth = n[6];
    tgaheight = n[7];

    pxbuf = (unsigned char*)malloc(tgawidth * tgaheight); // buffer of same size of tga canvas in 8bytes (indexed color)
     
    unsigned char* t_buffer;
    t_buffer = (unsigned char *)malloc(tgawidth * tgaheight);

    memcpy(t_buffer, tgabuf + 18 + 1024, tgawidth * tgaheight);    
    
    // read file uncompressed raw without palette
    // and transform to palette (1024bytes) and packed pixels indexed to color (128*64=8192)
    uint8 *buffer_rgba, *buffer_palette;
    buffer_rgba = tgabuf+18; // pointer to raw image
    buffer_palette = (uint8 *)calloc(1024,sizeof(uint8)); // new palette from buffer_rgba
    //buffer_palette = qtxpalette;
    raw2indexedTarga( t_buffer, buffer_rgba, buffer_palette, tgawidth, tgaheight);

    clut_swapcolor(buffer_palette); // swap colors in palette R<->B and Alpha cap to 0x80
    clut_swizzle(buffer_palette); // swizzle palette
    flipY(t_buffer, tgawidth, tgaheight); // flipY pixel buffer
    Swizzle8to32(pxbuf, t_buffer, tgawidth, tgaheight); // swizzle texture pixels

    // make it works
    // copy pixels
    memcpy(qtxreadbuf+offset+0x200,pxbuf,tgawidth * tgaheight);

    // copy pallete
    memcpy(qtxreadbuf+offset+0x200+(tgawidth * tgaheight)+0x80,buffer_palette,0x400);

    //write output file
    //--------------------------------------------------------------------------
    char filename[] = "new_";
    strcat(filename, argv[2]);
    printf("internal name: %s\n",qtxreadbuf+offset+0x60);

    outfile = fopen(filename, "wb"); // write binary
    if (outfile != NULL) {
        //fwrite(qtxreadbuf, 1, 0x200, outfile);     // header  512 bytes ------------------------ COPY
        //fwrite(pxbuf, 1, tgawidth * tgaheight, outfile); //swizzled pixels 128*64 = 8192 bytes
        //fwrite(qtxpxbuf+qtxpxlen, 1, 0x80, outfile); // ?? 128 bytes --------------------------- COPY
        //fwrite(buffer_palette, 1, 0x400, outfile); // palette 1024 bytes
        //fwrite(qtxpxbuf+qtxpxlen+0x80+0x400,1, 0x80, outfile); // ¿? 128 bytes ------------------ COPY
        fwrite(qtxreadbuf,1,qtxfsize,outfile);
        fclose(outfile);
        printf("file created: %s.\n", filename);
        
    }
    else {
        printf("error creating %s\n", argv[2]);
    }

cleanup:
    free(pxbuf);
    free(tgabuf);
    free(qtxreadbuf);

    return 0;
}
