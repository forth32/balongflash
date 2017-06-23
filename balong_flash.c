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
#include "signver.h"
#include "zlib.h"

// флаг ошибки структуры файла
unsigned int errflag=0;

// флаг цифровой подписи
int gflag=0;
// флаг типа прошивки
int dflag=0;

// тип прошивки из заголовка файла
int dload_id=-1;

//***********************************************
//* Таблица разделов
//***********************************************
struct ptb_t ptable[120];
int npart=0; // число разделов в таблице


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

int main(int argc, char* argv[]) {

unsigned int opt;
int res;
FILE* in;
char devname[50] = "";
unsigned int  mflag=0,eflag=0,rflag=0,sflag=0,nflag=0,kflag=0,fflag=0;
unsigned char fdir[40];   // каталог для мультифайловой прошивки

// разбор командной строки
while ((opt = getopt(argc, argv, "d:hp:mersng:kf")) != -1) {
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
-g#      - установка режима цифровой подписи\n\
  -gl - описание параметров\n\
  -gd - запрет автоопределения подписи\n\
-m       - вывести карту файла прошивки и завершить работу\n\
-e       - разобрать файл прошивки на разделы без заголовков\n\
-s       - разобрать файл прошивки на разделы с заголовками\n\
-k       - не перезагружать модем по окончании прошивки\n\
-r       - принудительно перезагрузить модем без прошивки разделов\n\
-f       - прошить даже при наличии ошибок CRC в исходном файле\n\
-d#      - установка типа прошивки (DLOAD_ID, 0..7), -dl - список типов\n\
\n",argv[0]);
    return 0;

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
     
   case 'd':
     dparm(optarg);
     break;
     
   case '?':
   case ':':  
     return -1;
  }
}  
printf("\n Программа для прошивки устройств на Balong-чипсете, V3.0.%i, (c) forth32, 2015, GNU GPLv3",BUILDNO);
#ifdef WIN32
printf("\n Порт для Windows 32bit  (c) rust3028, 2016");
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf("\n Ключи -s и -e несовместимы\n");
  return -1;
}  

if (kflag&rflag) {
  printf("\n Ключи -k и -r несовместимы\n");
  return -1;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf("\n Ключ -n несовместим с ключами -s, -m и -e\n");
  return -1;
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
  return -1;
}  

if (nflag) 
  // для -n - просто копируем префикс
  strncpy(fdir,argv[optind],39);
else {
  // для однофайловых операций
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf("\n Ошибка открытия %s",argv[optind]);
  return -1;
}
}


// Поиск разделов внутри файла
if (!nflag) {
  findparts(in);
  show_fw_info();
}  

// Поиск файлов прошивок в указанном каталоге
else findfiles(fdir);
  
//------ Режим вывода карты файла прошивки
if (mflag) show_file_map();

// выход по ошибкам CRC
if (!fflag && errflag) {
    printf("\n\n! Входной файл содержит ошибки - завершаем работу\n");
    return -1; 
}

//------- Режим разрезания файла прошивки
if (eflag|sflag) {
  fwsplit(sflag);
  printf("\n");
  return 0;
}

sio:
//--------- Основной режим - запись прошивки
//--------------------------------------------

// Настройка SIO
open_port(devname);

// Определяем режим порта и версию dload-протокола

res=dloadversion();
if (res == -1) return -2;
if (res == 0) {
  printf("\n Модем уже находится в HDLC-режиме");
  goto hdlc;
}

// Если надо, отправляем команду цифровой подписи
if (gflag != -1) send_signver();

// Входим в HDLC-режим

usleep(100000);
enter_hdlc();

// Вошли в HDLC
//------------------------------
hdlc:

// получаем версию протокола и идентификатор устройства
protocol_version();
dev_ident();


printf("\n----------------------------------------------------\n");

if ((optind>=argc)&rflag) {
  // перезагрузка без указания файла
  restart_modem();
  exit(0);
}  

// Записываем всю флешку
flash_all();
printf("\n");

port_timeout(1);

// выходим из режима HDLC и перезагружаемся
if (rflag || !kflag) restart_modem();
// выход из HDLC без перезагрузки
else leave_hdlc();
} 
