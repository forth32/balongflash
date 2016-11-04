//  Низкоуровневые процедуры работы с последовательным портом и HDLC

#include <stdio.h>
#include <windows.h>
#include <io.h>
#include "printf.h"

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
static HANDLE hSerial;


//*************************************************
//*    отсылка буфера в модем
//*************************************************
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


PurgeComm(hSerial, PURGE_RXCLEAR);

write(siofd,"\x7e",1);  // отсылаем префикс

if (write(siofd,outcmdbuf,outlen) == 0) {   printf("\n Ошибка записи команды");return 0;  }
FlushFileBuffers(hSerial);

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

//***************************************************
// Открытие и настройка последовательного порта
//***************************************************

int open_port(char* devname) {


char device[20] = "\\\\.\\COM";
DCB dcbSerialParams = {0};
COMMTIMEOUTS CommTimeouts;

strcat(device, devname);

hSerial = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
if (hSerial == INVALID_HANDLE_VALUE)
{
    return 0;
}

ZeroMemory(&dcbSerialParams, sizeof(dcbSerialParams));
dcbSerialParams.DCBlength=sizeof(dcbSerialParams);
dcbSerialParams.BaudRate = CBR_115200;
dcbSerialParams.ByteSize = 8;
dcbSerialParams.StopBits = ONESTOPBIT;
dcbSerialParams.Parity = NOPARITY;
dcbSerialParams.fBinary = TRUE;
dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;
if (!SetCommState(hSerial, &dcbSerialParams))
{
    CloseHandle(hSerial);
    return -1;
}

CommTimeouts.ReadIntervalTimeout = 5;
CommTimeouts.ReadTotalTimeoutConstant = 30000;
CommTimeouts.ReadTotalTimeoutMultiplier = 0;
CommTimeouts.WriteTotalTimeoutConstant = 0;
CommTimeouts.WriteTotalTimeoutMultiplier = 0;
if (!SetCommTimeouts(hSerial, &CommTimeouts))
{
    CloseHandle(hSerial);
    return -1;
}

PurgeComm(hSerial, PURGE_RXCLEAR);


return 1;
}


//*************************************
// Настройка времени ожидания порта
//*************************************

void port_timeout(int timeout) {
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


char fpattern[80];
char fname[_MAX_PATH];
struct _finddata_t fileinfo;
intptr_t res;
FILE* in;
unsigned int pt;

sprintf(fpattern,"%s\\%02d*", dirname, num);
res = _findfirst(fpattern, &fileinfo);
_findclose(res);
if (res == -1)
    return 0;
if ((fileinfo.attrib & _A_SUBDIR) != 0)
    return 0;
strcpy(fname, fileinfo.name);
strcpy(filename, dirname);
strcat(filename, "\\");
strcat(filename, fname);  

// 00-00000200-M3Boot.bin
//проверяем имя файла на наличие знаков '-'
if (fname[2] != '-' || fname[11] != '-') {
  printf("\n Неправильный формат имени файла - %s\n",fname);
  exit(1);
}

// проверяем цифровое поле ID раздела
if (strspn(fname+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf("\n Ошибка в идентификаторе раздела - нецифровой знак - %s\n",filename);
  exit(1);
}  
sscanf(fname+3,"%8x",id);

// Проверяем доступность и читаемость файла
in=fopen(filename,"rb");
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

fclose(in);

*size = fileinfo.size;


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
PurgeComm(hSerial, PURGE_RXCLEAR);

// отправка команды
write(siofd,cbuf,strlen(cbuf));
Sleep(100);

// чтение результата
res=read(siofd,rbuf,200);
return res;
}
  
