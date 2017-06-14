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

#define true 1
#define false 0


//***************************************************
//* Хранилище кода ошибки
//***************************************************
int errcode;


//***************************************************
//* Вывод кода ошибки команды
//***************************************************
void printerr() {
  
if (errcode == -1) printf(_(" - command timeout\n"));
else printf(_(" - error code %02x\n"), errcode);
}

//***************************************************
// Отправка команды начала раздела
// 
//  code - 32-битный код раздела
//  size - полный размер записываемого раздела
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int dload_start(uint32_t code,uint32_t size) {

uint32_t iolen;  
uint8_t replybuf[4096];
  
#ifndef WIN32
static struct __attribute__ ((__packed__))  {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t code;
  uint32_t size;
  uint8_t pool[3];
} cmd_dload_init =  {0x41,0,0,{0,0,0}};
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_init.code=htonl(code);
cmd_dload_init.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_init,sizeof(cmd_dload_init),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
else return true;
}  

//***************************************************
// Отправка блока раздела
// 
//  blk - # блока
//  pimage - адрес начала образа раздела в памяти
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int dload_block(uint32_t part, uint32_t blk, uint8_t* pimage) {

uint32_t res,blksize,iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t blk;
  uint16_t bsize;
  uint8_t data[fblock];
} cmd_dload_block;  
#ifdef WIN32
#pragma pack(pop)
#endif

blksize=fblock; // начальное значение размера блока
res=ptable[part].hd.psize-blk*fblock;  // размер оставшегося куска до конца файла
if (res<fblock) blksize=res;  // корректируем размер последнего блока

// код команды
cmd_dload_block.cmd=0x42;
// номер блока
cmd_dload_block.blk=htonl(blk+1);
// размер блока
cmd_dload_block.bsize=htons(blksize);
// порция данных из образа раздела
memcpy(cmd_dload_block.data,pimage+blk*fblock,blksize);
// отсылаем блок в модем
iolen=send_cmd((uint8_t*)&cmd_dload_block,sizeof(cmd_dload_block)-fblock+blksize,replybuf); // отсылаем команду

errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2))  {
  if (iolen == 0) errcode=-1;
  return false;
}
return true;
}

  
//***************************************************
// Завершение записи раздела
// 
//  code - код раздела
//  size - размер раздела
// 
//*  результат:
//  false - ошибка
//  true - команда принята модемом
//***************************************************
int dload_end(uint32_t code, uint32_t size) {

uint32_t iolen;
uint8_t replybuf[4096];

#ifndef WIN32
static struct __attribute__ ((__packed__)) {
#else
#pragma pack(push,1)
static struct {
#endif
  uint8_t cmd;
  uint32_t size;
  uint8_t garbage[3];
  uint32_t code;
  uint8_t garbage1[11];
} cmd_dload_end;
#ifdef WIN32
#pragma pack(pop)
#endif


cmd_dload_end.cmd=0x43;
cmd_dload_end.code=htonl(code);
cmd_dload_end.size=htonl(size);
iolen=send_cmd((uint8_t*)&cmd_dload_end,sizeof(cmd_dload_end),replybuf);
errcode=replybuf[3];
if ((iolen == 0) || (replybuf[1] != 2)) {
  if (iolen == 0) errcode=-1;
  return false;
}  
return true;
}  



//***************************************************
//* Запись в модем всех разделов из таблицы
//***************************************************
void flash_all() {

int32_t part;
uint32_t blk,maxblock;

printf(_("\n##  --- Partition name --- written"));
// Главный цикл записи разделов
for(part=0;part<npart;part++) {
printf("\n");  
//  printf("\n02i %s)",part,ptable[part].pname);
 // команда начала раздела
 if (!dload_start(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf(_("\n! Partition %i (%s) header rejected"),part,ptable[part].pname);
   printerr();
   exit(-2);
 }  
    
 maxblock=(ptable[part].hd.psize+(fblock-1))/fblock; // число блоков в разделе
 // Поблочный цикл передачи образа раздела
 for(blk=0;blk<maxblock;blk++) {
  // Вывод процента записанного
  printf("\r%02i  %-20s  %i%%",part,ptable[part].pname,(blk+1)*100/maxblock); 

    // Отсылаем очередной блок
  if (!dload_block(part,blk,ptable[part].pimage)) {
   printf(_("\n! Partition %i (%s) block %i rejected"),part,ptable[part].pname,blk);
   printerr();
   exit(-2);
  }  
 }    

// закрываем раздел
 if (!dload_end(ptable[part].hd.code,ptable[part].hd.psize)) {
   printf(_("\n! Error closing partition %i (%s)"),part,ptable[part].pname);
   printerr();
   exit(-2);
 }  
} // конец цикла по разделам
}
