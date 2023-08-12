// symbolic information API.
#include "pch.h"

using namespace Debug;

// IMPORTANT : EXE loading base must be 0x00400000, for correct HLE.
// MSVC : Project/Settings/Link/Output/Base Address
// CW : Edit/** Win32 x86 Settings/Linker/x86 COFF/Base address

// all important variables are here
SYMControl sym;                 // default workspace
static SYMControl *work = &sym; // current workspace (in use)

// ---------------------------------------------------------------------------

// find first occurency of symbol in list
static SYM * symfind(const char *symName)
{
	for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
	{
		if (!strcmp(it->second->savedName, symName))
		{
			return it->second;
		}
	}
	return nullptr;
}

void SYMSetWorkspace(SYMControl *useIt)
{
	work = useIt;
}

void SYMCompareWorkspaces (
	SYMControl      *source,
	SYMControl      *dest,
	void (*DiffCallback)(uint32_t ea, char * name)
)
{
	for (auto sourceId = source->symmap.begin(); sourceId != source->symmap.end(); ++sourceId)
	{
		bool found = false;

		for (auto destIt = dest->symmap.begin(); destIt != dest->symmap.end(); ++destIt)
		{
			if (!strcmp(sourceId->second->savedName, destIt->second->savedName))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			DiffCallback(sourceId->second->eaddr, sourceId->second->savedName);
		}
	}
}

// get address of symbolic label
// if label is not specified, return 0
uint32_t SYMAddress(const char *symName)
{
	// try to find specified symbol
	SYM *symbol = symfind(symName);

	if(symbol) return symbol->eaddr;
	else return 0;
}

// get symbolic label by given address
// if label is not specified, return NULL
char * SYMName(uint32_t symAddr)
{
	auto it = work->symmap.find(symAddr);

	if (it == work->symmap.end())
	{
		return nullptr;
	}

	return it->second->savedName;
}

// Get the symbol closest to the specified address and offset relative to the start of the symbol.
char* SYMGetNearestName(uint32_t address, size_t& offset)
{
	int minDelta = INT_MAX;
	SYM* nearestSymbol = nullptr;

	offset = 0;

	for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
	{
		if (address >= it->first)
		{
			int delta = address - it->first;
			if (delta < minDelta)
			{
				minDelta = delta;
				nearestSymbol = it->second;
			}
		}
	}

	if (nearestSymbol == nullptr)
		return nullptr;

	offset = address - nearestSymbol->eaddr;
	return nearestSymbol->savedName;
}

// associate high-level call with symbol
// (if CPU reaches label, it jumps to HLE call)
void SYMSetHighlevel(const char *symName, void (*routine)())
{
	// TODO: Disabled for now (redo)
	return;

	// try to find specified symbol
	SYM *symbol = symfind(symName);

	// check address
	// High-level call is too high in memory.
	if (((uint64_t)routine & ~0x03ffffff) != 0)
	{
		throw "High-level call is too high in memory!";
	}

	// leave, if symbol is not found. add otherwise.
	if(symbol)
	{
		symbol->routine = routine;      // overwrite

		// if first opcode is 'BLR', then just leave it
		uint32_t op;
		Core->ReadWord(symbol->eaddr, &op);
		if(op != 0x4e800020)
		{
			Core->WriteWord(
				symbol->eaddr,          // add patch
				(uint32_t)((uint64_t)routine & 0x03ffffff)  // 000: high-level opcode
			);
			if(!_stricmp(symName, "OSLoadContext"))
			{
				Core->WriteWord(
					symbol->eaddr + 4,  // return to caller
					0x4c000064          // rfi
				);
			}
			else
			{
				Core->WriteWord(
					symbol->eaddr + 4,  // return to caller
					0x4e800020          // blr
				);
			}
		}
		Report(Channel::HLE, "patched API call: %08X %s\n", symbol->eaddr, symName);
	}
}

// save string in memory
static char * strsave(const char *str)
{
	size_t len = strlen(str) + 1;
	char *saved = new char[len];
	assert(saved);
	strcpy(saved, str);
	return saved;
}

// add new symbol
void SYMAddNew(uint32_t addr, const char *name)
{
	SYM* symbol = symfind(name);

	if (symbol != nullptr)
	{
		// Replace name
		if (symbol->savedName)
		{
			delete[] symbol->savedName;
			symbol->savedName = nullptr;
		}
		symbol->savedName = strsave(name);
	}
	else
	{
		// Add new
		SYM* sym = new SYM;

		sym->eaddr = addr;
		sym->savedName = strsave(name);
		sym->routine = nullptr;

		work->symmap[addr] = sym;
	}
}

// Remove all symbols
void SYMKill()
{
	for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
	{
		if (it->second->savedName)
		{
			delete[] it->second->savedName;
		}
		delete it->second;
	}

	work->symmap.clear();
}

// list symbols, matching first occurence of "str".
// * - all symbols (warning! Zelda has about 20000 symbols).
void SYMList(const char *str)
{
	size_t len = strlen(str), cnt = 0;
	Report(Channel::Norm, "<address> symbol\n\n");

	for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
	{
		SYM* symbol = it->second;
		if (((*str == '*') || !_strnicmp(str, symbol->savedName, len)))
		{
			Report(Channel::Norm, "<%08X> %s\n", symbol->eaddr, symbol->savedName);
			cnt++;
		}
	}

	Report(Channel::Norm, "%i match\n\n", cnt);
}



// MAP files loader. currently there are support for three MAP file formats: 
// Custom ("RAW"), CodeWarrior and GCC-like.

// load CodeWarrior-generated map file
// thanks Dolphin team for idea
static MAP_FORMAT LoadMapCW(const wchar_t *mapname)
{
	bool    started = false;
	char    buf[1024], token1[256];
	FILE    *map;
	
	// symbol information
	uint32_t  moduleOffset, procSize, procAddr;
	int     flags;
	char    procName[512];

	map = fopen ( Util::WstringToString(mapname).c_str(), "r");
	if(!map) return MAP_FORMAT::BAD;

	while(!feof(map))
	{
		fgets(buf, 1024, map);
		sscanf(buf, "%s", token1);

		// check section type (we need only code sections)
		if(!strcmp(buf, ".init section layout\n")) { started = true; continue; }
		if(!strcmp(buf, ".text section layout\n")) { started = true; continue; }
		if(!strcmp(buf, ".data section layout\n")) break;

		// check first token
		#define IFIS(str) if(!strcmp(token1, #str)) continue;
		IFIS(Starting);
		IFIS(address);
		IFIS(-----------------------);
		IFIS(UNUSED);

		if(token1[strlen(token1) - 1] == ']') continue;
		if(started == false) continue;

		// parse symbols
		if(sscanf(buf, "%08x %08x %08x %i %s", 
			&moduleOffset, &procSize, &procAddr,
			&flags,
			procName) != 5) continue;

		if(flags != 1)
		{
			SYMAddNew(procAddr, procName);
		}
	}

	fclose(map);

	Report(Channel::HLE, "CodeWarrior format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
	return MAP_FORMAT::CW;
}

// load GCC-generated map file
static MAP_FORMAT LoadMapGCC(const wchar_t *mapname)
{
	bool    started = false;
	char    buf[1024];
	FILE    *map;
	
	// symbol information
	uint32_t     procAddr;
	char    par1[512];
	char    par2[512];

	map = fopen ( Util::WstringToString(mapname).c_str(), "r");
	if(!map) return MAP_FORMAT::BAD;

	while(!feof(map))
	{
		fgets(buf, 1024, map);

		// parse symbols
		if(sscanf(buf, "%s %s", par1, par2) != 2) continue;

		if(strcmp(par1, ".init") == 0) { started = true; continue; }
		if(strcmp(par1, ".text") == 0) { started = true; continue; }
		if(par1[0] == '.')  { started = false; continue; }

		if(started)
		{
			if (par1[0] == '0' && par1[1] == 'x') {
				sscanf(&par1[2], "%08x", &procAddr);
				SYMAddNew(procAddr, par2);
			}
		}
	}

	fclose(map);

	Report(Channel::HLE, "GCC format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
	return MAP_FORMAT::GCC;
}

// load raw format map-file
static MAP_FORMAT LoadMapRAW(const wchar_t *mapname)
{
	int i;
	size_t size = Util::FileSize(mapname);
	if (!size)
		return MAP_FORMAT::BAD;

	std::vector<uint8_t> mapbuf = Util::FileLoad(mapname);

	// remove all garbage, like tabs
	for (i = 0; i < size; i++)
	{
		if (mapbuf[i] < ' ') mapbuf[i] = '\n';
	}

	uint8_t* ptr = mapbuf.data();
	while (*ptr)
	{
		// some maps has really *huge* symbols
		char line[0x1000]{};
		line[i = 0] = 0;

		// cut string
		while (*ptr == '\n') ptr++;
		if (!*ptr) break;
		while (*ptr != '\n') line[i++] = *ptr++;
		line[i++] = 0;

		// remove comments
		char* p = line;
		while (*p)
		{
			if (p[0] == '/' && p[1] == '/')
			{
				*p = 0;
				break;
			}
			p++;
		}

		// remove spaces at the end
		p = &line[strlen(line) - 1];
		while (*p <= ' ') p--;
		if (*p) p[1] = 0;

		// remove spaces at the beginning
		p = line;
		while (*p <= ' ' && *p) p++;

		// empty string ?
		if (!*p) continue;

		// add symbol
		char* name;
		uint32_t addr = strtoul(p, &name, 16);
		while (*name <= ' ') name++;
		SYMAddNew(addr, name);
	}

	Report(Channel::HLE, "RAW format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
	return MAP_FORMAT::RAW;
}

// wrapper for all map formats.
MAP_FORMAT LoadMAP(const wchar_t *mapname, bool add)
{
	FILE *f;
	char sign[256];

	// delete previous MAP symbols?
	if(!add)
	{
		SYMKill();
	}

	// copy name for MAP saver (with SaveMAP "this" parameter)
	wcscpy(hle.mapfile, mapname);

	// try to open
	f = fopen ( Util::WstringToString(mapname).c_str(), "r");
	if(!f)
	{
		Report(Channel::HLE, "Cannot %s MAP: %s\n", (add) ? "add" : "load", Util::WstringToString(mapname).c_str());
		hle.mapfile[0] = 0;
		return MAP_FORMAT::BAD;
	}

	// recognize map format
	fread(sign, 1, 256, f);
	fclose(f);

	MAP_FORMAT format;
	if(!strncmp(sign, "Link map", 8)) format = LoadMapCW(mapname);
	else if(!strncmp(sign, "Archive member", 14)) format = LoadMapGCC(mapname);
	else format = LoadMapRAW(mapname);

	if(format == MAP_FORMAT::BAD)
	{
		hle.mapfile[0] = 0;
	}
	return format;
}

MAP_FORMAT LoadMAP(const char* mapname, bool add)
{
	wchar_t wcharStr[0x1000] = { 0, };
	wchar_t* wcharPtr = wcharStr;
	char* charPtr = (char*)mapname;

	while (*charPtr)
	{
		*wcharPtr++ = *charPtr++;
	}
	*wcharPtr++ = 0;

	return LoadMAP(wcharStr, add);
}



// MAP maker utility

typedef struct opMarker {
	uint32_t offset;
	bool blr;
} opMarker;

typedef struct funcDesc {
	uint32_t checksum;
	uint32_t nameoffset;
} funcDesc;

FILE *Map;

opMarker * Map_marks;
int Map_marksSize, Map_marksMaxSize;

std::vector<uint8_t> temp;
uint8_t * Map_buffer;
int Map_functionsSize;
funcDesc * Map_functions;
char * Map_functionsNamesTable;

#define MAPDAT_FILE   "./Data/makemap.dat"
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
		opcode = _BYTESWAP_UINT32(*((uint32_t *)&mi.ram[offset & RAMMASK]));
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
	temp = Util::FileLoad((std::string)MAPDAT_FILE);
	
	Map_buffer = temp.data();
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
void MAPInit(const wchar_t * mapname)
{
	MAPOpen ();
	Map = fopen( Util::WstringToString(mapname).c_str(), "w");

	Map_marksMaxSize = 500;
	Map_marksSize = 0;
	Map_marks = (opMarker *)malloc(Map_marksMaxSize * sizeof(opMarker));
}

/*
 * Adds a mark to the opcode at the specified offset.
 * if blr is false, the mark is considerated an entrypoint to a function
 * if blr is not false, the mark is considerated an exitpoint from the function
 * Use carefully!!!
 */
void MAPAddMark (uint32_t offset, bool blr)
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

	MAPAddMark (offsetStart, false);
	while(offsetStart < offsetEnd) {

		opcode = _BYTESWAP_UINT32(*((uint32_t *)&mi.ram[offsetStart & RAMMASK]));
		op = opcode >> 26, op2 = 0;

		switch (op) {
			case 18: //bl and bla
				switch(opcode & 3) {
				case 1:
				case 3:
					target = opcode & 0x03fffffc;
					if(target & 0x02000000) target |= 0xfc000000;
					if ((opcode & 3) == 1) target += offsetStart;
					MAPAddMark(target, false);
					break;
				}
				break;
			case 19: //OP2
				op2 = opcode & 0x7ff;
				switch(op2) {
				case 32:
				case 33:
				case 100:
					MAPAddMark (offsetStart, true);
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
	size_t namelen;

	if (!Map) return ;

	memset ( namebuf, 0, MAP_MAXFUNCNAME );

	i = 0;
	while (i < Map_marksSize - 1) {
		// find start of function
		while (Map_marks[i].blr && i < Map_marksSize) i++; 
		while (i < Map_marksSize - 1 && Map_marks[i+1].blr == false) i++;
		// find end of function
		for ( k = i + 1; k < Map_marksSize - 1 && Map_marks[k+1].blr; k++);
		
		if (i < Map_marksSize && k < Map_marksSize &&
			Map_marks[i].blr == false && Map_marks[k].blr) {

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
				{
					wchar_t wideName[0x100] = { 0, };

					wchar_t* wideNamePtr = wideName;
					char* namePtr = name;
					while (*namePtr)
					{
						*wideNamePtr++ = *namePtr++;
					}
					*wideNamePtr++ = 0;
				}

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



// MAP saver is moved to stand-alone module, because MAP saving operation
// is not easy, like you may think. We should watch for MAP file formats
// and try to append symbols into alredy present MAP.


#define DEFAULT_MAP L"Data\\default.map"
//#define HEX "0x"
#define HEX

static MAP_FORMAT mapFormat;
static char *mapName;
static FILE* mapFile = NULL;
static bool appendStarted;
static int itemsUpdated;

static void AppendMAPBySymbol(uint32_t address, char *symbol)
{
	mapFile = fopen(mapName, "a");
	if(!mapFile) return;

	// linefeed
	if(!appendStarted)
	{
		appendStarted = 1;
		fprintf(mapFile, "\n");
	}

	if(mapFormat == MAP_FORMAT::RAW)
	{
		//80002300 Symbol
		// * or * (dont care)
		//0x80002300 Symbol
		fprintf(mapFile, HEX "%08X %s\n", address, symbol);
	}
	else if(mapFormat == MAP_FORMAT::CW)
	{
		//00000000 000000f0 80003100 0 __start
		// ignore size (set to 4)
		fprintf(mapFile, "00000000 00000004 %08X 0 %s\n", address, symbol);
	}
	else if(mapFormat == MAP_FORMAT::GCC)
	{
		//0x8000ab00 Symbol
		// its not clear for me, because its hotquik's stuff :o)
		fprintf(mapFile, "%08x %s\n", address, symbol);        
	}

	Report(Channel::HLE, "New map entry: %08X %s\n", address, symbol);
	itemsUpdated ++;
	fclose (mapFile);
}

// save whole symbolic information in map.
// there can be two cases of this call : save map into specified file and update current map
// if there is not map loaded, all new symbols will go in default.map
// saved map is appended (mean no file overwrite, and add new symbols to the end)
static void SaveMAP2(const wchar_t *mapname)
{
	static SYMControl temp;     // STATIC !
	SYMControl *thisSet = &sym, *mapSet = &temp;

	if(!mapname)
	{
		if(hle.mapfile[0] == 0)
		{
			Report(Channel::Error, "No map file loaded! Symbols will be saved to default map\n");
			mapname = DEFAULT_MAP;
		}
		else mapname = hle.mapfile;
	}

	Report(Channel::HLE, "Saving/updating map: %s ...\n\n", mapname);

	// load MAP symbols
	SYMSetWorkspace(mapSet);
	mapFormat = LoadMAP(mapname);
	if(mapFormat == MAP_FORMAT::BAD) return;  // :(

	// find new map entries to append file
	mapName = (char *)mapname;
	appendStarted = 0;
	itemsUpdated = 0;
	SYMCompareWorkspaces(thisSet, mapSet, AppendMAPBySymbol);
	if ( itemsUpdated == 0 ) Report (Channel::HLE, "Nothing to update\n");

	// restore old workspace
	SYMKill();
	SYMSetWorkspace(thisSet);
}

void SaveMAP(const char* mapname)
{
	if (!mapname)
	{
		SaveMAP2(nullptr);
		return;
	}

	wchar_t wcharStr[0x1000] = { 0, };
	char* ansiPtr = (char*)mapname;
	wchar_t* wcharPtr = wcharStr;
	while (*ansiPtr)
	{
		*wcharPtr++ = *ansiPtr++;
	}
	*wcharPtr++ = 0;
	SaveMAP2(wcharStr);
}
