// Процедуры работы с таблицей разделов

#include <stdio.h>
#include <string.h>
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

// Маркер начала заголовка раздела	      
const unsigned int dpattern=0xa55aaa55;
unsigned int i,npart=0;
char buf[200];

while (fread(&i,1,4,in) == 4) {
  if (i != dpattern) continue; // ищем разделитель
    
  // Выделяем параметры раздела
  ptable[npart].hdoffset=ftell(in);  // позиция начала заголовка раздела
  fread(buf,1,96,in); // заголовок
  ptable[npart].hdsize=*((unsigned int*)&buf[0])-4;  // размер заголовка
  ptable[npart].offset=ptable[npart].hdoffset+ptable[npart].hdsize; // смещение до тела раздела 
  ptable[npart].size=*((unsigned int*)&buf[20]); // размер раздела
  ptable[npart].code=*((unsigned int*)&buf[16]); // тип раздела
    
  // Ищем символическое имя раздела по таблице 
  find_pname(ptable[npart].code,ptable[npart].pname);
  // увеличиваем счетчик разделов 
  npart++; 
  }
return npart;
}
