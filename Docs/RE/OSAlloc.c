// Dolphin OSAlloc

typedef struct _Cell
{
    struct _Cell * prev;
    struct _Cell * next;
    int size;
    HeapDesc * hd;
    u32 requested;      // Size of original unaligned data 
} Cell;

typedef struct HeapDesc
{
    u32         size;
    Cell *      free;
    Cell *      allocated;
    u32         paddingBytes;
    u32         headerBytes;
    u32         payloadBytes;
} HeapDesc;

HeapDesc * HeapArray;
int NumHeaps;
void * ArenaStart;
void * ArenaEnd;
volatile OSHeapHandle __OSCurrHeap;

//
// Double-linked Lists
//

Cell * DLAddFront ( Cell * list, Cell * cell )
/*++

    Add new cell to the head. Return new list address.

--*/
{
    cell->next = list;
    cell->prev = NULL;
    if ( list ) list->prev = cell;
    return cell;
}

Cell * DLLookup ( Cell * list, Cell * cell)
/*++

    Check whenever cell is owned by the list.

--*/
{
    while ( list )
    {
        if ( list == cell ) return list;
        list = list->next;
    }
    return NULL;
}

Cell * DLExtract ( Cell * list, Cell * cell )
/*++

    Delete cell, return new list address.

--*/
{
    if ( cell->next )
        cell->next->prev = cell->prev;
    if ( cell->prev )
    {
        cell->prev->next = cell->next;
        return list;
    }
    else return cell->next;
}

Cell * DLInsert (Cell * list, Cell * cell )
{
    Cell * prev, next;

    next = list;
    prev = NULL;

    while ( next && (next < cell) )
    {
        prev = next;
        next = next->next;
    }

    cell->next = next;
    cell->prev = prev;

    if ( next )
    {
        next->prev = cell;

        if ( (cell + cell->size) == next )
        {
            cell->size += next->size;
            cell->next = next->next;
            if ( cell->next ) cell->next->prev = cell;
        }
    }

    if ( prev )
    {
        prev->next = cell;

        if ( (prev + prev->size) == cell )
        {
            prev->size += cell->size;
            prev->next = next;
            if ( next ) next->prev = prev;
        }

        return list;
    }
    
    return cell;
}

BOOL DLOverlap ( Cell * list, void * start, void * end )
/*++

    Check if list lies within specified range, or at least touch it by any cell.

--*/
{
    Cell * cell;

    cell = list;

    while ( cell )
    {
        if ( start <= cell && cell < end ) return TRUE;

        if ( start < (cell + cell->size) && (cell + cell->size) <= end ) return TRUE;

        cell = cell->next;
    }

    return FALSE;
}

int DLSize (Cell * list)
/*++

    Get list size (sum of all cell sizes)

--*/
{
    int size = 0;

    cell = list;
    while ( cell )
    {
        size += cell->size;
        cell = cell->next;
    }

    return size;
}

void * OSInitAlloc ( void * arenaStart,
                     void * arenaEnd,
                     int maxHeaps )
{
    HeapDesc * Heap;
    int n;

    ASSERTMSG ( maxHeaps > 0, "OSInitAlloc(): invalid number of heaps." );

    ASSERTMSG ( arenaStart < arenaEnd, "OSInitAlloc(): invalid range." );

    ASSERTMSG ( ((arenaEnd - arenaStart) / sizeof(HeapDesc) ) >= maxHeaps, "OSInitAlloc(): too small range." );
 
    HeapArray = arenaStart;

    NumHeaps = maxHeaps;

    //
    // Initialize heap descriptors
    //

    for (n=0; n<NumHeaps; n++)
    {
        Heap = &HeapDesc[n];

        Heap->size = -1;
        Heap->allocated = NULL;
        Heap->free = NULL;
        Heap->payloadBytes = 0;
        Heap->headerBytes = 0;
        Heap->paddingBytes = 0;
    }

    __OSCurrHeap = -1;

    ArenaStart = OSRoundUp32B ( &HeapArray[maxHeaps] );
    ArenaEnd = OSRoundDown32B ( arenaEnd );

    ASSERTMSG ( (ArenaEnd - ArenaStart) >= 0x40, "OSInitAlloc(): too small range." );

    return ArenaStart;
}

void * OSAllocFromHeap ( OSHeapHandle heap, u32 size)
{
    HeapDesc * hd;
    Cell * cell;
    Cell * newCell;
    u32 leftoverSize;
    u32 requested;
    const int CellSizePadded = OSRoundUp32B( sizeof(Cell) );

    ASSERTMSG ( HeapArray, "OSAllocFromHeap(): heap is not initialized." );

    ASSERTMSG ( size > 0, "OSAllocFromHeap(): invalid size." );

    ASSERTMSG ( heap >= 0 && heap < NumHeaps, "OSAllocFromHeap(): invalid heap handle." );

    ASSERTMSG ( HeapArray[heap].size >= 0, "OSAllocFromHeap(): invalid heap handle." );

    //
    // Find free space in free list.
    //

    hd = &HeapArray[heap];

    requested = OSRoundUp32B (size);

    cell = hd->free;

    while ( cell )
    {
        if ( cell->size >= requested ) break;
        cell = cell->next;
    }

    if ( cell == NULL )
    {
        OSReport ( "OSAllocFromHeap: Warning- failed to allocate %d bytes\n", requested );
        return NULL;
    }

    ASSERTMSG ( (cell & (CellSizePadded-1)) == 0, "OSAllocFromHeap(): heap is broken." );

    ASSERTMSG ( cell->hd == NULL, "OSAllocFromHeap(): heap is broken." );

    leftoverSize = cell->size - requested;

    //
    // Allocate new cell.
    //

    if ( leftoverSize >= 2 * CellSizePadded )
    {
        cell->size = requested;
        
        newCell = cell + requested;
        newCell->size = leftoverSize;
        newCell->hd = NULL;
        newCell->prev = cell->prev;
        newCell->next = cell->next;

        if ( newCell->next ) newCell->next->prev = newCell;

        if ( newCell->prev == NULL )
        {
            if ( hd->free != cell )
                OSHalt ( "OSAllocFromHeap(): heap is broken." );

            hd->free = newCell;
        }
        else newCell->prev->next = newCell;
    }
    else hd->free = DLExtract ( hd->free, cell );

    //
    // Add allocated cell in allocated list.
    //

    hd->allocated = DLAddFront ( hd->allocated, cell );

    cell->hd = hd;
    cell->requested = size;

    hd->headerBytes += CellSizePadded;
    hd->paddingBytes += cell->size - (size + CellSizePadded );
    hd->payloadBytes += size;

    return cell + CellSizePadded;
}

void OSFreeToHeap ( OSHeapHandle heap, void * ptr )
{
    HeapDesc * hd;
    Cell * cell;
    const int CellSizePadded = OSRoundUp32B( sizeof(Cell) );

    ASSERTMSG ( HeapArray, "OSFreeToHeap(): heap is not initialized." );

    ASSERTMSG ( ptr >= (ArenaStart + CellSizePadded) && ptr < ArenaEnd , "OSFreeToHeap(): invalid pointer." );

    ASSERTMSG ( (ptr & (CellSizePadded-1)) == 0, "OSFreeToHeap(): invalid pointer." );

    hd = &HeapArray[heap];

    ASSERTMSG ( hd->size >= 0, "OSFreeToHeap(): invalid heap handle." );

    cell = ptr - CellSizePadded;

    ASSERTMSG ( cell->hd == hd, "OSFreeToHeap(): invalid pointer." );

    ASSERTMSG ( DLLookup(hd->allocated, cell), "OSFreeToHeap(): invalid pointer." );

    cell->hd = NULL;

    hd->headerBytes -= CellSizePadded;
    hd->paddingBytes -= (cell->size - (cell->requested + CellSizePadded));
    hd->payloadBytes -= cell->requested;

    hd->allocated = DLExtract ( hd->allocated, cell );
    hd->free = DLInsert ( hd->free, cell );
}

void OSAddToHeap (OSHeapHandle heap, void *start, void *end )
{
    HeapDesc *hd;
    Cell *cell;
    int i;

    ASSERTMSG ( HeapArray, "OSAddToHeap(): heap is not initialized." );

    ASSERTMSG ( heap >= 0 && heap < NumHeaps, "OSAddToHeap(): invalid heap handle." );

    ASSERTMSG ( HeapArray[heap].size >= 0, "OSAllocFromHeap(): invalid heap handle." );

    hd = &HeapArray[heap];

    ASSERTMSG ( start < end, "OSAddToHeap(): invalid range." );

    start = OSRoundUp32B (start);
    end = OSRoundDown32B (end);

    ASSERTMSG ( (end - start) >= 0x40, "OSAddToHeap(): too small range." );

    ASSERTMSG ( start >= ArenaStart && end <= ArenaEnd, "OSAddToHeap(): invalid range." );

    //
    // Check if area crossing already existing heap.
    //

    for (i=0; i<NumHeaps; i++)
    {
        if ( HeapArray[i].size >= 0 )
        {
            if ( DLOverlap ( HeapArray[i].free, start, end) )
            {
                OSHalt ( "OSAddToHeap(): invalid range." );
            }

            if ( DLOverlap ( HeapArray[i].allocated, start, end) )
            {
                OSHalt ( "OSAddToHeap(): invalid range." );
            }            
        }
    }

    //
    // Add large single cell.
    //

    cell = (Cell *)start;

    cell->size = end - start;
    cell->hd = NULL;

    hd->size += cell->size;
    hd->free = DLInsert ( hd->free, cell );
}
