import struct
import zlib
import argparse
import os


# MUNGE chunks (FILESTART/offset, TAMAÑO COMPRIMIDO, EXTENSION, TAMAÑO DESCOMPRIMIDO)

_munge =[]
_ridx_offset=0
_type_offset=0
_exix_offset=0
_rbb0_offset=0
idx=None
operacion="v"
filebase="RES.RBB-original"
filemunge="RES.RBB"

# ------------------------------------------------------------------------------------

def lee_archivo(fichero):
    with open(fichero, 'rb') as fo:
        data = fo.read()
    return(data)

def graba_archivo(fichero, decompressed_data):
    with open(fichero, 'wb') as fz:
        try:
            fz.write(decompressed_data)
        except:
            print('error grabando archivo: ',fichero)
            quit

def m_siff():
    # SIFF
    # 4b signature
    # 4b container size 
    siff = f.read(8) 
    siff_sig = siff[0:4]
    siff_size = struct.unpack("I",siff[4:8])[0] # tamaño del fichero completo
    
    #print( siff_sig, "size:", siff_size)

def m_rsrc():
    # RSRC
    # 4b signature
    # 4b datasize
    # (size_datasize) { datos }
    rsrc = f.read(8)
    rsrc_sig = rsrc[0:4]
    rsrc_size = struct.unpack("I", rsrc[4:8])[0] # tamaño del siguiente bloque
    rsrc_munge = f.read(rsrc_size) # datos desconocidos...
    graba_archivo("_rsrc.munge", rsrc_munge)

    #print( rsrc_sig, rsrc_size) 


def m_ridx():
    global _ridx_offset
    global _munge

    # RIDX
    # 4b signature
    # 4b table size 
    # (table) {
    # 4b file start // start relativo a rbb0data offset
    # 3b tamaño comprimido 
    # 1b size-addr terminator 0x80
    # }
    ridx = f.read(8)
    ridx_sig = ridx[0:4]
    ridx_size = struct.unpack("I", ridx[4:8])[0]
    _ridx_offset=f.tell()

    for d in range(int(ridx_size/8)):
        data = f.read(8)
        inicio = struct.unpack("I",data[0:4])[0]
        longitud = int(struct.unpack("B",data[4:5])[0]) + int(struct.unpack("B",data[5:6])[0]*256) + int(struct.unpack("B",data[6:7])[0]*256*256)
        fin = struct.unpack("B",data[7:8])[0]
        t = (inicio, longitud)
        _munge.append(t)

    #print( ridx_sig, ridx_size)
    #print( munge[0:4]) # TEST DATA
  
def m_type():
    global _munge
    global _type_offset

    # TYPE 
    # 4b signature
    # 4b table size
    # (table) {
    # 3b ascii file extension // excepto 4b "NONE"
    # 1b ascii terminator 0x30
    # }
    extensiones = f.read(8)
    extensiones_sig = extensiones[0:4]
    extensiones_tamaño = struct.unpack("I", extensiones[4:8])[0]
    _type_offset = f.tell()

    for d in range(int(extensiones_tamaño/4)):
        data = f.read(4)
        if data[0:4]=="NONE" :
            _munge[d] += ("NONE",)
        else:
            cadena = (data[0:3]).decode()
            _munge[d] += (cadena,)

    #print( _type_sig, _type_size)
    #print( munge[0:5]) # TEST DATA 

def m_exix():
    global _munge
    global _exix_offset

    # EXIX
    # 4b signature
    # 4b table size
    # table {
    # 4b tamaño descomprimido
    # }
    exix = f.read(8)
    exix_sig = exix[0:4]
    exix_size = struct.unpack("I", exix[4:8])[0]
    _exix_offset = f.tell()

    for d in range(int(exix_size/4)):
        data = f.read(4)
        tamaño = struct.unpack("I",data[0:4])[0]
        _munge[d] += (tamaño,)

    #print( exix_sig, exix_size)
    #print( munge[0:4]) # TEST DATA

def m_gap0():
    # GAP0
    # 4b signature
    # 4b table size
    # table {
    # datos desconocidos ¿hashed filenames?
    # }
    gap0 = f.read(8)
    gap0_sig = gap0[0:4]
    gap0_size = struct.unpack("I", gap0[4:8])[0]
    gap0_data = f.read(gap0_size)
    graba_archivo("_gap0.munge", gap0_data)
    
    #print( gap0_sig, gap0_size)
    #print( gap0_data) # TEST DATA

def m_rbb0():
    global _munge
    global _rbb0_offset

    # RBB0
    # 4b signature
    # 4b table size
    # table {
    # datos en chunks segun hemos obtenido en ridx, comprimido usando zlib ( firma 0x789c)
    # }
    rbb0 = f.read(8)
    rbb0_sig = rbb0[0:4]
    rbb0_size = struct.unpack("I", rbb0[4:8])[0]
    _rbb0_offset = f.tell()

def m_extract(idx=None,folder=""):
    global _munge
    global _rbb0_offset

    offset=0
    tamaño=0

    f = open(filebase,"rb")

    if idx!=None : 
        from_=idx
        to_ =idx+1
        if to_>len(_munge):
            to_=len(_munge)
    else:
        from_=0
        to_=len(_munge)
    
    for d in range(from_,to_):
        este_munge = _munge[d]
        offset = este_munge[0]
        tamaño = este_munge[1]
        extension = este_munge[2]

        f.seek(offset+_rbb0_offset)

        print("generando fichero {0} de {1} (tam={2} - ext={3}); offset={4}".format(d, len(_munge), tamaño , extension, offset))
        if tamaño==0:
            print('Error, tamaño 0: munge=',d)
            continue
        else:        
            fichero = "{0}munge_{1}.{2}".format(folder, str(d).zfill(6), extension)
            data = f.read(tamaño)

            try:
                decompressed_data = zlib.decompress(data)
            except:
                print(f'Error: al descomprimir zlib: {fichero}')
                quit()
            graba_archivo(fichero, decompressed_data)
    f.close

def m_insert(idx=None,folder=""):
    global filemunge

    if idx==None:
        return
    offset = idx*8 + 4
    #f.seek(_ridx_offset+offset)

    inicio = _munge[idx][0]
    if idx<len(_munge):
        inicio_siguiente=_munge[idx+1][0]
    
    tam_comprimido=_munge[idx][1]
    extension = _munge[idx][2]
    tam_descomprimido=_munge[idx][2]
    

    fichero = folder + 'munge_' + str(idx).zfill(6) + '.' + extension

    data_descomprimido = lee_archivo(fichero)
    tamaño_descomprimido = len(data_descomprimido)
    data_comprimido = zlib.compress(data_descomprimido)
    tamaño_comprimido = len(data_comprimido)

    if (inicio+tamaño_comprimido)>inicio_siguiente:
        print("Error: El tamaño comprimido es mayor que el espacio disponible para el chunk ", idx)
        print("Espacio disponible: ", inicio_siguiente-inicio)
        print("Espacio requerido: ", tamaño_comprimido)
        quit()

    #grabar tamaño comprimido en ridx (offset + 3 (BBB) + 1 (0x80))
    with open(filemunge,"r+b") as fw:
        fw.seek(_ridx_offset + (idx*8) + 4) # me salto los primeros 4B
        buffer=tamaño_comprimido.to_bytes(3,"little")
        fw.write(buffer)
        #fw.flush()

        #grabar tamaño descomprimido en EXIX
        fw.seek(_exix_offset + (idx*4) )
        buffer =tamaño_descomprimido.to_bytes(4,"little")
        fw.write(buffer)
        #fw.flush()

        #grabar zeros en rbb0
        fw.seek(_rbb0_offset + inicio)
        zeroes = int(_munge[idx][1])
        fw.write( b'\0' * zeroes)
        #fw.flush()
        
        #grabar datos en rbb0
        fw.seek(_rbb0_offset + inicio)
        buffer = data_comprimido
        fw.write(buffer)
        #fw.flush()

# ----------------------------------------------------------------------------
parser = argparse.ArgumentParser()
parser.add_argument("-v", "--verbose", help="Mostrar información de chunk",action="store_true")
parser.add_argument("-f", "--filebase", help="Nombre del munge RES.RBB a extraer ")
parser.add_argument("-m", "--filemunge", help="Nombre del munge RES.RBB a modificar")
parser.add_argument("-x", "--extract", help="Extraer mungenito en carpeta 'mungenitos'",action="store_true")
parser.add_argument("-i", "--insert", help="Insertar mungenito de carpeta 'mungenitos-new'",action="store_true")
parser.add_argument('integers', metavar='N', type=int, nargs='+', help='indice para extraer/insertar/ver (-1 para todos)')
args = parser.parse_args()

if args.verbose:
    operacion="v"

if args.filebase:
    filebase = args.filebase

if args.filemunge:
    filebase = args.filemunge

if args.extract:
    operacion="x"

if args.insert:
    operacion="i"

if args.extract and args.insert:
    print("Error: no puedes elegir insertar y extraer al mismo tiempo.")
    quit()

with open(filebase, "rb") as f:
    m_siff()
    m_rsrc()
    m_ridx()
    m_type()
    m_exix()
    m_gap0()
    m_rbb0()


if args.integers[0]==-1:
    if operacion=="x":
        print("Extrayendo todos.")    
        m_extract(None,"mungenitos/")
        print("Hecho")
        quit()

    if operacion=="i":
        dir_path="./mungenitos-new/"

        for path in os.listdir(dir_path):
            if os.path.isfile(os.path.join(dir_path, path)):
                from_=path.index("_")+1
                to_= path.index(".")
                idx_=int(path[from_:to_])
                print("insertando: ", path, " en indice: ", idx_)
                m_insert(idx_,"mungenitos-new/")
        print("Hecho.")
        quit()

    if operacion=="v":
        print("Combinacion de parametros '-v -1' no soportado.")
        quit()
else:
    for x in args.integers:
        if operacion=="x":
            print("Extrayendo: ",x)
            m_extract(x,"mungenitos/") # extrae el fichero del bloque
            
        if operacion=="i":
            print("Insertando: ",x)
            m_insert(x,"mungenitos-new/") # inserta un fichero

        if operacion=="v":
            print("Indice:               ",x)
            print("Offset de inicio:     ", _munge[x][0])
            print("Tamaño comprimido:    ", _munge[x][1])
            print("Extensión:            ", _munge[x][2])
            print("Tamaño descomprimido: ", _munge[x][3])
            
# 2022-10-23   