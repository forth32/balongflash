extern int siofd; // fd для работы с Последовательным портом

int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf);
int open_port(char* devname);
int find_file(int num, char* dirname, char* filename,unsigned int* id, unsigned int* size);
void port_timeout(int timeout);
int atcmd(char* cmd, char* rbuf);

#ifdef WIN32
#define usleep(x) Sleep(x/1000)
#endif
