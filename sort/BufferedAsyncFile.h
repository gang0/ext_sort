//+----------------------------------------------------+
//| Класс файла с асинхронной буферизацией             |
//+----------------------------------------------------+
class CBufferedAsyncFile
  {
public:
   //--- определения
   typedef unique_ptr<CBufferedAsyncFile> Ptr;
   typedef std::vector<Ptr> PtrArray;

private:
   //--- файл
   CBinFile          m_file;
   //--- асинхронный сервис
   boost::asio::io_service &m_io_service;
   //--- буфер
   std::unique_ptr<char[]> m_buffer;
   const size_t      m_buffer_size;
   //--- размер данных в буфере
   size_t            m_data_size;
   //--- управление асинхронным IO
   bool              m_completed;
   boost::mutex      m_completed_sync;
   boost::condition_variable m_completed_cond;

public:
                     CBufferedAsyncFile( boost::asio::io_service &io, const size_t buffer_size );
                    ~CBufferedAsyncFile();
   //--- открытие/закрытие файла
   bool              Open( const std::string &file_name, const int mode );
   void              Close();
   //--- чтение буфера
   size_t            Read( std::unique_ptr<char[]> &buffer );
   //--- запись буфера
   void              Write( std::unique_ptr<char[]> &buffer, size_t data_size );

private:
   //--- уведомление/ожидание завершения асинхронной операции
   void              AsyncComplete();
   void              AsyncWait();
   //--- асинхронное чтение
   void              ReadAsync();
   void              ReadAsyncHandler();
   //--- асинхронная запись
   void              WriteAsync();
   void              WriteAsyncHandler();
  };
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
CBufferedAsyncFile::CBufferedAsyncFile( boost::asio::io_service &io, const size_t buffer_size ) : m_io_service( io ), m_buffer( new char[buffer_size] ), m_buffer_size( buffer_size ), m_data_size( 0 ), m_completed( true )
  {
  }
//+----------------------------------------------------+
//| Деструктор                                         |
//+----------------------------------------------------+
CBufferedAsyncFile::~CBufferedAsyncFile()
  {
   Close();
  }
//+----------------------------------------------------+
//| Открытие файла                                     |
//+----------------------------------------------------+
bool CBufferedAsyncFile::Open( const std::string &file_name, const int mode )
  {
   Close();
//--- открываем файл
   if( !m_file.Open( file_name, mode ) )
      return( false );
//--- если файл открыт для чтения заполняем буфер
   if( mode & CBinFile::MODE_READ )
      ReadAsync();
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Закрытие файла                                     |
//+----------------------------------------------------+
void CBufferedAsyncFile::Close()
  {
//--- должны дождаться завершения любой асинхронной операцией перед закрытием файла
   AsyncWait();
   m_file.Close();
  }
//+----------------------------------------------------+
//| Чтение буфера                                      |
//+----------------------------------------------------+
size_t CBufferedAsyncFile::Read( std::unique_ptr<char[]> &buffer )
  {
//--- ждем завершения асинхронной операции
   AsyncWait();
//--- меняем буферы
   buffer.swap( m_buffer );
   size_t data_size = m_data_size;
//--- запускаем асинхронную операцию
   ReadAsync();
//--- возвращаем длину буфера
   return( data_size );
  }
//+----------------------------------------------------+
//| Запись буфера                                      |
//+----------------------------------------------------+
void CBufferedAsyncFile::Write( std::unique_ptr<char[]> &buffer, size_t data_size )
  {
//--- ждем завершения асинхронной операции
   AsyncWait();
//--- меняем буферы
   m_buffer.swap( buffer );
   m_data_size = data_size;
//--- запускаем асинхронную операцию
   WriteAsync();
  }
//+----------------------------------------------------+
//| Уведомление о завершении асинхронной операции      |
//+----------------------------------------------------+
void CBufferedAsyncFile::AsyncComplete()
  {
   m_completed_sync.lock();
   m_completed = true;
   m_completed_sync.unlock();
   m_completed_cond.notify_all();
  }
//+----------------------------------------------------+
//| Ожидание завершения асинхронной операции           |
//+----------------------------------------------------+
void CBufferedAsyncFile::AsyncWait()
  {
   boost::unique_lock<boost::mutex> lock( m_completed_sync );
//--- TODO: ожидать по таймауту
   if( !m_completed )
      m_completed_cond.wait( lock );
  }
//+----------------------------------------------------+
//| Асинхронное чтение                                 |
//+----------------------------------------------------+
void CBufferedAsyncFile::ReadAsync()
  {
   m_completed = false;
   m_data_size = 0;
   m_io_service.post( boost::bind( &CBufferedAsyncFile::ReadAsyncHandler, this ) );
  }
//+----------------------------------------------------+
//| Обработчик асинхронного чтения                     |
//+----------------------------------------------------+
void CBufferedAsyncFile::ReadAsyncHandler()
  {
//--- читаем блок данных в буфер
   m_data_size = m_file.Read( m_buffer.get(), m_buffer_size );
//--- уведомляем о завершении операции
   AsyncComplete();
  }
//+----------------------------------------------------+
//| Асинхронная запись                                 |
//+----------------------------------------------------+
void CBufferedAsyncFile::WriteAsync()
  {
   m_completed = false;
   m_io_service.post( boost::bind( &CBufferedAsyncFile::WriteAsyncHandler, this ) );
  }
//+----------------------------------------------------+
//| Обработчик асинхронной записи                      |
//+----------------------------------------------------+
void CBufferedAsyncFile::WriteAsyncHandler()
  {
//--- записываем блок данных из буфера
   m_file.Write( m_buffer.get(), m_data_size );
//--- уведомляем о завершении операции
   AsyncComplete();
  }
//+----------------------------------------------------+
