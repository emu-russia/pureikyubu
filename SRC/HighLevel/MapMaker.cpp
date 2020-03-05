// MAP maker utility
#include "dolphin.h"

typedef struct opMarker {
    uint32_t offset;
    BOOL blr;
} opMarker;

typedef struct funcDesc {
    uint32_t checksum;
    uint32_t nameoffset;
} funcDesc;

FILE *Map;

opMarker * Map_marks;
int Map_marksSize, Map_marksMaxSize;

uint8_t * Map_buffer;
int Map_functionsSize;
funcDesc * Map_functions;
char * Map_functionsNamesTable;

#define MAPDAT_FILE   ".\\Data\\makemap.dat"
#define MAP_MAXFUNCNAME 100

/*
 * Allocates more space for the markers array
 */
static void MAPGrow (int num)
{
    Map_marksMaxSize += num;
    Map_marks = (opMarker *)realloc(Map_marks, Map_marksMaxSize * sizeof(opMarker));
}

/*
 * Calculates a custom Checksum for a range of opcodes
 */
static uint32_t MAPFuncChecksum (uint32_t offsetStart, uint32_t offsetEnd)
{
    uint32_t sum = 0, offset;
    uint32_t opcode, auxop, op, op2, op3;

    for (offset = offsetStart; offset <= offsetEnd; offset+=4) {
        opcode = MEMSwap(*((uint32_t *)&RAM[offset & RAMMASK]));
        op = opcode & 0xFC000000; 
        op2 = 0;
        op3 = 0;
        auxop = op >> 26;
        switch (auxop) {
        case 4:
            op2 = opcode & 0x0000003F;
            switch ( op2 ) {
            case 0:
            case 8:
            case 16:
            case 21:
            case 22:
                op3 = opcode & 0x000007C0;
            }
            break;
        case 19:
        case 31: 
        case 63: 
            op2 = opcode & 0x000007FF;
            break;
        case 59:
            op2 = opcode & 0x0000003F;
            if ( op2 < 16 ) 
                op3 = opcode & 0x000007C0;
            break;
        }
        // Checksum only uses opcode, not opcode data, because opcode data changes 
        // in all compilations, but opcodes dont!
        sum = ( ( (sum << 17 ) & 0xFFFE0000 ) | ( (sum >> 15) & 0x0001FFFF ) );
        sum = sum ^ (op | op2 | op3);
    }
    return sum;
}

/*
 * Open the file with the information about common recognized functions
 */
static void MAPOpen ()
{
    uint32_t size = 0;
    Map_buffer = (uint8_t *)FileLoad(MAPDAT_FILE, &size);
    if (Map_buffer == NULL) return;
    Map_functionsSize = *(uint32_t *)(Map_buffer);
    Map_functions = (funcDesc *)(Map_buffer + sizeof(uint32_t));
    Map_functionsNamesTable = (char *)((char *)Map_functions 
                                        + Map_functionsSize * sizeof(funcDesc));

    /*
    // This just prints a list of all the common functions
    FILE *f = fopen(MAPDAT_FILE ".txt", "w");
    fprintf(f, "[%08x]\n",Map_functionsSize);
    for (int i = 0; i < Map_functionsSize; i++)
        fprintf(f, "[%08x][%s]\n",Map_functions[i].checksum,
        &Map_functionsNamesTable[Map_functions[i].nameoffset] );
    fclose ( f ) ;
    */
}

static void MAPClose ()
{
    if (Map_buffer == NULL) return;

    free(Map_buffer);
    Map_buffer = NULL;
    Map_functionsSize = 0;
    Map_functions = NULL;
    Map_functionsNamesTable = NULL;
}

static char * MAPFind (uint32_t checksum)
{
    int inf, med, sup;
    if (Map_buffer == NULL) return NULL;

    inf = 0; 
    sup = Map_functionsSize - 1;
    while (inf <= sup) {
        med = (inf + sup) / 2;
        if (Map_functions[med].checksum == checksum)
            return &Map_functionsNamesTable[Map_functions[med].nameoffset];
        if (checksum < Map_functions[med].checksum)
            sup = med - 1;
        else
            inf = med + 1;
    }
    return NULL;
}
/*
 * Starts the creation of a new map
 */
void MAPInit(char * mapname)
{
    MAPOpen ();
    Map = fopen(mapname, "w");

    Map_marksMaxSize = 500;
    Map_marksSize = 0;
    Map_marks = (opMarker *)malloc(Map_marksMaxSize * sizeof(opMarker));

    SetStatusText(STATUS_PROGRESS, "Please, wait until emulator making new MAP");
    Sleep(1000);
}

/*
 * Adds a mark to the opcode at the specified offset.
 * if blr is FALSE, the mark is considerated an entrypoint to a function
 * if blr is not FALSE, the mark is considerated an exitpoint from the function
 * Use carefully!!!
 */
void MAPAddMark (uint32_t offset, BOOL blr)
{
    int inf, med, sup;

    if (!Map) return ;
    if (Map_marksSize == Map_marksMaxSize) MAPGrow ( 500 ) ;

    inf = 0; 
    sup = Map_marksSize - 1;
    while (inf <= sup) {
        med = (inf + sup) / 2;
        if (Map_marks[med].offset == offset) return;
        if (offset < Map_marks[med].offset)
            sup = med - 1;
        else
            inf = med + 1;
    }

    for (sup = Map_marksSize; inf < sup; sup--)
        Map_marks[sup] = Map_marks[sup - 1];
    Map_marks[inf].offset = offset;
    Map_marks[inf].blr = blr;
    Map_marksSize++;

}

/*
 * Checks the specified range, and automatically adds marks to entry and exit points to functions.
 */
void MAPAddRange (uint32_t offsetStart, uint32_t offsetEnd)
{
    uint32_t opcode;
    uint32_t target;
    uint32_t op, op2;

    if (!Map) return ;
    if (!Map_marks) return ;

    MAPAddMark (offsetStart, FALSE);
    while(offsetStart < offsetEnd) {

        opcode = MEMSwap(*((uint32_t *)&RAM[offsetStart & RAMMASK]));
        op = opcode >> 26, op2 = 0;

        switch (op) {
        case 18: //bl and bla
            switch(opcode & 3) {
            case 1:
            case 3:
                target = opcode & 0x03fffffc;
                if(target & 0x02000000) target |= 0xfc000000;
                if ((opcode & 3) == 1) target += offsetStart;
                MAPAddMark (target, FALSE);
                break;
            }
            break;
        case 19: //OP2
            op2 = opcode & 0x7ff;
            switch(op2) {
            case 32:
            case 33:
            case 100:
                MAPAddMark (offsetStart, TRUE);
            }
            break;
        }
        offsetStart += 4;
    }
}

/*
 * Finishes the creation of the current map
 */
void MAPFinish()
{
    int i, k;
    uint32_t Checksum;
    char * name, namebuf[MAP_MAXFUNCNAME];
    int namelen;

    if (!Map) return ;

    memset ( namebuf, 0, MAP_MAXFUNCNAME );

    i = 0;
    while (i < Map_marksSize - 1) {
        // find start of function
        while (Map_marks[i].blr && i < Map_marksSize) i++; 
        while (i < Map_marksSize - 1 && Map_marks[i+1].blr == FALSE) i++;
        // find end of function
        for ( k = i + 1; k < Map_marksSize - 1 && Map_marks[k+1].blr; k++);
        
        if (i < Map_marksSize && k < Map_marksSize &&
            Map_marks[i].blr == FALSE && Map_marks[k].blr) {

            // look if the function is HLE
            Checksum = MAPFuncChecksum (Map_marks[i].offset , Map_marks[k].offset);
            name = MAPFind (Checksum) ;

            if (name != NULL) {
                char buf[16], *nameptr;
                nameptr = strchr(name, ',');
                if (nameptr != NULL) {
                    // if the function name contains at least a comma, it means that 
                    // it shares the same checksum with another function.
                    // Nothing else can be done to identify this function
                    name = buf;
                    sprintf (buf, "[0x%08x]", Checksum);
                }

                // show status
                SetStatusText(STATUS_PROGRESS, name);
                Sleep(10);

                namelen = strlen(name);
                if (namelen >= MAP_MAXFUNCNAME) {
                    memcpy (namebuf, name, MAP_MAXFUNCNAME - 1);
                    fprintf(Map, "%08x %s\n", Map_marks[i].offset, namebuf);
                }
                else 
                    fprintf(Map, "%08x %s\n", Map_marks[i].offset, name);
            }
        }
        i = k + 1;
    }

    fclose(Map);
    free(Map_marks);
    Map_marks = NULL;

    MAPClose();
}
