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
int gflag;

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

setlocale(LC_ALL, "");
bindtextdomain("balongflash", LOCALE_DIR);
textdomain("balongflash");

// разбор командной строки
while ((opt = getopt(argc, argv, "hp:mersng:kf")) != -1) {
  switch (opt) {
   case 'h': 
     
printf(_("\n Flasher tool for USB-modems on Balong V7 chipset\n\n"
         "%s [option] <file or directory>\n\n"
         " options:\n\n"
         "-p <tty> - bootloader serial port%s\n"
         "-n       - flash multiple files from directory\n"
         "-g#      - digital signature mode (use '-gl' to list modes)\n"
         "-m       - print firmware map and exit\n"
         "-e       - split firmware by partitions without headers\n"
         "-s       - split firmware by partitions with headers\n"
         "-k       - do not reboot modem after flashing\n"
         "-r       - reboot modem without flashing\n"
         "-f       - ignore CRC errors in firmware file\n"
         "\n"), argv[0],
#ifndef win32
        _(" (default is /dev/ttyUSB0)")
#else
        ""
#endif
);
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
     
   case '?':
   case ':':  
     return -1;
  }
}  
printf(_("\n Flasher tool for Balong-based devices, V3.0.%i, (c) forth32, 2015, GNU GPLv3"), BUILDNO);
#ifdef WIN32
printf(_("\n Windows 32bit port (c) rust3028, 2016"));
#endif
printf("\n--------------------------------------------------------------------------------------------------\n");

if (eflag&sflag) {
  printf(_("\n Options -s and -e are incompatiple\n"));
  return -1;
}  

if (kflag&rflag) {
  printf(_("\n Options -k and -r are incompatiple\n"));
  return -1;
}  

if (nflag&(eflag|sflag|mflag)) {
  printf(_("\n Option -n is not compatible with options -s, -m and -e\n"));
  return -1;
}  
  

// ------  перезагрузка без указания файла
//--------------------------------------------
if ((optind>=argc)&rflag) goto sio; 


// Открытие входного файла
//--------------------------------------------
if (optind>=argc) {
  if (nflag)
    printf(_("\n - Directory is missing\n"));
  else 
    printf(_("\n - Filename is missing, use -h for help\n"));
  return -1;
}  

if (nflag) 
  // для -n - просто копируем префикс
  strncpy(fdir,argv[optind],39);
else {
  // для однофайловых операций
in=fopen(argv[optind],"rb");
if (in == 0) {
  printf(_("\n Cannot open %s"),argv[optind]);
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
    printf(_("\n\n! Errors in input file - quitting\n"));
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
  printf(_("\n Modem is already in HDLC-mode"));
  goto hdlc;
}

// Если надо, отправляем команду цифровой подписи
if (gflag) send_signver();

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
