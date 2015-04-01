extern int siofd; // fd для работы с Последовательным портом

void dump(char buffer[],int len,long base);
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int open_port(char* devname);
