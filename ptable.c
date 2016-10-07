// Процедуры работы с таблицей разделов

#include <stdio.h>
#include <stdint.h>
#ifndef WIN32
#include <string.h>
#include <stdlib.h>
#else
#include <windows.h>
#include "printf.h"
#endif

#include "ptable.h"

//******************************************************
//*  поиск символического имени раздела по его коду
//******************************************************

void  find_pname(unsigned int id,unsigned char* pname) {

unsigned int j;
struct {
  char name[20];
  int code;
} pcodes[]={ 
  {"M3Boot",0x20000}, 
  {"M3Boot-ptable",0x10000}, 
  {"M3Boot_R11",0x200000}, 
  {"Ptable",0x10000},
  {"Fastboot",0x110000},
  {"Logo",0x130000},
  {"Kernel",0x30000},
  {"Kernel_R11",0x90000},
  {"VxWorks",0x40000},
  {"VxWorks_R11",0x220000},
  {"M3Image",0x50000},
  {"M3Image_R11",0x230000},
  {"DSP",0x60000},
  {"DSP_R11",0x240000},
  {"Nvdload",0x70000},
  {"Nvdload_R11",0x250000},
  {"Nvimg",0x80000},
  {"System",0x590000},
  {"System",0x100000},
  {"APP",0x570000}, 
  {"APP",0x5a0000}, 
  {"Oeminfo",0xa0000},
  {"CDROMISO",0xb0000},
  {"Oeminfo",0x550000},
  {"Oeminfo",0x510000},
  {"Oeminfo",0x1a0000},
  {"WEBUI",0x560000},
  {"WEBUI",0x5b0000},
  {"Wimaxcfg",0x170000},
  {"Wimaxcrf",0x180000},
  {"Userdata",0x190000},
  {"Online",0x1b0000},
  {"Online",0x5d0000},
  {"Online",0x5e0000},
  {"Ptable_R1",0x100},
  {"Bootloader_R1",0x101},
  {"Bootrom_R1",0x102},
  {"VxWorks_R1",0x550103},
  {"Fastboot_R1",0104},
  {"Kernel_R1",0x105},
  {"System_R1",0x107},
  {"Nvimage_R1",0x66},
  {"WEBUI_R1",0x113},
  {"APP_R1",0x109},
  {0,0}
};

for(j=0;pcodes[j].code != 0;j++) {
  if(pcodes[j].code == id) break;
}
if (pcodes[j].code != 0) strcpy(pname,pcodes[j].name); // имя найдено - копируем его в структуру
else sprintf(pname,"U%08x",id); // имя не найдено - подставляем псевдоимя Uxxxxxxxx в тупоконечном формате
}

//*******************************************************
//*  Поиск разделов в файле прошивки
//* 
//* возвращает число найденных разделов
//*******************************************************
int findparts(FILE* in, struct ptb_t* ptable) {

// структура описания заголовка раздела
struct {
 uint32_t magic;    //   0xa55aaa55
 uint32_t hdsize;   // размер заголовка
 uint32_t hdversion;
 uint8_t unlock[8];
 uint32_t code;     // тип раздела
 uint32_t psize;    // разме поля данных
 uint8_t date[16];
 uint8_t time[16];  // дата-время сборки прошивки
 uint8_t version[32];   // версия пршоивки
 uint16_t crc;   // CRC заголовка
 uint16_t blocksize;
} header; 
// Маркер начала заголовка раздела	      
const unsigned int dpattern=0xa55aaa55;
unsigned int i,npart=0;

// поиск начала цепочки разделов в файле
while (fread(&i,1,4,in) == 4) {
  if (i == dpattern) break;
}
if (feof(in)) {
  printf("\n В файле не найдены разделы - файл не содержит образа прошивки\n");
  exit(0);
}  

fseek(in,-4,SEEK_CUR); // отъезжаем на начало маркера
// поиск остальных разделов
while (fread(&i,1,4,in) == 4) {
  if (i != dpattern) {
    fseek(in,-3,SEEK_CUR);
    continue; // ищем разделитель
  }  
  // Выделяем параметры раздела
  fseek(in,-4,SEEK_CUR); // встаем на начало заголовка
  ptable[npart].hdoffset=ftell(in);  // позиция начала заголовка раздела
  fread(&header,1,sizeof(header),in); // читаем заголовок
  
  ptable[npart].hdsize=header.hdsize;  // размер заголовка
  ptable[npart].offset=ptable[npart].hdoffset+ptable[npart].hdsize; // смещение до тела раздела 
  ptable[npart].size=header.psize; // размер раздела
  ptable[npart].code=header.code; // тип раздела
    
  // Ищем символическое имя раздела по таблице 
  find_pname(ptable[npart].code,ptable[npart].pname);
  // Выводим информацию о версии проивки
  if (npart == 0) {
    printf("\n Версия прошивки: %s",header.version);
    printf("\n Дата сборки:     %s %s",header.date,header.time);
    printf("\n Заголовок: версия %i,  платформа %8.8s",header.hdversion,header.unlock);
  }  
  // увеличиваем счетчик разделов 
  npart++;
  // пропускаем тело раздела
  fseek(in,header.psize+header.hdsize-sizeof(header)-16,SEEK_CUR);
  }
return npart;
}
