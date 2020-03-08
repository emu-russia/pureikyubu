/*
 * Starts the creation of a new map
 */
void MAPInit(char * mapname);

/*
 * Adds a mark to the opcode at the specified offset.
 * if blr is FALSE, the mark is considerated an entrypoint to a function
 * if blr is not FALSE, the mark is considerated an exitpoint from the function
 * Use carefully!!!
 */
void MAPAddMark (uint32_t offset, bool blr) ;

/*
 * Checks the specified range, and automatically adds marks to entry and exit points to functions.
 */
void MAPAddRange (uint32_t offsetStart, uint32_t offsetEnd) ;

/*
 * Finishes the creation of the current map
 */
void MAPFinish() ;
