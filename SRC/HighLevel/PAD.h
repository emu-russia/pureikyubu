// watch for bigendian order!
typedef struct PADStatus
{
    u16     button;
    s8      stickX;
    s8      stickY;
    s8      substickX;
    s8      substickY;
    u8      triggerLeft;
    u8      triggerRight;
    u8      analogA;
    u8      analogB;
    s8      err;
} PADStatus;

void    PADOpenHLE();
void    _PADRead();
