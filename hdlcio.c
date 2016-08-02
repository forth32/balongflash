#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include "hdlcio.h"

unsigned int nand_cmd=0x1b400000;
unsigned int spp=0;
unsigned int pagesize=0;
unsigned int sectorsize=512;
unsigned int maxblock=0;     // Общее число блоков флешки
char flash_mfr[30]={0};
char flash_descr[30]={0};
unsigned int oobsize=0;


struct termios sioparm;
int siofd; // fd для работы с Последовательным портом


//***********************
//* Дамп области памяти *
//***********************

void dump(char buffer[],int len,long base) {
unsigned int i,j;
char ch;

for (i=0;i<len;i+=16) {
  printf("%08lx: ",(unsigned long)(base+i));
  for (j=0;j<16;j++){
   if ((i+j) < len) printf("%02x ",buffer[i+j]&0xff);
   else printf("   ");
  }
  printf(" *");
  for (j=0;j<16;j++) {
   if ((i+j) < len) {
    ch=buffer[i+j];
    if ((ch < 0x20)|(ch > 0x80)) ch='.';
    putchar(ch);
   }
   else putchar(' ');
  }
  printf("*\n");
}
}


//*************************************************
//*  Вычисление CRC-16 
//*************************************************
unsigned short crc16(char* buf, int len) {

unsigned short crctab[] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,   // 0
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,   // 8
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,   //16
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,   //24
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,   //32
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,   //40
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,   //48
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,   //56
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,   //64
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};
int i;
unsigned short crc=0xffff;  

for(i=0;i<len;i++)  crc=crctab[(buf[i]^crc)&0xff]^((crc>>8)&0xff);  
return (~crc)&0xffff;
}


//*************************************************
//*    отсылка буфера в модем
//*************************************************
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


tcflush(siofd,TCIOFLUSH);  // сбрасываем недочитанный буфер ввода
write(siofd,"\x7e",1);  // отсылаем префикс

if (write(siofd,outcmdbuf,outlen) == 0) {   printf("\n Ошибка записи команды");return 0;  }
tcdrain(siofd);  // ждем окончания вывода блока
return 1;
}

//******************************************************************************************
//* Прием буфера с ответом из модема
//*
//*  masslen - число байтов, принимаемых единым блоком без анализа признака конца 7F
//******************************************************************************************

unsigned int receive_reply(char* iobuf, int masslen) {
  
int i,iolen,escflag,bcnt,incount;
unsigned char c;
unsigned int res;
unsigned char replybuf[14000];

incount=0;
if (read(siofd,&c,1) != 1) {
//  printf("\n Нет ответа от модема");
  return 0; // модем не ответил или ответил неправильно
}
//if (c != 0x7e) {
//  printf("\n Первый байт ответа - не 7e: %02x",c);
//  return 0; // модем не ответил или ответил неправильно
//}
replybuf[incount++]=c;

// чтение массива данных единым блоком при обработке команды 03
if (masslen != 0) {
 res=read(siofd,replybuf+1,masslen-1);
 if (res != (masslen-1)) {
   printf("\nСлишком короткий ответ от модема: %i байт, ожидалось %i байт\n",res+1,masslen);
   dump(replybuf,res+1,0);
   return 0;
 }  
 incount+=masslen-1; // у нас в буфере уже есть masslen байт
// printf("\n ------ it mass --------");
// dump(replybuf,incount,0);
}

// принимаем оставшийся хвост буфера
while (read(siofd,&c,1) == 1)  {
 replybuf[incount++]=c;
// printf("\n-- %02x",c);
 if (c == 0x7e) break;
}

// Преобразование принятого буфера для удаления ESC-знаков
escflag=0;
iolen=0;
for (i=0;i<incount;i++) { 
  c=replybuf[i];
  if ((c == 0x7e)&&(iolen != 0)) {
    iobuf[iolen++]=0x7e;
    break;
  }  
  if (c == 0x7d) {
    escflag=1;
    continue;
  }
  if (escflag == 1) { 
    c|=0x20;
    escflag=0;
  }  
  iobuf[iolen++]=c;
}  
return iolen;

}

//***********************************************************
//* Преобразование командного буфера с Escape-подстановкой
//***********************************************************
unsigned int convert_cmdbuf(char* incmdbuf, int blen, char* outcmdbuf) {

int i,iolen,escflag,bcnt,incount;
unsigned char cmdbuf[14096];

bcnt=blen;
memcpy(cmdbuf,incmdbuf,blen);
// Вписываем CRC в конец буфера
*((unsigned short*)(cmdbuf+bcnt))=crc16(cmdbuf,bcnt);
bcnt+=2;

// Пребразование данных с экранированием ESC-последовательностей
iolen=0;
outcmdbuf[iolen++]=cmdbuf[0];  // первый байт копируем без модификаций
for(i=1;i<bcnt;i++) {
   switch (cmdbuf[i]) {
     case 0x7e:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5e;
       break;
      
     case 0x7d:
       outcmdbuf[iolen++]=0x7d;
       outcmdbuf[iolen++]=0x5d;
       break;
      
     default:
       outcmdbuf[iolen++]=cmdbuf[i];
   }
 }
outcmdbuf[iolen++]=0x7e; // завершающий байт
outcmdbuf[iolen]=0;
return iolen;
}



//***************************************************
//*  Отсылка команды в порт и получение результата  *
//***************************************************
int send_cmd(unsigned char* incmdbuf, int blen, unsigned char* iobuf) {
  
unsigned char outcmdbuf[14096];
unsigned int  iolen;

iolen=convert_cmdbuf(incmdbuf,blen,outcmdbuf);  
if (!send_unframed_buf(outcmdbuf,iolen)) return 0; // ошибка передачи команды
return receive_reply(iobuf,0);
}


//*************************************
// Настройка Последовательного порта
//*************************************

int open_port(char* devname)
{
siofd = open(devname, O_RDWR | O_NOCTTY |O_SYNC);
if (siofd == -1) return 0;

bzero(&sioparm, sizeof(sioparm)); // готовим блок атрибутов termios
sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // INPCK;
sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=30; // timeout  
sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);
return 1;
}

//*************************************
// Настройка Последовательного порта
//*************************************

void port_timeout(int timeout) {
bzero(&sioparm, sizeof(sioparm)); // готовим блок атрибутов termios
sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // INPCK;
sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=timeout; // timeout  
sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);
}

//*************************************************
//*  Поиск файла по номеру в указанном каталоге
//*
//* num - # файла
//* filename - буфер для полного имени файла
//* id - переменная, в которую будет записан идентификатор раздела
//*
//* return 0 - не найдено
//*        1 - найдено
//*************************************************
int find_file(int num, char* dirname, char* filename,unsigned int* id, unsigned int* size) {

DIR* fdir;
FILE* in;
unsigned int pt;
struct dirent* dentry;
char fpattern[5];

sprintf(fpattern,"%02i",num); // образец для поиска файла по 3 цифрам номера
fdir=opendir(dirname);
if (fdir == 0) {
  printf("\n Каталог %s не открывается\n",dirname);
  exit(1);
}

// главный цикл - поиск нужного нам файла
while ((dentry=readdir(fdir)) != 0) {
  if (dentry->d_type != DT_REG) continue; // пропускаем все кроме регулярных файлов
  if (strncmp(dentry->d_name,fpattern,2) == 0) break; // нашли нужный файл. Точнее, файл с нужными 3 цифрами в начале имени.
}

closedir(fdir);

// формируем полное имя файла в буфере результата
if (dentry == 0) return 0; // не нашли
strcpy(filename,dirname);
strcat(filename,"/");
// копируем имя файла в буфер результата
strcat(filename,dentry->d_name);  

// 00-00000200-M3Boot.bin
//проверяем имя файла на наличие знаков '-'
if ((dentry->d_name[2] != '-') || (dentry->d_name[11] != '-')) {
  printf("\n Неправильный формат имени файла - %s\n",dentry->d_name);
  exit(1);
}

// проверяем цифровое поле ID раздела
if (strspn(dentry->d_name+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf("\n Ошибка в идентификаторе раздела - нецифрвой знак - %s\n",filename);
  exit(1);
}  
sscanf(dentry->d_name+3,"%8x",id);

// Проверяем доступность и читаемость файла
in=fopen(filename,"r");
if (in == 0) {
  printf("\n Ошибка открытия файла %s\n",filename);
  exit(1);
}
if (fread(&pt,1,4,in) != 4) {
  printf("\n Ошибка чтения файла %s\n",filename);
  exit(1);
}
  
// проверяем, что файл - сырой образ, без заголовка
if (pt == 0xa55aaa55) {
  printf("\n Файл %s имеет заголовок - для прошивки не подходит\n",filename);
  exit(1);
}


// Что еще можно проверить? Пока не придумал.  

//  Получаем размер файла
fseek(in,0,SEEK_END);
*size=ftell(in);
fclose(in);

return 1;
}
