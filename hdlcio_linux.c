//  Низкоуровневые процедуры работы с последовательным портом и HDLC

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
#include "util.h"

unsigned int nand_cmd=0x1b400000;
unsigned int spp=0;
unsigned int pagesize=0;
unsigned int sectorsize=512;
unsigned int maxblock=0;     // Общее число блоков флешки
char flash_mfr[30]={0};
char flash_descr[30]={0};
unsigned int oobsize=0;

static char pdev[500]; // имя последовательного порта

int siofd; // fd для работы с Последовательным портом
struct termios sioparm;
//int siofd; // fd для работы с Последовательным портом

//*************************************************
//*    отсылка буфера в модем
//*************************************************
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


tcflush(siofd,TCIOFLUSH);  // сбрасываем недочитанный буфер ввода

write(siofd,"\x7e",1);  // отсылаем префикс

if (write(siofd,outcmdbuf,outlen) == 0) {   printf(_("\n Error writing command"));return 0;  }
tcdrain(siofd);  // ждем окончания вывода блока

return 1;
}

//******************************************************************************************
//* Прием буфера с ответом из модема
//*
//*  masslen - число байтов, принимаемых единым блоком без анализа признака конца 7F
//******************************************************************************************

unsigned int receive_reply(char* iobuf, int masslen) {
  
int i,iolen,escflag,incount;
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
   printf(_("\nModem answer is too short: %i bytes, expected %i bytes\n"),res+1,masslen);
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

int i,iolen,bcnt;
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

//***************************************************
// Открытие и настройка последовательного порта
//***************************************************

int open_port(char* devname) {


int i,dflag=1;
char devstr[200]={0};


if (strlen(devname) != 0) strcpy(pdev,devname);   // сохраняем имя порта  
else strcpy(devname,"/dev/ttyUSB0");  // если имя порта не было задано

// Вместо полного имени устройства разрешается передавать только номер ttyUSB-порта

// Проверяем имя устройства на наличие нецифровых символов
for(i=0;i<strlen(devname);i++) {
  if ((devname[i]<'0') || (devname[i]>'9')) dflag=0;
}
// Если в строке - только цифры, добавляем префикс /dev/ttyUSB

if (dflag) strcpy(devstr,"/dev/ttyUSB");

// копируем имя устройства
strcat(devstr,devname);

siofd = open(devstr, O_RDWR | O_NOCTTY |O_SYNC);
if (siofd == -1) {
  printf(_("\n! - Cannot open serial port %s\n"), devname); 
  exit(0);
}
bzero(&sioparm, sizeof(sioparm)); // готовим блок атрибутов termios
sioparm.c_cflag = B115200 | CS8 | CLOCAL | CREAD ;
sioparm.c_iflag = 0;  // INPCK;
sioparm.c_oflag = 0;
sioparm.c_lflag = 0;
sioparm.c_cc[VTIME]=30; // timeout  
sioparm.c_cc[VMIN]=0;  
tcsetattr(siofd, TCSANOW, &sioparm);

tcflush(siofd,TCIOFLUSH);  // очистка выходного буфера

return 1;
}


//*************************************
// Настройка времени ожидания порта
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
  printf(_("\n Cannot open directory %s\n"),dirname);
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
  printf(_("\n Invalid filename format - %s\n"),dentry->d_name);
  exit(1);
}

// проверяем цифровое поле ID раздела
if (strspn(dentry->d_name+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf(_("\n Invalid parition identifier - non-digit character - %s\n"),filename);
  exit(1);
}  
sscanf(dentry->d_name+3,"%8x",id);

// Проверяем доступность и читаемость файла
in=fopen(filename,"r");
if (in == 0) {
  printf(_("\n Error opening file %s\n"),filename);
  exit(1);
}
if (fread(&pt,1,4,in) != 4) {
  printf(_("\n Error reading file %s\n"),filename);
  exit(1);
}
  
// проверяем, что файл - сырой образ, без заголовка
if (pt == 0xa55aaa55) {
  printf(_("\n File %s has a header - cannot flash\n"),filename);
  exit(1);
}


// Что еще можно проверить? Пока не придумал.  

//  Получаем размер файла
fseek(in,0,SEEK_END);
*size=ftell(in);
fclose(in);

return 1;
}

//****************************************************
//*  Отсылка модему АТ-команды
//*  
//* cmd - буфер с командой
//* rbuf - буфер для записи ответа
//*
//* Возвращает длину ответа
//****************************************************
int atcmd(char* cmd, char* rbuf) {

int res;
char cbuf[128];

strcpy(cbuf,"AT");
strcat(cbuf,cmd);
strcat(cbuf,"\r");

port_timeout(100);
// Вычищаем буфер приемника и передатчика
tcflush(siofd,TCIOFLUSH);

// отправка команды
write(siofd,cbuf,strlen(cbuf));
usleep(100000);

// чтение результата
res=read(siofd,rbuf,200);
return res;
}
  
