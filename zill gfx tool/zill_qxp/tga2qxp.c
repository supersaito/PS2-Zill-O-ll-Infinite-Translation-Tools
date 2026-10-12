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
    char bulkname[64]; // without extension
    int colormode;
    int canvas_size; 
    int canvas_bytes; // from header, may be different from canvas
    int palette_size; // 1024 for 8bit or 64 for 4bit
    int mode; // 8 or 4
    int swizzle; // mode=4 then swizzle off -- mode=8 palette swizzle, pixels unknow 
} QTX_HEADER;

const unsigned int HEADER_SIZE = 0x200;
const unsigned int PAD_SIZE = 0x80;

struct RGBA {
    uint8 color1; // blue
    uint8 color2; // green
    uint8 color3; // red
    uint8 alfa;
};

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



void raw2indexedTarga( uint8 *t_buffer, uint8 *buffer_rgba, uint8 *buffer_palette, int width, int height, int maxcolors){
    // t_buffer << packed pixels
    // buffer_rgba = raw image
    // buffer_palette << indexed colors (256|64)

    int x,y,pos,last,c;
    int idx;

    // lets make the palette
    struct RGBA p[1024]; // our max is 256 or 64 but i want to know how much we have to cut
    last=0;

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
            //printf("%2X",idx);
        }
        //printf("\n");
    }

    if (last>maxcolors) {
        printf("Error: We have %d diferent rgba\n", last);
        printf("Error: Our max is %d indexed colors. Redo the image please.\n", maxcolors);
        
        exit(0);
    };

    printf("We have %d diferent rgba, which is ok. Indexing palette...\n", last);

    // lets make the indexed palette
    for(c=0;c<maxcolors;c++){
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
    FILE *base_file;
    unsigned char *zill_buffer,*qtxpxbuf,*qtxpalette;
    int base_file_size, qtxwidth, qtxheight, qtxpxlen;
    int colormode;

    FILE *targa_file,*outfile;
    unsigned char *tgabuf, *pxbuf;
    int fsize, tgawidth, tgaheight;
    unsigned short *n;

    unsigned char temp[1024];

    printf("version 3 (qxp)\n");

    if (argc < 3)
    {
        printf("specify [tga file] [qxp zill file] [1 = overwite qxp]\n");
        return 1;
    }

    // open original zill oll file
    //-------------------------------------------------------------------
    base_file = fopen(argv[2],"rb"); // read binary
    if (base_file == NULL) {
        printf("error opening original file %s\n", argv[3]);
        goto cleanup;
    }
    fseek(base_file, 0, SEEK_END);
    base_file_size = ftell(base_file);
    fseek(base_file, 0, SEEK_SET);

    zill_buffer = (unsigned char*)malloc(base_file_size);
    fread(zill_buffer, 1, base_file_size, base_file); // zill oll file
    fclose(base_file);
    printf("File '%s' read: %i bytes\n", argv[2], base_file_size);

    // build index 
    // ****************************************************************
    int index;
    int offset;  
    int width,height;
    char *name, *bulkname; 
    uint16 total_files;
    QTX_HEADER *qtx;

    total_files = *(int*)(zill_buffer);
    printf("Total files inside container: %d\n", total_files);
        
    qtx=calloc(total_files,sizeof(QTX_HEADER));

    for(int i=0; i<total_files; i++) {
        offset = *(uint32*)(zill_buffer+0x10+i*(sizeof(uint32)));

        width = *(int*)(zill_buffer + 0x30 + offset); // 32bits, 4 bytes
        height = *(int*)(zill_buffer + 0x34 + offset); 
        name = zill_buffer + 0x60 + offset;
        bulkname= zill_buffer + 0xA0 + offset;

        qtx[i].offset=offset;
        qtx[i].width=width;
        qtx[i].height=height;
        strcpy(qtx[i].name, name);
        strcpy(qtx[i].bulkname, bulkname);
        qtx[i].colormode=zill_buffer[offset+0xf6];
        qtx[i].canvas_size=qtx[i].width*qtx[i].height;
        qtx[i].canvas_bytes=*(int*)(zill_buffer + 0x108 + offset)-128;
        // lets guess
        if (qtx[i].canvas_bytes==qtx[i].canvas_size/2) {
            qtx[i].mode=4; // 4 bits per pixel
            qtx[i].palette_size=64;
        } else {
            qtx[i].mode=8; // 8 bits per pixel
            qtx[i].palette_size=1024;
        }

        //printf("%d: %d %s %dx%d \n", i, qtx[i].offset, qtx[i].name, qtx[i].width, qtx[i].height ); 

    }   

    // search for the index of the input file
    // rip off extension
    strcpy(bulkname, argv[1]); // name of the targa
    for(int i=0; i<strlen(bulkname); i++){
        if (bulkname[i]=='.'){
            bulkname[i]='\0';
            break;
        }
    }

    for(int i=0; i<total_files; i++){
        if (strcmp(bulkname,qtx[i].bulkname)==0){
            index=i;
            printf("%d: %d %s %dx%d \n", i, qtx[i].offset, qtx[i].name, qtx[i].width, qtx[i].height ); 
            break;
        }
    }

    // open targa file (raw)
    //------------------------------------------------------------------------

    int maxcolors;
    maxcolors=qtx[index].palette_size /4 ; // 1024 = 256 colors; 64 = 10 colors;

    targa_file = fopen(argv[1], "rb"); // unswizzled
    if (targa_file == NULL)
    {
        printf("error opening %s\n", argv[1]);
        return 1;
    }

    fseek(targa_file, 0, SEEK_END);
    fsize = ftell(targa_file);
    fseek(targa_file, 0, SEEK_SET);

    tgabuf = (unsigned char*)malloc(fsize);  // buffer to store the tga file
    fread(tgabuf, 1, fsize, targa_file); // tga file read
    fclose(targa_file); // close file

    n = (unsigned short*)tgabuf; // n= pointer to readed tga file
    tgawidth = n[6];
    tgaheight = n[7];

    pxbuf = (unsigned char*)malloc(tgawidth * tgaheight); // buffer of same size of tga canvas in 8bytes (indexed color)
     
    unsigned char* t_buffer;
    t_buffer = (unsigned char *)malloc(tgawidth * tgaheight);

    memcpy(t_buffer, tgabuf + 18 + 1024, tgawidth * tgaheight);    
    
    // read file uncompressed raw without palette
    uint8 *buffer_rgba, *buffer_palette;
    buffer_rgba = tgabuf+18; // pointer to raw image
    buffer_palette = (uint8 *)calloc(1024,sizeof(uint8)); // new palette from buffer_rgba

    raw2indexedTarga( t_buffer, buffer_rgba, buffer_palette, tgawidth, tgaheight, maxcolors);

    clut_swapcolor(buffer_palette); // swap colors in palette R<->B and Alpha cap to 0x80
    if (qtx[index].mode==8) clut_swizzle(buffer_palette); // swizzle palette when palette is 1024 bytes
    
    flipY(t_buffer, tgawidth, tgaheight); // flipY pixel buffer
    //Swizzle8to32(pxbuf, t_buffer, tgawidth, tgaheight); // not swizzle this time
    
    if (qtx[index].mode==4){
        // 4 bits mode, pack pixels and 64 bits of palette
        // shrink palette
        // not need =  the process to index colors must be enougth to pack the colors in the first 64 bytes

        // shrink pixels
        // every pixel must be indexed in less than 0x0F so we pack every two into one byte
        uint8 *mini_buffer;
        mini_buffer = calloc(qtx[index].canvas_bytes, sizeof(uint8));

        for (int i=0; i<qtx[index].canvas_size;){
            uint8 byte1,byte2,result;
            byte1 = *(uint8 *)(t_buffer+i);
            byte2 = *(uint8 *)(t_buffer+i+1);
            result = (byte2<<4)|(byte1);
            mini_buffer[i/2] = result;
            i+=2;
            //printf("%02x %02x %02x \n", byte1, byte2, result);
        }

        memcpy(zill_buffer + qtx[index].offset + HEADER_SIZE, mini_buffer, qtx[index].canvas_bytes);

        free(mini_buffer);

    } else {
        // 8 bits mode, pixels indexed and 1024 bytes of palette

        // copy pixels
        memcpy(zill_buffer + qtx[index].offset + HEADER_SIZE, t_buffer, qtx[index].canvas_bytes);

    }

    // copy palette
    memcpy(zill_buffer + qtx[index].offset + HEADER_SIZE + qtx[index].canvas_bytes + PAD_SIZE, buffer_palette, qtx[index].palette_size);


    //write output file
    //--------------------------------------------------------------------------
    char filename[] = "new_";
    strcat(filename, argv[2]); 

    if ((argc==4) && (atoi(argv[3])==1)) {
        printf("Overwriting %s\n", argv[2]);
        outfile = fopen(argv[2], "wb"); // write binary
    } else {
        printf("Creating new file %s\n.", filename);
        outfile = fopen(filename, "wb"); // write binary
    }
    if (outfile != NULL) { 
        fwrite(zill_buffer,1,base_file_size,outfile);
        fclose(outfile);
        printf("file saved\n");
        
    }
    else {
        printf("error saving file\n");
    }

cleanup:
    free(pxbuf);
    free(tgabuf);
    free(zill_buffer);

    return 0;
}
