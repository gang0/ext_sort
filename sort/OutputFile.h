//+----------------------------------------------------+
//| Выходной файл с буфером                            |
//+----------------------------------------------------+
class COutputFile
  {
private:
   CBinFile*         m_file;
   std::unique_ptr<char[]> m_data;
   std::unique_ptr<char[]> m_buffer;
   size_t            m_data_max;
   size_t            m_data_len;
   size_t            m_buffer_len;

public:
   //--- конструктор
                     COutputFile( const size_t buffer_size ) : m_file( nullptr ), m_data( new char[buffer_size] ), m_buffer( new char[buffer_size] ), m_data_max( buffer_size ), m_data_len( 0 ), m_buffer_len( 0 ) {}
                    ~COutputFile() { if( m_file != nullptr ) { m_file->Wait(); if( m_data_len > 0 ) m_file->Write( m_data.get(), &m_data_len ); } }
   //--- установка файла
   bool              SetFile( CBinFile* file );
   //--- запись элемента
   bool              WriteItem( boost::asio::io_service &io, int item );
  };
//+----------------------------------------------------+
//| Установка файла                                    |
//+----------------------------------------------------+
bool COutputFile::SetFile( CBinFile* file )
  {
   if( file == nullptr )
      return( false );
   m_file = file;
//--- 
   m_data_len = 0;
   m_buffer_len = 0;
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Запись элемента                                    |
//+----------------------------------------------------+
bool COutputFile::WriteItem( boost::asio::io_service &io, int item )
  {
//--- 
   if( m_file == nullptr )
      return( false );
//--- проверяем, что не выходим за границы буфера
   if( m_data_len > m_data_max )
      return( false );
//--- записываем элемент
   *(int*)&m_data[m_data_len] = item;
   m_data_len += sizeof( int );
//--- если буфер заполнился 
   if( m_data_len >= m_data_max )
     {
      //--- TODO: сделать нормально!
      if( m_buffer_len > 0 )
         m_file->Wait();
      m_data.swap( m_buffer );
      m_data_len = 0;
      m_buffer_len = m_data_max;
      m_file->WriteAsync( io, m_buffer.get(), &m_buffer_len );
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
