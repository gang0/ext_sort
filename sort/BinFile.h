//+----------------------------------------------------+
//| Бинарный файл                                      |
//+----------------------------------------------------+
class CBinFile
  {
public:
   enum EnMode
     {
      MODE_NONE = 0,
      MODE_READ = 0x01,
      MODE_WRITE = 0x02,
      MODE_TEMP = 0x04,
      MODE_RW = MODE_READ | MODE_WRITE,
     };

private:
   //--- атрибуты файла
   std::string       m_name;
   int               m_mode;
   //--- поток
   FILE*             m_stream;

public:
   //--- конструктор/деструктор
                     CBinFile();
                    ~CBinFile();
   //--- атрибуты
   std::string       Name() { return( m_name ); }
   int               Mode() { return( m_mode ); }
   //--- открытие/закрытие файла
   bool              Open( const std::string &name, const int mode);
   void              Close();
   //--- чтение/запись из файла
   size_t            Read( char* buffer, const size_t buffer_size );
   size_t            Write( const char* buffer, const size_t buffer_size );
   //--- перемещение указателя в начало
   void              Seek( size_t offset, int method ) { if( m_stream != NULL ) fseek( m_stream, (long) offset, method ); }
   //--- удаление файла
   void              Remove() { Close(); if( !m_name.empty() ) remove( m_name.c_str() ); }

private:
   const CBinFile&   operator=( const CBinFile &bin_file );
  };
typedef vector<CBinFile*> CBinFilePtrArray;
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
CBinFile::CBinFile() : m_mode( 0 ), m_stream( nullptr )
  {
  }
//+----------------------------------------------------+
//| Деструктор                                         |
//+----------------------------------------------------+
CBinFile::~CBinFile()
  {
   Close();
  }
//+----------------------------------------------------+
//| Открытие файла                                     |
//+----------------------------------------------------+
bool CBinFile::Open( const std::string &name, const int mode )
  {
   Close();
//--- запоминаем атрибуты
   m_name = name;
   m_mode = mode;
//--- открываем файл в нужном режиме
   const char* mode_str = nullptr;
   if( mode & MODE_WRITE )
      if( mode & MODE_READ ) mode_str = "w+b";
      else
         mode_str = "wSb";
   else
      mode_str = "rSb";
   errno_t err = fopen_s( &m_stream, name.c_str(), mode_str);
   if( err != 0 || m_stream == nullptr )
     {
      cerr << "failed to open file " << name << " (" << err << ")" << endl;
      return( false );
     }
//--- 
   return( true );
  }
//+----------------------------------------------------+
//| Закрытие файла                                     |
//+----------------------------------------------------+
void CBinFile::Close()
  {
   if( m_stream != nullptr )
     {
      fclose( m_stream );
      m_stream = nullptr;
      if( m_mode & MODE_TEMP )
         Remove();
     }
  }
//+----------------------------------------------------+
//| Чтение из файла                                    |
//+----------------------------------------------------+
size_t CBinFile::Read( char* buffer, const size_t buffer_size )
  {
   if( buffer == nullptr || buffer_size == 0 )
      return( 0 );
   if( m_stream == nullptr )
      return( 0 );
   return( fread( buffer, 1, buffer_size, m_stream ) );
  }
//+----------------------------------------------------+
//| Запись в файл                                      |
//+----------------------------------------------------+
size_t CBinFile::Write( const char* buffer, const size_t buffer_size )
  {
   if( buffer == nullptr || buffer_size == 0 )
      return( 0 );
   if( m_stream == nullptr )
      return( 0 );
   return( fwrite( buffer, 1, buffer_size, m_stream ) );
  }
//+----------------------------------------------------+
