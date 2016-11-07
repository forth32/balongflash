#include <stdint.h>
void dump(char buffer[],int len,long base);
unsigned short crc16(char* buf, int len);
int dloadversion();
void fwsplit(uint32_t sflag);
void enter_hdlc();
void protocol_version();
void leave_hdlc();
void restart_modem();
void dev_ident();
void show_file_map();
void show_fw_info();

