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
   int               m_mode;
   std::string       m_name;
   //--- поток
   FILE*             m_stream;
   //--- синхронизация доступа к потоку
   boost::mutex      m_stream_sync;
   //--- управление асинхронными операциями
   bool              m_handler_complete;
   boost::mutex      m_handler_complete_sync;
   boost::condition_variable m_handler_complete_sync_event;

public:
   //--- конструктор/деструктор
                     CBinFile();
                    ~CBinFile();
   //--- открытие/закрытие файла
   bool              Open( const string &name, const int mode);
   void              Close();
   //--- чтение/запись из файла
   bool              Read( char* buffer, size_t* size );
   bool              Write( const char* buffer, size_t* size );
   //--- асинхронные чтение/запись
   bool              ReadAsync( boost::asio::io_service &io, char* buffer, size_t* size );
   bool              WriteAsync( boost::asio::io_service &io, const char* buffer, size_t* size );
   void              Wait();
   //--- перемещение указателя в начало
   void              SeekBegin() { if( m_stream != NULL ) fseek( m_stream, 0, SEEK_SET ); }
   //--- удаление файла
   void              Remove() { Close(); if( !m_name.empty() ) remove( m_name.c_str() ); }

private:
   const CBinFile&   operator=( const CBinFile &bin_file );
   //--- обработка асинхронного чтения/записи
   void              ReadAsyncHandler( char* buffer, size_t* size );
   void              WriteAsyncHandler( const char* buffer, size_t* size );
   void              AsyncHandlerCompleted() { m_handler_complete_sync.lock(); m_handler_complete = true; m_handler_complete_sync.unlock(); m_handler_complete_sync_event.notify_all(); }
  };
typedef vector<CBinFile*> CBinFilePtrArray;
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
CBinFile::CBinFile() : m_mode( 0 ), m_stream( nullptr ), m_handler_complete( false )
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
bool CBinFile::Open( const string &name, const int mode )
  {
   Close();
//--- запоминаем атрибуты
   m_name = name;
   m_mode = mode;
//--- открываем файл в нужном режиме
   const char* mode_str = NULL;
   if( mode & MODE_WRITE )
      if( mode & MODE_READ ) mode_str = "w+b";
      else
         mode_str = "wSb";
   else
      mode_str = "rSb";
   errno_t err = fopen_s( &m_stream, name.c_str(), mode_str);
   if( err != 0 || m_stream == NULL )
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
   if( m_stream != NULL )
     {
      fclose( m_stream );
      m_stream = NULL;
      if( m_mode & MODE_TEMP )
         Remove();
     }
  }
//+----------------------------------------------------+
//| Чтение из файла                                    |
//+----------------------------------------------------+
bool CBinFile::Read( char* buffer, size_t* size )
  {
   if( buffer == NULL || size == NULL )
      return( false );
   if( m_stream == NULL )
      return( false );
   *size = fread( buffer, 1, *size, m_stream );
   return( true );
  }
//+----------------------------------------------------+
//| Запись в файл                                      |
//+----------------------------------------------------+
bool CBinFile::Write( const char* buffer, size_t* size )
  {
   if( buffer == NULL || size == NULL )
      return( false );
   if( m_stream == NULL )
      return( false );
   *size = fwrite( buffer, 1, *size, m_stream);
   return( true );
  }
//+----------------------------------------------------+
//| Асинхронное чтение                                 |
//+----------------------------------------------------+
bool CBinFile::ReadAsync( boost::asio::io_service &io, char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return( false );
//--- TODO: нужно ли проверять значение метода post
   io.post( boost::bind( &CBinFile::ReadAsyncHandler, this, buffer, size ) );
   return( true );
  }
//+----------------------------------------------------+
//| Асинхронная запись                                 |
//+----------------------------------------------------+
bool CBinFile::WriteAsync( boost::asio::io_service &io, const char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return( false );
//--- TODO: проверять возврат метода post
   io.post( boost::bind( &CBinFile::WriteAsyncHandler, this, buffer, size ) );
   return( true );
  }
//+----------------------------------------------------+
//| Ожидание завершения асинхронной операции           |
//+----------------------------------------------------+
void CBinFile::Wait()
  {
   boost::unique_lock<boost::mutex> lock( m_handler_complete_sync );
//--- TODO: перейти на wait_for
   if( !m_handler_complete )
     {
      m_handler_complete_sync_event.wait( lock );
      m_handler_complete = false;
     }
  }
//+----------------------------------------------------+
//| Обработчик асинхронного чтения                     |
//+----------------------------------------------------+
void CBinFile::ReadAsyncHandler( char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return;
//--- читаем данные
   boost::lock_guard<boost::mutex> lock( m_stream_sync );
   Read( buffer, size );
   AsyncHandlerCompleted();
  }
//+----------------------------------------------------+
//|                                                    |
//+----------------------------------------------------+
void CBinFile::WriteAsyncHandler( const char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return;
//--- пишем данные
   boost::lock_guard<boost::mutex> lock( m_stream_sync );
   Write( buffer, size );
   AsyncHandlerCompleted();
  }
//+----------------------------------------------------+
