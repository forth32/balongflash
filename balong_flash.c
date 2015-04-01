#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "hdlcio.h"

//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc, char* argv[]) {

struct termios sioparm;
unsigned int i,j,res,opt,npart=0,iolen,part,blk,blksize;
FILE* in;
FILE* out;

struct {
  unsigned int offset;
  unsigned int size;
  unsigned int code;
}ptable[100];

unsigned char buf[4096];
unsigned char devname[50]="/dev/ttyUSB0";
unsigned char replybuf[4096];
unsigned char datamodecmd[]="AT^DATAMODE";
unsigned char OKrsp[]={0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};
unsigned int  dpattern=0xa55aaa55;
unsigned int  mflag=0,eflag=0;
unsigned char filename [100];

unsigned char cmdver[7]={0x0c};           // версия протокола
unsigned char cmddone[7]={0x1};           // команда выхода из HDLC
unsigned char cmd_dload_init[15]={0x41};  // команда начала пакета
unsigned char cmd_data_packet[11000]={0x42};  // команда начала пакета
unsigned char cmd_dload_end[30]={0x43};
// Коды типов разделов
struct {
  char name[20];
  int code;
} pcodes[]={ 
  {"M3Boot",0x20000}, 
  {"Ptable",0x10000},
  {"Fastboot",0x110000},
  {"Kernel",0x30000},
  {"VxWorks",0x40000},
  {"M3Image",0x50000},
  {"DSP",0x60000},
  {"NVRAM",0x70000},
  {"System",0x590000},
  {"APP",0x5a0000}, 
  {"CD_WUI_VER",0xa0000},
  {"CDROMISO",0xb0000},
  {"WEBUI",0x5b0000},
  {0,0}
};

while ((opt = getopt(argc, argv, "hp:me")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n Утилита предназначена для аварийной USB-загрузки модемов E3372S\n\n\
%s [ключи] <имя файла для загрузки>\n\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком (по умолчанию /dev/ttyUSB0\n\
-m       - вывести карту файла прошивки и завершить работу\n\
-e       - разобрать файл прошивки на разделы и завершить работу\n\
\n",argv[0]);
    return;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'm':
     mflag=1;
     break;
     
   case 'e':
     eflag=1;
     break;

   case '?':
   case ':':  
     return;
  }
}  

if (optind>=argc) {
    printf("\n - Не указано имя файла для загрузки\n");
    return;
}  

in=fopen(argv[optind],"r");
if (in == 0) {
  printf("\n Ошибка открытия %s",argv[optind]);
  return;
}

// Поиск разделов внутри файла
printf("\n Разбираем файл прошвки...");
while (fread(&i,1,4,in) == 4) {
  if (i != dpattern) continue; // ищем разделитель
  ptable[npart].offset=ftell(in);  // позиция начала раздела
  fread(buf,1,96,in); // заголовок
  ptable[npart].offset+=*((unsigned int*)&buf[0])-4; // выкидываем из смещения длину заголовка+КС 
  ptable[npart].size=*((unsigned int*)&buf[20]); // размер раздела
  ptable[npart].code=*((unsigned int*)&buf[16]); // тип раздела
  npart++; 
}
if (npart == 0) {
  printf("\nРазделы не найдены!");
  return ;
}  
printf(" найдено %i разделов",npart);
//------ Режим вывода карты файла прошивки

if (mflag) {
 printf("\n Таблица разделов, найденных в файле:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
 for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  ",i,ptable[i].offset,ptable[i].size); 
   for(j=0;pcodes[j].code != 0;j++) {
     if(pcodes[j].code == ptable[i].code) break;
   }
   if (pcodes[j].code != 0) printf("%s",pcodes[j].name);
   else printf("Неопределенный раздел - %08x",ptable[i].code);
 }
printf("\n");
return;
}


//------- Режим разрезания файла прошивки
if (eflag) {
 printf("\n Выделение разделов из файла прошивки:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
 for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  ",i,ptable[i].offset,ptable[i].size); 
   for(j=0;pcodes[j].code != 0;j++) {
     if(pcodes[j].code == ptable[i].code) break;
   }
   if (pcodes[j].code != 0) {
     printf("%s",pcodes[j].name);
     sprintf(filename,"%02i-%s",i,pcodes[j].name);
   } 
   else {
     printf("Неопределенный раздел - %08x",ptable[i].code);
     sprintf(filename,"%02i-U%08x",i,ptable[i].code);
   }
   out=fopen(filename,"w");
   fseek(in,ptable[i].offset,SEEK_SET); // встаем на начало
   for(j=0;j<ptable[i].size;j+=4) {
     fread(buf,4,1,in);
     fwrite(buf,4,1,out);
   }
   fclose(out);
 }
printf("\n");
return;
}

// Настройка SIO

if (!open_port(devname))  {
#ifndef WIN32
   printf("\n - Последовательный порт %s не открывается\n", devname); 
#else
   printf("\n - Последовательный порт COM%s не открывается\n", devname); 
#endif
   return; 
}


tcsetattr(siofd, TCSANOW, &sioparm);
tcflush(siofd,TCIOFLUSH);  // очистка выходного буфера
port_timeout(50);

// Входим в HDLC-режим
printf("\n Входим в режим HDLC...");
write(siofd,datamodecmd,strlen(datamodecmd));
res=read(siofd,replybuf,6);
if (res != 6) {
  printf("\n Неправильный ответ на DATAMODE\n");
  dump(replybuf,res,0);
  return;
}  
if (memcmp(replybuf,OKrsp,6) != 0) {
  printf("\n Неправильный ответ на DATAMODE\n");
  dump(replybuf,res,0);
  return;
}  

printf("ok");
iolen=send_cmd(cmdver,1,replybuf);
if ((iolen == 0)||(replybuf[1] != 0x0d)) {
  printf("\nОшибка команды GET_VERSION\n");
  if (iolen != 0) dump(replybuf,iolen,0);
  return;
}  
i=replybuf[2];
replybuf[3+i]=0;
printf("\n Версия протокола: %s",replybuf+3);
printf("\n");



// Главный цикл записи разделов
printf("\n Начало записи разделов");
for(part=0;part<npart;part++) {
  printf("\n Записываем раздел %i -",part);
  // выводим имя раздела
  for(j=0;pcodes[j].code != 0;j++) {
     if(pcodes[j].code == ptable[part].code) break;
   }
   if (pcodes[j].code != 0) {
     printf("%s\n",pcodes[j].name);
   } 
   else {
     printf("код %08x\n",ptable[npart].code);
   }
  
  // заполняем командный пакет
  *((unsigned int*)&cmd_dload_init[1])=htonl(ptable[part].code);  
  *((unsigned int*)&cmd_dload_init[5])=htonl(ptable[part].size);  
  // отсылаем команду
  iolen=send_cmd(cmd_dload_init,12,replybuf);
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Заголовок раздела не принят, код ошибки = %02x\n",replybuf[3]);
    dump(cmd_dload_init,13,0);
    return;
  }  

  
  // Поблочный цикл
  blksize=4096;
  fseek(in,ptable[part].offset,SEEK_SET);
  for(blk=0;blk<((ptable[part].size+4095)/4096);blk++) {
    printf("\r Блок %i из %i",blk,(ptable[part].size+4095)/4096);
    res=ptable[part].size+ptable[part].offset-ftell(in);  // размер оставшегося куска до конца файла
    if (res<4096) blksize=res;  // корректируем размер последнего блока
    *(unsigned int*)&cmd_data_packet[1]=htonl(blk+1);  // # пакета
    *(unsigned short*)&cmd_data_packet[5]=htons(blksize);  // размер блока
    fread(cmd_data_packet+7,1,blksize,in); // читаем очередной кусок раздела в буфер команды
    iolen=send_cmd(cmd_data_packet,blksize+7,replybuf); // отсылаем команду
    if ((iolen == 0) || (replybuf[1] != 2)) {
      printf("\n Блок %i раздела не принят, код ошибки = %02x\n",blk,replybuf[3]);
      dump(cmd_data_packet,blksize+7,0);
      return;
    }  
   }
    
   // Закрытие потока 
   *((unsigned int*)&cmd_dload_end[1])=htonl(ptable[part].size);     
   *((unsigned int*)&cmd_dload_end[8])=htonl(ptable[part].code);
   iolen=send_cmd(cmd_dload_end,24,replybuf); // отсылаем команду
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Ошибка закрытия раздела, код ошибки = %02x\n",replybuf[3]);
    return;
  }  
   
}   
// Конец главного цикла  
printf("\n");


port_timeout(1);
// выходим из режима HDLC
send_cmd(cmddone,1,replybuf);

}  