//  Низкоуровневые процедуры работы с последовательным портом и HDLC

#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <io.h>

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

static int read(int siofd, void* buf, unsigned int len)
{
    DWORD bytes_read = 0;

    ReadFile(hSerial, buf, len, &bytes_read, NULL);
 
    return bytes_read;
}

static int write(int siofd, void* buf, unsigned int len)
{
    DWORD bytes_written = 0;

    WriteFile(hSerial, buf, len, &bytes_written, NULL);

    return bytes_written;
}

//*************************************************
//*    отсылка буфера в модем
//*************************************************
unsigned int send_unframed_buf(char* outcmdbuf, unsigned int outlen) {


PurgeComm(hSerial, PURGE_RXCLEAR);

write(siofd,"\x7e",1);  // отсылаем префикс

if (write(siofd,outcmdbuf,outlen) == 0) {   printf(_("\n Error writing command"));return 0;  }
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

DEFINE_GUID(GUID_DEVCLASS_PORTS, 0x4D36E978, 0xE325, 0x11CE, 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18);

static int find_port(int* port_no, char* port_name)
{
  HDEVINFO device_info_set;
  DWORD member_index = 0;
  SP_DEVINFO_DATA device_info_data;
  DWORD reg_data_type;
  char property_buffer[256];
  DWORD required_size;
  char* p;
  int result = 1;

  device_info_set = SetupDiGetClassDevs(&GUID_DEVCLASS_PORTS, NULL, 0, DIGCF_PRESENT);

  if (device_info_set == INVALID_HANDLE_VALUE)
    return result;

  while (TRUE)
  {
    ZeroMemory(&device_info_data, sizeof(SP_DEVINFO_DATA));
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

    if (!SetupDiEnumDeviceInfo(device_info_set, member_index, &device_info_data))
      break;

    member_index++;

    if (!SetupDiGetDeviceRegistryPropertyA(device_info_set, &device_info_data, SPDRP_HARDWAREID,
             &reg_data_type, (PBYTE)property_buffer, sizeof(property_buffer), &required_size))
      continue;

    if (
        (
         strstr(_strupr(property_buffer), "VID_12D1&PID_1C05") != NULL &&
         strstr(_strupr(property_buffer), "&MI_02") != NULL
        ) ||
        (
         strstr(_strupr(property_buffer), "VID_12D1&PID_1442") != NULL &&
         strstr(_strupr(property_buffer), "&MI_00") != NULL
        )
       )
    {
      if (SetupDiGetDeviceRegistryPropertyA(device_info_set, &device_info_data, SPDRP_FRIENDLYNAME,
              &reg_data_type, (PBYTE)property_buffer, sizeof(property_buffer), &required_size))
      {
        p = strstr(property_buffer, " (COM");
        if (p != NULL)
        {
          *port_no = atoi(p + 5);
          strcpy(port_name, property_buffer);
          result = 0;
        }
      }
      break;
    }
  }

  SetupDiDestroyDeviceInfoList(device_info_set);

  return result;
}

//***************************************************
// Открытие и настройка последовательного порта
//***************************************************

int open_port(char* devname) {

DCB dcbSerialParams = {0};
COMMTIMEOUTS CommTimeouts;

char device[20] = "\\\\.\\COM";
int port_no;
char port_name[256];

if (*devname == '\0')
{
  printf(_("\n\nSearching for serial port...\n"));
  
  if (find_port(&port_no, port_name) == 0)
  {
    sprintf(devname, "%d", port_no);
    printf(_("Port: \"%s\"\n"), port_name);
  }
  else
  {
    printf(_("Port not found!\n"));
    exit(0); 
  }
    //printf("\n! - Последовательный порт не задан\n"); 
    //exit(0); 
}

strcat(device, devname);

hSerial = CreateFileA(device, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
if (hSerial == INVALID_HANDLE_VALUE)
{
   printf(_("\n! - Cannot open serial port COM%s\n"), devname); 
   exit(0); 
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
    printf(_("\n! - Serial port initialization failed\n")); 
    exit(0); 
    //return -1;
}

CommTimeouts.ReadIntervalTimeout = 5;
CommTimeouts.ReadTotalTimeoutConstant = 30000;
CommTimeouts.ReadTotalTimeoutMultiplier = 0;
CommTimeouts.WriteTotalTimeoutConstant = 0;
CommTimeouts.WriteTotalTimeoutMultiplier = 0;
if (!SetCommTimeouts(hSerial, &CommTimeouts))
{
    CloseHandle(hSerial);
    printf(_("\n! - Serial port initialization failed\n")); 
    exit(0); 
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
  printf(_("\n Invalid filename format - %s\n"),fname);
  exit(1);
}

// проверяем цифровое поле ID раздела
if (strspn(fname+3,"0123456789AaBbCcDdEeFf") != 8) {
  printf(_("\n Invalid parition identifier - non-digit character - %s\n"),filename);
  exit(1);
}  
sscanf(fname+3,"%8x",id);

// Проверяем доступность и читаемость файла
in=fopen(filename,"rb");
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
  
