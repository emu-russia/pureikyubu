
typedef struct _MIControl
{
	uint8_t* ram;
	size_t ramSize;
} MIControl;

extern	MIControl mi;

void    MIOpen(HWConfig * config);
void	MIClose();
