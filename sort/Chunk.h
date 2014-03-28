//+----------------------------------------------------+
//| Класс чанка с промежуточным буфером                |
//+----------------------------------------------------+
class CChunk
  {
private:
   CBinFile*         m_file;
   std::unique_ptr<char[]> m_data;
   std::unique_ptr<char[]> m_buffer;
   size_t            m_data_max;
   size_t            m_data_len;
   size_t            m_buffer_len;
   size_t            m_data_current;

public:
   //--- конструктор
                     CChunk( const size_t buffer_size ) : m_file( nullptr ), m_data( new char[buffer_size] ), m_buffer( new char[buffer_size] ), m_data_max( buffer_size ), m_data_len( 0 ), m_buffer_len( 0 ), m_data_current( 0 ) {}
   //--- установка файла чанка
   bool              SetFile( CBinFile* file );
   //--- инициализация
   bool              Initialize( boost::asio::io_service &io );
   void              Wait() { m_file->Wait(); }
   //--- получение следующего элемента из чанка
   bool              NextItem( boost::asio::io_service &io, int &item );
  };
//+----------------------------------------------------+
//| Установка файла чанка                              |
//+----------------------------------------------------+
bool CChunk::SetFile( CBinFile* file )
  {
   if( file == nullptr )
      return( false );
   m_file = file;
//--- устанавливаем курсор на начало файла и читаем буфер
   m_file->SeekBegin();
//--- 
   m_data_len = 0;
   m_buffer_len = 0;
   m_data_current = 0;
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Инициализация                                      |
//+----------------------------------------------------+
bool CChunk::Initialize( boost::asio::io_service &io )
  {
   if( m_file == nullptr )
      return( false );
   m_data_len = m_data_max;
   return( m_file->ReadAsync( io, (char*)m_data.get(), &m_data_len ) );
  }
//+----------------------------------------------------+
//| Получение следующего элемента из чанка             |
//+----------------------------------------------------+
bool CChunk::NextItem( boost::asio::io_service &io, int &item )
  {
//--- если дошли до конца данных, уходим
   if( m_data_current == m_data_len )
      return( false );
//--- если начало данных, асинхронно читаем следующий буфер
   if( m_data_current == 0 )
     {
      m_buffer_len = m_data_max;
      m_file->ReadAsync( io, m_buffer.get(), &m_buffer_len );
     }
//--- читаем очередной элемент
   item = *(int*)&m_data[m_data_current];
   m_data_current += sizeof( int );
//--- если дошли до конца данных меняем буферы
   if( m_data_current >= m_data_len )
     {
      m_file->Wait();
      m_data.swap( m_buffer );
      m_data_len = m_buffer_len;
      m_data_current = 0;
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
