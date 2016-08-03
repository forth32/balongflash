// Структура описания таблицы разделов

struct ptb_t{
  unsigned int offset;    // позиция образа раздела
  unsigned int hdoffset;  // позиция заголовка раздела
  unsigned int size;      // размер образа раздела
  unsigned int hdsize;    // размер заголовка раздела
  unsigned int code;      // ID раздела
  unsigned char pname[20];    // буквенное имя раздела
  unsigned char filename[50]; // имя файла, соответствующее разделу
};

int findparts(FILE* in, struct ptb_t* ptable);
void  find_pname(unsigned int id,unsigned char* pname);