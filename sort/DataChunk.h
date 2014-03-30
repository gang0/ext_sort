//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+
//+----------------------------------------------------+
//| ����� �������� �����                               |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CDataChunkItem;
//+----------------------------------------------------+
//| ����� ����� � ������������� �������                |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CDataChunk
  {
public:
   //--- �����������
   typedef std::shared_ptr<CDataChunk<IntType>> Ptr;
   typedef std::vector<Ptr> PtrArray;

private:
   //--- ���� � ����������� ������������
   CBufferedAsyncFile m_file;
   //--- ������
   std::unique_ptr<char[]> m_data;
   //--- ������������ ������ ������
   const size_t      m_data_max;
   //--- ������� ������ ������
   size_t            m_data_len;
   //--- ������� ������� ������
   size_t            m_data_current;

public:
                     CDataChunk( boost::asio::io_service &io, const size_t buffer_size );
                    ~CDataChunk();
   //--- ��������/�������� �����
   bool              Open( const std::string &file_name, const int mode );
   void              Close();
   //--- ��������� �������� �� �����
   bool              Read( CDataChunkItem<IntType> &item );
   //--- ������ �������� � ����
   bool              Write( CDataChunkItem<IntType> item );
  };
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
template<class IntType>
CDataChunk<IntType>::CDataChunk( boost::asio::io_service &io, const size_t buffer_size ) : m_file( io, buffer_size ), m_data( new char[buffer_size] ), m_data_max( buffer_size ), m_data_len( 0 ), m_data_current( 0 )
  {
  }
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
template<class IntType>
CDataChunk<IntType>::~CDataChunk()
  {
   Close();
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Open( const std::string &file_name, const int mode )
  {
   Close();
//--- ��������� ����
   m_file.Open( file_name, mode );
   return( true );
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
template<class IntType>
void CDataChunk<IntType>::Close()
  {
//--- ���� ���� ������ ��� ������, ������� ��, ��� ���� � ������
   if( ( m_file.Mode() & CBinFile::MODE_WRITE ) && m_data_len > 0 )
      m_file.Write( m_data, m_data_len );
//--- ��������� ����
   m_file.Close();
//--- ���������� ��������
   m_data_len = 0;
   m_data_current = 0;
  }
//+----------------------------------------------------+
//| ��������� �������� �� �����                        |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Read( CDataChunkItem<IntType> &item )
  {
//--- ���� ��� ������ ��� ������ �����������
   if( m_data_len == 0 || m_data_current == m_data_len )
     {
      //--- ������ �� ����� ��������� ������
      m_data_current = 0;
      m_data_len = m_file.Read( m_data );
      //--- �������� ������ ��������� ������
      if( m_data_len == 0 || m_data_len > m_data_max || m_data_len % sizeof( IntType ) != 0 )
         return( false );
     }
//--- �������� �������
   item.m_item = *(IntType*) &m_data[m_data_current];
   m_data_current += sizeof( IntType );
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| ������ �������� � ����                             |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunk<IntType>::Write( CDataChunkItem<IntType> item )
  {
//--- ��������, ��� ���� ����� � ������
   if( m_data_len >= m_data_max )
      return( false );
//--- ���������� �������
   *(IntType*) &m_data[m_data_len] = item.m_item;
   m_data_len += sizeof( IntType );
//--- ���� ��������� �����
   if( m_data_len >= m_data_max )
     {
      //--- ����� �����
      m_file.Write( m_data, m_data_len );
      m_data_len = 0;
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| ������� �����                                      |
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
   //--- ��������� ��������
   bool              Next();
  };
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
template<class IntType>
CDataChunkItem<IntType>::CDataChunkItem( std::shared_ptr<CDataChunk<IntType>> chunk ) : m_item( 0 ), m_chunk( chunk )
  {
  }
//+----------------------------------------------------+
//| �������� ������                                    |
//+----------------------------------------------------+
template<class IntType>
bool CDataChunkItem<IntType>::operator> ( const CDataChunkItem<IntType> &chunk_item ) const
  {
   return( this->m_item > chunk_item.m_item );
  }
//+----------------------------------------------------+
//| ��������� ��������                                 |
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
