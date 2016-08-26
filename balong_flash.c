#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "hdlcio.h"
#include "ptable.h"

unsigned char replybuf[4096];

// Цифровые подписи

struct {
  uint8_t type;
  uint32_t len;
  char* descr;
} signbase[] = {
  {1,2958,"Основная прошивка"},
  {1,2694,"Прошивка E3372s-stick"},
  {2,1110,"Вебинтерфейс+ISO для HLINK-модема"},
  {6,1110,"Вебинтерфейс+ISO для HLINK-модема"},
  {2,846,"ISO (dashboard) для stick-модема"},
  {99,1110,"универсальная"},
};

#define signbaselen 6

struct {
  uint8_t code;
  char* descr;
} fwtypes[]={
  {1,"ONLY_FW"},
  {2,"ONLY_ISO"},
  {3,"FW_ISO"},
  {4,"ONLY_WEBUI"},
  {5,"FW_WEBUI"},
  {6,"ISO_WEBUI"},
  {7,"FW_ISO_WEBUI"},
  {99,"COMPONENT_MAX"},
  {0,0}
};  


// результирующая строка ^signver - команды
static unsigned char signver[100];
// Флаг режима цифровой подписи
int gflag=0;

//****************************************************
//* Получение описания типа прошивки по коду
//****************************************************
char* fw_description(uint8_t code) {
  
int i;  
for (i=0; (fwtypes[i].code != 0); i++) {
  if (code == fwtypes[i].code) return fwtypes[i].descr;
}
return 0;
}

//****************************************************
//* Получение списка параметров ключа -g
//****************************************************
void glist() {
  
int i;
printf("\n #  длина  тип описание \n--------------------------------------");
for (i=0; i<signbaselen; i++) {
  printf("\n%1i  %5i  %2i   %s",i,signbase[i].len,signbase[i].type,signbase[i].descr);
}
printf("\n\n Также можно указать произвольные параметры подписи в формате:\n  -g *,type,len\n\n");
exit(0);
}

//***************************************************
//* Обработка параметров ключа -g
//***************************************************
void gparm(char* sparm) {
  
int index;  
char* sptr;
char parm[100];

// Параметры текущей цифровой подписи
uint32_t signtype; // тип прошивки
uint32_t signlen;  // длина подписи


if (gflag != 0) {
  printf("\n Дублирующийся ключ -g\n\n");
  exit(0);
}  

strcpy(parm,sparm); // локальная копия параметров

if (parm[0] == 'l') {
  glist();
  exit(0);
}  

if (strncmp(parm,"*,",2) == 0) {
  // произвольные параметры
  // выделяем длину
  sptr=strrchr(parm,',');
  if (sptr == 0) goto perror;
  signlen=atoi(sptr+1);
  *sptr=0;
  // выделяем тип раздела
  sptr=strrchr(parm,',');
  if (sptr == 0) goto perror;
  signtype=atoi(sptr+1);
  if (fw_description(signtype) == 0) {
    printf("\n Ключ -g: неизвестный тип прошивки - %i\n",signtype);
    exit(0);
  }  
}
else {  
  index=atoi(parm);
  if (index >= signbaselen) goto perror;
  signlen=signbase[index].len;
  signtype=signbase[index].type;
}

gflag=1;
sprintf(signver,"^SIGNVER=%i,0,778A8D175E602B7B779D9E05C330B5279B0661BF2EED99A20445B366D63DD697,%i",signtype,signlen);
printf("\n Режим цифровой подписи: %s (%i байт)",fw_description(signtype),signlen);
// printf("\nstr - %s",signver);
return;

perror:
 printf("\n Ошибка в параметрах ключа -g\n");
 exit(0);
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

unsigned int i,j,opt,npart=0,iolen,part,blk,blksize;
int res;
FILE* in;
FILE* out;

struct ptb_t ptable[100];

unsigned char buf[40960];
unsigned char devname[50]="/dev/ttyUSB0";


unsigned int err;

unsigned char OKrsp[]={0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};
unsigned char NAKrsp[]={0x03, 0x00, 0x02, 0xba, 0x0a, 0x7e};
// ответ на ^signver
unsigned char SVrsp[]={0x0d, 0x0a, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x4f, 0x4b, 0x0d, 0x0a};

unsigned int  dpattern=0xa55aaa55;
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0;
unsigned char filename [100];

unsigned char fdir[40];   // каталог для мультифайловой прошивки

unsigned char cmdver[7]={0x0c};           // версия протокола
unsigned char cmddone[7]={0x1};           // команда выхода из HDLC
unsigned char cmd_reset[7]={0xa};           // команда выхода из HDLC
unsigned char cmd_dload_init[15]={0x41};  // команда начала раздела
unsigned char cmd_data_packet[11000]={0x42};  // команда начала пакета
unsigned char cmd_dload_end[30]={0x43};       // команда конца раздела
// Коды типов разделов
//-d       - попытка переключить модем из режима HDLC в АТ-режим\n\       

while ((opt = getopt(argc, argv, "hp:mersng:")) != -1) {
  switch (opt) {
   case 'h': 
     
printf("\n Утилита предназначена для прошивки модемов E3372S\n\n\
%s [ключи] <имя файла для загрузки или имя каталога с файлами>\n\n\
 Допустимы следующие ключи:\n\n\
-p <tty> - последовательный порт для общения с загрузчиком (по умолчанию /dev/ttyUSB0)\n\
-n       - режим мультифайловой прошивки из указанного каталога\n\
-g#      - установка режима цифровой подписи (-gl - описание параметров)\n\
-m       - вывести карту файла прошивки и завершить работу\n\
-e       - разобрать файл прошивки на разделы без заголовков\n\
-s       - разобрать файл прошивки на разделы с заголовками\n\
-r       - выйти из режима прошивки и перезагрузить модем\n\
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
     
   case 'r':
     rflag=1;
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

if (eflag&sflag) {
  printf("\n Ключи -s и -e несовместимы\n");
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
    printf("\n - Не указано имя файла для загрузки\n");
  return;
}  

if (nflag) 
  // для -n - просто копируем префикс
  strncpy(fdir,argv[optind],39);
else {
  // для однофайловых операций
in=fopen(argv[optind],"r");
if (in == 0) {
  printf("\n Ошибка открытия %s",argv[optind]);
  return;
}
}


// Поиск разделов внутри файла
//--------------------------------------------

if (!nflag) {
  printf("\n Разбираем файл прошвки...");
  npart=findparts(in,ptable);
  if (npart == 0) {
    printf("\nРазделы не найдены!");
    return ;
  }  
}  

// Поиск файлов прошивок в указанном каталоге
//--------------------------------------------
else {
  printf("\n Поиск файлов-образов разделов...\n\n ##   Размер      ID       Имя       Файл\n-----------------------------------------------------------------\n");
  for (npart=0;npart<30;npart++) {
    if (find_file(npart, fdir, ptable[npart].filename, &ptable[npart].code, &ptable[npart].size) == 0) break; // конец поиска - раздела с таким ID не нашли
    // получаем символическое имя раздела
    find_pname(ptable[npart].code,ptable[npart].pname);
    printf("\n %02i  %8i  %08x  %-8.8s  %s",npart,ptable[npart].size,ptable[npart].code,ptable[npart].pname,ptable[npart].filename);
  }
}

printf("\n Найдено %i разделов",npart);

  
//------ Режим вывода карты файла прошивки
//--------------------------------------------
  
if (mflag) {
 printf("\n Таблица разделов, найденных в файле:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
 for (i=0;i<npart;i++) 
     printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].size,ptable[i].pname); 
 printf("\n");
 return;
}


//------- Режим разрезания файла прошивки
//--------------------------------------------
if (eflag|sflag) {
 printf("\n Выделение разделов из файла прошивки:\n\n ## Смещение  Размер   Имя\n-------------------------------------");
 for (i=0;i<npart;i++) {  
   printf("\n %02i %08x %8i  %s",i,ptable[i].offset,ptable[i].size,ptable[i].pname); 
   // формируем имя файла
   sprintf(filename,"%02i-%08x-%s.%s",i,ptable[i].code,ptable[i].pname,(eflag?"bin":"fw"));
   out=fopen(filename,"w");
   
   if(sflag) {
     // запись заголовка
     fwrite(&dpattern,1,4,out); // маркер- magic заголовка блока
     fseek(in,ptable[i].hdoffset,SEEK_SET); // встаем на начало заголовка
     fread(buf,1,ptable[i].offset-ptable[i].hdoffset,in);
     fwrite(buf,1,ptable[i].offset-ptable[i].hdoffset,out);
   }
   // запись тела
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


sio:
//--------- Основной режим - запись прошивки
//--------------------------------------------

// Настройка SIO

if (!open_port(devname))  {
   printf("\n - Последовательный порт %s не открывается\n", devname); 
   return; 
}


tcflush(siofd,TCIOFLUSH);  // очистка выходного буфера

res=dloadversion();
if (res == -1) return;
if (res == 0) {
  printf("\n Модем уже находится в HDLC-режиме");
  goto hdlc;
}

// Если надо, отправляем команду цифровой подписи
if (gflag) {
 printf("\n Отправлем signver...");
 res=atcmd(signver,replybuf);
 if (memcmp(replybuf,SVrsp,sizeof(SVrsp)) != 0) {
   printf("\n Ошибка проверки цифровой сигнатуры\n");
   return;
}
}

// Входим в HDLC-режим
printf("\n Входим в режим HDLC...");

usleep(100000);
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
if ((iolen == 0)||(replybuf[1] != 0x0d)) {
  printf("\n Ошибка получения версии протокола\n");
  return;
}  
  
i=replybuf[2];
replybuf[3+i]=0;
printf("ok");
printf("\n Версия протокола: %s",replybuf+3);
printf("\n");


if ((optind>=argc)&rflag) goto reset; // перезагрузка без указания файла


// Главный цикл записи разделов
for(part=0;part<npart;part++) {
  printf("\r Записываем раздел %i - %s\n",part,ptable[part].pname);
  
  // заполняем командный пакет
  *((unsigned int*)&cmd_dload_init[1])=htonl(ptable[part].code);  
  *((unsigned int*)&cmd_dload_init[5])=htonl(ptable[part].size);  
  // отсылаем команду
  iolen=send_cmd(cmd_dload_init,12,replybuf);
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Заголовок раздела не принят, код ошибки = %02x %02x %02x\n",replybuf[1],replybuf[2],replybuf[3]);
//    dump(cmd_dload_init,13,0);
    return;
  }  

  //  Подготовка к поблочному циклу
  blksize=4096; // начальное значение размера блока
  if (!nflag)   
  // встаем на начало раздела в однофайловом режиме
    fseek(in,ptable[part].offset,SEEK_SET);
  else 
  // открываем файл в многофайловом режиме  
    in=fopen(ptable[part].filename,"r");

  // Поблочный цикл
  for(blk=0;blk<((ptable[part].size+4095)/4096);blk++) {
    printf("\r Блок %i из %i",blk,(ptable[part].size+4095)/4096); fflush(stdout);
    res=ptable[part].size+ptable[part].offset-ftell(in);  // размер оставшегося куска до конца файла
    if (res<4096) blksize=res;  // корректируем размер последнего блока
    *(unsigned int*)&cmd_data_packet[1]=htonl(blk+1);  // # пакета
    *(unsigned short*)&cmd_data_packet[5]=htons(blksize);  // размер блока
    fread(cmd_data_packet+7,1,blksize,in); // читаем очередной кусок раздела в буфер команды
    iolen=send_cmd(cmd_data_packet,blksize+7,replybuf); // отсылаем команду
    if ((iolen == 0) || (replybuf[1] != 2)) {
      printf("\n Блок %i раздела не принят, код ошибки = %02x %02x %02x\n",blk,replybuf[1],replybuf[2],replybuf[3]);
//      dump(cmd_data_packet,blksize+7,0);
      return;
    }  
   }
   
   // Закрытие файла в многофайловом режиме
   if (nflag) fclose(in);
    
   // Закрытие потока 
   *((unsigned int*)&cmd_dload_end[1])=htonl(ptable[part].size);     
   *((unsigned int*)&cmd_dload_end[8])=htonl(ptable[part].code);
   iolen=send_cmd(cmd_dload_end,24,replybuf); // отсылаем команду
  if ((iolen == 0) || (replybuf[1] != 2)) {
    printf("\n Ошибка закрытия раздела, код ошибки = %02x %02x %02x\n",replybuf[1],replybuf[2],replybuf[3]);
//     dump(replybuf,iolen,0);
    return;
  }  
   
}   
// Конец главного цикла  
printf("\n");


port_timeout(1);
// выходим из режима HDLC и перезагружаемся
reset:

if (rflag) {
  printf("\n Перезарузка модема...\n");
  send_cmd(cmd_reset,1,replybuf);
  atcmd("^RESET",replybuf);
}
// выход из HDLC пока выкидываем  
//else send_cmd(cmddone,1,replybuf);
} 
