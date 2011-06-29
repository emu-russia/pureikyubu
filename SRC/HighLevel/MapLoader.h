enum MAP_FORMAT
{
    MAP_FORMAT_BAD = 0,

    MAP_FORMAT_RAW,             // MAP format, invented by org
    MAP_FORMAT_CW,              // CodeWarrior
    MAP_FORMAT_GCC,             // GCC
};

// 0: cannot load map
// >0: map loaded (MAP_FORMAT returned)
int LoadMAP(char *mapname, BOOL add=FALSE);
