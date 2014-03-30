//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+
//+----------------------------------------------------+
//| Класс элемента чанка                               |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CDataChunkItem;
//+----------------------------------------------------+
//| Класс чанка с промежуточным буфером                |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CDataChunk
  {
public:
   //--- определения
   typedef std::shared_ptr<CDataChunk<IntType>> Ptr;
   typedef std::vector<Ptr> PtrArray;

private:
   //--- файл с асинхронной буферизацией
   CBufferedAsyncFile m_file;
   //--- данные
   std::unique_ptr<char[]> m_data;
   //--- максимальный размер данных
   const size_t      m_data_max;
   //--- текущий размер данных
   size_t            m_data_len;
   //--- текущий элемент чтения
   size_t            m_data_current;

public:
                     CDataChunk( boost::asio::io_service &io, const size_t buffer_size );
                    ~CDataChunk();
   //--- открытие/закрытие файла
   bool              Open( const std::string &file_name, const int mode );
   void              Close();
   //--- получение элемента из чанка
   bool              Read( CDataChunkItem<IntType> &item );
   //--- запись элемента в чанк
   bool              Write( CDataChunkItem<IntType> item );
  };
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
template<class IntType>
CDataChunk<IntType>::CDataChunk( boost::asio::io_service &io, const size_t buffer_size ) : m_file( io, buffer_size ), m_data( new char[buffer_size] ), m_data_max( buffer_size ), m_data_len( 0 ), m_data_current( 0 )
  {
  }
//+----------------------------------------------------+
//| Деструктор                                         |
//+----------------------------------------------------+
template<class IntType>
CDataChunk<IntType>::~CDataChunk()
  {
   Close();
  }
//+----------------------------------------------------+
//| Открытие файла                                     |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Open( const std::string &file_name, const int mode )
  {
   Close();
//--- открываем файл
   m_file.Open( file_name, mode );
   return( true );
  }
//+----------------------------------------------------+
//| Закрытие файла                                     |
//+----------------------------------------------------+
template<class IntType>
void CDataChunk<IntType>::Close()
  {
//--- если файл открыт для записи, запишем то, что есть в буфере
   if( ( m_file.Mode() & CBinFile::MODE_WRITE ) && m_data_len > 0 )
      m_file.Write( m_data, m_data_len );
//--- закрываем файл
   m_file.Close();
//--- сбрасываем свойства
   m_data_len = 0;
   m_data_current = 0;
  }
//+----------------------------------------------------+
//| Получение элемента из чанка                        |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Read( CDataChunkItem<IntType> &item )
  {
//--- если нет данных или данные закончились
   if( m_data_len == 0 || m_data_current == m_data_len )
     {
      //--- читаем из файла следующую порцию
      m_data_current = 0;
      m_data_len = m_file.Read( m_data );
      //--- проверим размер считанных данных
      if( m_data_len == 0 || m_data_len > m_data_max || m_data_len % sizeof( IntType ) != 0 )
         return( false );
     }
//--- получаем элемент
   item.m_item = *(IntType*) &m_data[m_data_current];
   m_data_current += sizeof( IntType );
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Запись элемента в чанк                             |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Write( CDataChunkItem<IntType> item )
  {
//--- проверим, что есть место в буфере
   if( m_data_len >= m_data_max )
      return( false );
//--- записываем элемент
   *(IntType*) &m_data[m_data_len] = item.m_item;
   m_data_len += sizeof( IntType );
//--- если заполнили буфер
   if( m_data_len >= m_data_max )
     {
      //--- пишем буфер
      m_file.Write( m_data, m_data_len );
      m_data_len = 0;
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Элемент чанка                                      |
//+----------------------------------------------------+
template<class IntType>
class CDataChunkItem
  {
   friend class CDataChunk<IntType>;

private:
   IntType           m_item;
   std::weak_ptr<CDataChunk<IntType>> m_chunk;

public:
                     CDataChunkItem( std::shared_ptr<CDataChunk<IntType>> chunk );
   bool              operator>( const CDataChunkItem<IntType> &chunk_item ) const;
   //--- следующее значение
   bool              Next();
  };
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
template<class IntType>
CDataChunkItem<IntType>::CDataChunkItem( std::shared_ptr<CDataChunk<IntType>> chunk ) : m_item( 0 ), m_chunk( chunk )
  {
  }
//+----------------------------------------------------+
//| Оператор больше                                    |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunkItem<IntType>::operator> ( const CDataChunkItem<IntType> &chunk_item ) const
  {
   return( this->m_item > chunk_item.m_item );
  }
//+----------------------------------------------------+
//| Следующее значение                                 |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunkItem<IntType>::Next()
  {
   if( m_chunk.expired() )
      return( false );
   std::shared_ptr<CDataChunk<IntType>> chunk( m_chunk.lock() );
   return( chunk->Read( *this ) );
  }
//+----------------------------------------------------+
