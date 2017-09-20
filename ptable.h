

// структура описания заголовка раздела
#ifndef WIN32
struct __attribute__ ((__packed__)) pheader {
#else
#pragma pack(push,1)
struct pheader {
#endif
 int32_t magic;    //   0xa55aaa55
 uint32_t hdsize;   // размер заголовка
 uint32_t hdversion;
 uint8_t unlock[8];
 uint32_t code;     // тип раздела
 uint32_t psize;    // разме поля данных
 uint8_t date[16];
 uint8_t time[16];  // дата-время сборки прошивки
 uint8_t version[32];   // версия пршоивки
 uint16_t crc;   // CRC заголовка
 uint32_t blocksize;  // размер блока CRC образа прошивки
}; 
#ifdef WIN32
#pragma pack(pop)
#endif


// Структура описания таблицы разделов

struct ptb_t{
  unsigned char pname[20];    // буквенное имя раздела
  struct pheader hd;  // образ заголовка
  uint16_t* csumblock; // блок контрольных сумм
  uint8_t* pimage;   // образ раздела
  uint32_t offset;   // смещение в файле до начала раздела
  uint32_t zflag;     // признак сжатого раздела  
  uint8_t ztype;    // тип сжатия
};

//******************************************************
//*  Внешние массивы для хранения таблицы разделов
//******************************************************
extern struct ptb_t ptable[];
extern int npart; // число разделов в таблице

extern uint32_t errflag;

int findparts(FILE* in);
void  find_pname(unsigned int id,unsigned char* pname);
void findfiles (char* fdir);
uint32_t psize(int n);

extern int dload_id;
