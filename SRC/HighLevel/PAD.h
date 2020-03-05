// watch for bigendian order!
typedef struct PADStatus
{
    uint16_t    button;
    int8_t      stickX;
    int8_t      stickY;
    int8_t      substickX;
    int8_t      substickY;
    uint8_t     triggerLeft;
    uint8_t     triggerRight;
    uint8_t     analogA;
    uint8_t     analogB;
    int8_t      err;
} PADStatus;

void    PADOpenHLE();
void    _PADRead();
