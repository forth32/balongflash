// 
//   Вспомогательные процедуры проекта balong_flash
// 
#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>
#else
#include <windows.h>
#include "getopt.h"
#include "printf.h"
#include "buildno.h"
#endif

#include "hdlcio.h"
#include "ptable.h"
#include "flasher.h"
#include "util.h"



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

//****************************************************
//* Определение версии прошивальщика
//*
//*   0 - нет ответа на команду
//*   1 - версия 2.0
//*  -1 - версия не 2.0 
//****************************************************
int dloadversion() {

int res;  
int i;  
uint8_t replybuf[4096];

res=atcmd("^DLOADVER?",replybuf);
if (res == 0) return 0;
if (strncmp(replybuf+2,"2.0",3) == 0) return 1;
for (i=2;i<res;i++) {
  if (replybuf[i] == 0x0d) replybuf[i]=0;
}  
printf("\n! Неправильная версия монитора прошивки - [%i]%s\n",res,replybuf+2);
// dump(replybuf,res,0);
return -1;
}
  
//****************************************************
//*------- Режим разрезания файла прошивки
//****************************************************
void fwsplit(uint32_t sflag) {
 
uint32_t i;
FILE* out;
uint8_t filename[200];

printf("\n Выделение разделов из файла прошивки:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].hd.psize,ptable[i].pname); 
   // формируем имя файла
   sprintf(filename,"%02i-%08x-%s.%s",i,ptable[i].hd.code,ptable[i].pname,(sflag?"fw":"bin"));
   out=fopen(filename,"wb");
   
   if(sflag) {
     // запись заголовка
     fwrite(&ptable[i].hd,1,sizeof(struct pheader),out);   // фиксированный заголовок
     fwrite((void*)ptable[i].csumblock,1,ptable[i].hd.hdsize-sizeof(struct pheader),out); // блок контрольных сумм
   }
   // запись тела
   fwrite(ptable[i].pimage,ptable[i].hd.psize,1,out);
   fclose(out);
}
printf("\n");
return;
}



//****************************************************
//* Вход в HDLC-режим
//****************************************************
void enter_hdlc() {

uint32_t res;  
unsigned char OKrsp[]={0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};
uint8_t replybuf[100]; 

usleep(100000);

res=atcmd("^DATAMODE",replybuf);
if (res != 6) {
  printf("\n Неправильная длина ответа на ^DATAMODE");
  exit(-2);
}  
if (memcmp(replybuf,OKrsp,6) != 0) {
  printf("\n Команда ^DATAMODE отвергнута, возможно требуется режим цифровой подписи\n");
  exit(-2);
}  
}

//****************************************************
//* Выход из HDLC-режима
//****************************************************
void leave_hdlc() {

uint8_t replybuf[100]; 
unsigned char cmddone[7]={0x1};           // команда выхода из HDLC
  
send_cmd(cmddone,1,replybuf);
}


//****************************************************
//*  Получение версии протокол прошивки
//****************************************************

void protocol_version() {
  
uint8_t replybuf[100]; 
uint32_t iolen,i;  
unsigned char cmdver[7]={0x0c};           // команда запроса версии протокола
  
iolen=send_cmd(cmdver,1,replybuf);
if (iolen == 0) {
  printf("\n Нет ответа от модема в HDLC-режиме\n");
  exit(-2);
}  
if (replybuf[0] == 0x7e) memcpy(replybuf,replybuf+1,iolen-1);
if (replybuf[0] != 0x0d) {
  printf("\n Ошибка получения версии протокола\n");
  exit(-2);
}  
  
i=replybuf[1];
replybuf[2+i]=0;
printf("\n Версия протокола: %s",replybuf+2);
}



//****************************************************
//*  Перезагрузка модема
//****************************************************
void restart_modem() {

unsigned char cmd_reset[7]={0xa};           // команда выхода из HDLC
uint8_t replybuf[100]; 

printf("\n Перезагрузка модема...\n");
send_cmd(cmd_reset,1,replybuf);
atcmd("^RESET",replybuf);
}

//****************************************************
//* Получение идентификатора устройства
//****************************************************
void dev_ident() {
  
uint8_t replybuf[100]; 
uint32_t iolen;
unsigned char cmd_getproduct[30]={0x45};

iolen=send_cmd(cmd_getproduct,1,replybuf);
if (iolen>2) printf("\n Идентификатор устройства: %s",replybuf+2); 
}


//****************************************************
//* Вывод карты файла прошивки
//****************************************************
void show_file_map() {

int i;  
  
printf("\n\n ## Смещение  Размер    Сжатие     Имя\n-----------------------------------------------");
for (i=0;i<npart;i++) { 
     printf("\n %02i %08x %8i  ",i,ptable[i].offset,ptable[i].hd.psize);
     if (ptable[i].zflag == 0) 
       // несжатый раздел
       printf("           ");
     else {
       // сжатый раздел
       switch(ptable[i].ztype) {
	 case 'L':
           printf("Lzma");
	   break;
	 case 'Z':  
           printf("Gzip");
	   break;
       }   
       printf(" %3i%%  ",(ptable[i].hd.psize-ptable[i].zflag)*100/ptable[i].hd.psize);
     }  
     printf(" %s",ptable[i].pname); 
}   
 printf("\n");
 exit(0);
}


//****************************************************
//* Вывод информации о файле прошивки
//****************************************************
void show_fw_info() {

uint8_t* sptr; 
char ver[36];
  
if (ptable[0].hd.version[0] != ':') printf("\n Версия прошивки: %s",ptable[0].hd.version); // нестандартная строка версии
else {
  // стандартная строка версии
  memset(ver,0,sizeof(ver));  
  strncpy(ver,ptable[0].hd.version,32);  
  sptr=strrchr(ver+1,':'); // ищем разделитель-двоеточие
  if (sptr == 0) printf("\n Версия прошивки: %s",ver); // не найдено - несоответствие стандарту
  else {
    *sptr=0;
    printf("\n Версия прошивки: %s",sptr+1);
    printf("\n Платформа:       %s",ver+1);
  }
}  
  
printf("\n Дата сборки:     %s %s",ptable[0].hd.date,ptable[0].hd.time);
printf("\n Заголовок: версия %i, код соответствия: %8.8s",ptable[0].hd.hdversion,ptable[0].hd.unlock);
}




