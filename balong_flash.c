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
#include "zlib.h"

unsigned char replybuf[4096];
// флаг ошибки структуры файла
unsigned int errflag=0;
void gparm(char* sparm);

int gflag;
uint8_t signver[200];

//***********************************************
//* Таблица разделов
//***********************************************
struct ptb_t ptable[120];
int npart=0; // число разделов в таблице


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

res=atcmd("^DLOADVER?",replybuf);
if (res == 0) return 0;
if (strncmp(replybuf+2,"2.0",3) == 0) return 1;
for (i=2;i<res;i++) {
  if (replybuf[i] == 0x0d) replybuf[i]=0;
}  
printf("! Неправильная версия монитора прошивки - [%i]%s\n",res,replybuf+2);
dump(replybuf,res,0);
return -1;
}
  
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

void main(int argc, char* argv[]) {

unsigned int i,opt,iolen;
int res;
FILE* in;
FILE* out;

#ifndef WIN32
unsigned char devname[50]="/dev/ttyUSB0";
#else
char devname[50] = "";
#endif

unsigned char OKrsp[]={0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};
// ответ на ^signver
unsigned char SVrsp[]={0x0d, 0x0a, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};

unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0,kflag=0,fflag=0;
unsigned char filename [100];

unsigned char fdir[40];   // каталог для мультифайловой прошивки

unsigned char cmdver[7]={0x0c};           // версия протокола
unsigned char cmddone[7]={0x1};           // команда выхода из HDLC
unsigned char cmd_reset[7]={0xa};           // команда выхода из HDLC

unsigned char cmd_getproduct[30]={0x45};
// Коды типов разделов
//-d       - попытка переключить модем из режима HDLC в АТ-режим\n\       

while ((opt = getopt(argc, argv, "hp:mersng:kf")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n Утилита предназначена для прошивки модемов на чипсете Balong V7\n\n\
%s [ключи] <имя файла для загрузки или имя каталога с файлами>\n\n\
 Допустимы следующие ключи:\n\n"
#ifndef WIN32
"-p <tty> - последовательный порт для общения с загрузчиком (по умолчанию /dev/ttyUSB0)\n"
#else
"-p <tty> - последовательный порт для общения с загрузчиком\n"
#endif
"-n       - режим мультифайловой прошивки из указанного каталога\n\
-g#      - установка режима цифровой подписи (-gl - описание параметров)\n\
-m       - вывести карту файла прошивки и завершить работу\n\
-e       - разобрать файл прошивки на разделы без заголовков\n\
-s       - разобрать файл прошивки на разделы с заголовками\n\
-k       - не перезагружать модем по окончании прошивки\n\
-r       - принудительно перезагрузить модем без прошивки разделов\n\
-f       - прошить даже при наличии ошибок CRC в исходном файле\n\
\n",argv[0]);
    return;

   case 'p':
    strcpy(devname,optarg);
    break;

   case 'm':
     mflag=1;
     break;
     
   case 'n':
     nflag=1;
     break;
     
   case 'f':
     fflag=1;
     break;
     
   case 'r':
     rflag=1;
     break;
     
   case 'k':
     kflag=1;
     break;
     
   case 'e':
     eflag=1;
     break;

   case 's':
     sflag=1;
     break;

   case 'g':
     gparm(optarg);
     break;
     
   case '?':
   case ':':  
     return;
  }
}  
printf("\n Программа для прошивки устройств на Balong-чипсете, V3.0.%i, (c) forth32, 2015, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n Порт для Windows 32bit  (c) rust3028, 2016");
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf("\n Ключи -s и -e несовместимы\n");
  return;
}  

if (kflag&rflag) {
  printf("\n Ключи -k и -r несовместимы\n");
  return;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n Ключ -n несовместим с ключами -s, -m и -e\n");
  return;
}  
  

// ------  перезагрузка без указания файла
//--------------------------------------------
if ((optind>=argc)&rflag) goto sio; 


// Открытие входного файла
//--------------------------------------------
if (optind>=argc) {
  if (nflag)
    printf("\n - Не указан каталог с файлами\n");
  else 
    printf("\n - Не указано имя файла для загрузки, используйте ключ -h для подсказки\n");
  return;
}  

if (nflag) 
  // для -n - просто копируем префикс
  strncpy(fdir,argv[optind],39);
else {
  // для однофайловых операций
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\n Ошибка открытия %s",argv[optind]);
  return;
}
}


// Поиск разделов внутри файла
if (!nflag) {
  findparts(in);
  printf("\n Версия прошивки: %s",ptable[0].hd.version);
  printf("\n Дата сборки:     %s %s",ptable[0].hd.date,ptable[0].hd.time);
  printf("\n Заголовок: версия %i,  платформа %8.8s",ptable[0].hd.hdversion,ptable[0].hd.unlock);
}  

// Поиск файлов прошивок в указанном каталоге
else findfiles(fdir);

  
//------ Режим вывода карты файла прошивки
//--------------------------------------------
  
if (mflag) {
 printf("\n\n ## Смещение  Размер  Сжатие  Имя\n-------------------------------------");
 for (i=0;i<npart;i++) { 
     printf("\n %02i %08x %8i",i,ptable[i].offset,ptable[i].hd.psize);
     if (ptable[i].zflag == 0) printf("        ");
     else printf("  %3i%%   ",(ptable[i].hd.psize-ptable[i].zflag)*100/ptable[i].hd.psize);
     printf("%s",ptable[i].pname); 
 }   
 printf("\n");
 return;
}

// выход по ошибкам CRC
if (!fflag && errflag) {
    printf("\n\n! Входной файл содержит ошибки - завершаем работу\n");
    return; 
}

//------- Режим разрезания файла прошивки
//--------------------------------------------
if (eflag|sflag) {
 printf("\n Выделение разделов из файла прошивки:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
 for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].hd.psize,ptable[i].pname); 
   // формируем имя файла
   sprintf(filename,"%02i-%08x-%s.%s",i,ptable[i].hd.code,ptable[i].pname,(eflag?"bin":"fw"));
   out=fopen(filename,"wb");
   
   if(sflag) {
     // запись заголовка
     fwrite(&ptable[i].hd,1,sizeof(struct pheader),out);   // фиксированный заголовок
     fwrite(&ptable[i].csumblock,1,ptable[i].hd.hdsize-sizeof(struct pheader),out); // блок контрольных сумм
   }
   // запись тела
   fwrite(ptable[i].pimage,ptable[i].hd.psize,1,out);
   fclose(out);
 }
printf("\n");
return;
}


sio:
//--------- Основной режим - запись прошивки
//--------------------------------------------

// Настройка SIO

#ifndef WIN32

if (!open_port(devname))  {
   printf("\n! - Последовательный порт %s не открывается\n", devname); 
   return; 
}

tcflush(siofd,TCIOFLUSH);  // очистка выходного буфера

#else

if (*devname == '\0')
{
    printf("\n! - Последовательный порт не задан\n"); 
    return; 
}

res = open_port(devname);
if (res == 0)  {
   printf("\n! - Последовательный порт COM%s не открывается\n", devname); 
   return; 
}
else if (res == -1)  {
   printf("\n! - Ошибка при инициализации COM-порта\n"); 
   return; 
}

#endif

res=dloadversion();
if (res == -1) return;
if (res == 0) {
  printf("\n Модем уже находится в HDLC-режиме");
  goto hdlc;
}

// Если надо, отправляем команду цифровой подписи
if (gflag) {
//  printf("\n Отправляем signver...");
 res=atcmd(signver,replybuf);
 if (memcmp(replybuf,SVrsp,sizeof(SVrsp)) != 0) {
   printf("\n ! Ошибка проверки цифровой сигнатуры\n");
//    return;
}
}

// Входим в HDLC-режим
// printf("\n Входим в режим HDLC...");

#ifndef WIN32
usleep(100000);
#else
Sleep(100);
#endif

res=atcmd("^DATAMODE",replybuf);
if (res != 6) {
  printf("\n Неправильная длина ответа на ^DATAMODE");
//   dump(replybuf,res,0);
  return;
}  
if (memcmp(replybuf,OKrsp,6) != 0) {
  printf("\n Команда ^DATAMODE отвергнута, возможно требуется режим цифровой подписи\n");
//   dump(replybuf,res,0);
  return;
}  

// Вошли в HDLC
//------------------------------
hdlc:

iolen=send_cmd(cmdver,1,replybuf);
if (iolen == 0) {
  printf("\n Нет ответа от модема в HDLC-режиме\n");
  return;
}  
if (replybuf[0] == 0x7e) memcpy(replybuf,replybuf+1,iolen-1);
if (replybuf[0] != 0x0d) {
  printf("\n Ошибка получения версии протокола\n");
  return;
}  
  
i=replybuf[1];
replybuf[2+i]=0;
printf("ok");
printf("\n Версия протокола: %s",replybuf+2);


iolen=send_cmd(cmd_getproduct,1,replybuf);
if (iolen>2) printf("\n Идентификатор устройства: %s",replybuf+2); 

printf("\n----------------------------------------------------\n");

if ((optind>=argc)&rflag) goto reset; // перезагрузка без указания файла

// Записываем всю флешку
// printf("\n=== Запись разделов в устройство ===\n");
flash_all();
printf("\n");


port_timeout(1);
// выходим из режима HDLC и перезагружаемся
reset:

if (rflag || !kflag) {
  printf("\n Перезагрузка модема...\n");
  send_cmd(cmd_reset,1,replybuf);
  atcmd("^RESET",replybuf);
}
// выход из HDLC   
else send_cmd(cmddone,1,replybuf);
} 
