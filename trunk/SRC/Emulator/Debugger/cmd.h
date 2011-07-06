void    con_command(int argc, char argv[][CON_LINELEN], int lnum=0);

void    cmd_blr(int argc, char argv[][CON_LINELEN]);
void    cmd_boot(int argc, char argv[][CON_LINELEN]);
void    cmd_d(int argc, char argv[][CON_LINELEN]);
void    cmd_denop();
void    cmd_disa(int argc, char argv[][CON_LINELEN]);
void    cmd_dop();
void    cmd_dvdopen(int argc, char argv[][CON_LINELEN]);
void    cmd_full(int argc, char argv[][CON_LINELEN]);
void    cmd_help();
void    cmd_log(int argc, char argv[][CON_LINELEN]);
void    cmd_logfile(int argc, char argv[][CON_LINELEN]);
void    cmd_lr(int argc, char argv[][CON_LINELEN]);
void    cmd_name(int argc, char argv[][CON_LINELEN]);
void    cmd_nop();
void    cmd_ostest();
void    cmd_plist();
void    cmd_r(int argc, char argv[][CON_LINELEN]);
void    cmd_savemap(int argc, char argv[][CON_LINELEN]);
void    cmd_script(char *file);
void    cmd_sd(int sd, int argc, char argv[][CON_LINELEN]);
void    cmd_sop(int argc, char argv[][CON_LINELEN]);
void    cmd_stat(int argc, char argv[][CON_LINELEN]);
void    cmd_syms(int argc, char argv[][CON_LINELEN]);
void    cmd_top10();
void    cmd_u(int argc, char argv[][CON_LINELEN]);

// NOP command history
typedef struct NOPHistory
{
    u32     ea;                 // effective address of NOP instruction
    u32     oldValue;           // old value for "denop" command
} NOPHistory;
