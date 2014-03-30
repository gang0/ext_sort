//+----------------------------------------------------+
//| ������� ����������                                 |
//+----------------------------------------------------+
template<class IntType = unsigned, class ParallelSort = CParallelQuickSort<IntType>>
class CExternalSort
  {
private:
   //--- ������ ������������� �������
   const size_t      m_buffer_size;
   //--- ����������� ������
   boost::asio::io_service &m_io_service;
   //--- ������������ ����������
   ParallelSort      m_parallel_sort;
   //--- ����� ������ � �������
   std::vector<std::string> m_chunks;

public:
   //--- �����������/����������
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_buffer_size( RAM_MAX / 4 ), m_io_service( io ), m_parallel_sort( io, concurrency_level ) {};
   //--- ���������� ����� <input_file_name>, ��������� � ����� <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- ������������ ����� ����� ��� ���������� �����
   std::string       ChunkNextName( const std::string &input_file_name );
   //--- ���������� �����
   void              ChunkAdd( const std::string &chunk_name );
   //--- ��������� �������� ���� �� ��������������� �����
   bool              Split( std::string &input_file_name );
   //--- ������� ��������������� ����� � �������� ����
   bool              Merge( std::string &output_file_name );
  };
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::Sort( std::string input_file_name, std::string output_file_name )
  {
//--- ��������� ������� ���� �� ������������� �����
   if( !Split( input_file_name ) )
      return;
//--- ������� ����� � �������� ����
   Merge( output_file_name );
  }
//+----------------------------------------------------+
//| ������������ ����� ����� ��� ���������� �����      |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
std::string CExternalSort<IntType, ParallelSort>::ChunkNextName( const std::string &input_file_name )
  {
//--- ��������� ��� �� ������ ����� �������� �����, ���������� ����� ���������
   std::string chunk_name( input_file_name );
   chunk_name.append( "_" );
//--- ��������� ��������� ������
   std::ostringstream chunk_index;
   chunk_index.width( 3 );
   chunk_index.fill( '0' );
   chunk_index << m_chunks.size();
//--- ��������� ������
   chunk_name.append( chunk_index.str() );
   return( chunk_name );
  }
//+----------------------------------------------------+
//| ���������� �����                                   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::ChunkAdd( const std::string &chunk_name )
  {
   m_chunks.push_back( chunk_name );
  }
//+----------------------------------------------------+
//| ��������� �������� ���� �� ��������������� �����   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Split( std::string &input_file_name )
  {
   CAutoTimer timer( "input file splitting to sorted chunks" );
//--- �������� ����
   CBufferedAsyncFile input_file( m_io_service, m_buffer_size );
   if( !input_file.Open( input_file_name, CBinFile::MODE_READ ) )
     {
      std::cerr << "failed to open input file " << input_file_name << std::endl;
      return( false );
     }
//--- ��������� ��� ������ ������
   CBufferedAsyncFile chunk_file( m_io_service, m_buffer_size );
   int chunk_file_index = 0;
//--- ����� ��� ��������� � ��������������� ������
   std::unique_ptr<char[]> unsorted_data( new char[m_buffer_size] );
   std::unique_ptr<char[]> sorted_data( new char[m_buffer_size] );
//--- ������ ������ ������
   size_t data_size = input_file.Read( unsorted_data );
   while( data_size > 0 )
     {
      //--- �������� ������ ������
      if( data_size > m_buffer_size || data_size % sizeof( IntType ) != 0 )
        {
         std::cerr << "invalid size of data" << std::endl;
         return( false );
        }
      //--- ���������
      if( !m_parallel_sort.Sort( (IntType*) unsorted_data.get(), (IntType*) ( unsorted_data.get() + data_size ), (IntType*) sorted_data.get() ) )
         return( false );
      //--- ��������� ����� ����
      std::string chunk_name = ChunkNextName( input_file.Name() );
      if( chunk_file.Open( chunk_name, CBinFile::MODE_WRITE ) )
         ChunkAdd( chunk_name );
      else
         return( false );
      //--- ���������� ���������� ���� � ����
      chunk_file.Write( sorted_data, data_size );
      //--- ������ ��������� ������
      data_size = input_file.Read( unsorted_data );
     }
   return( true );
  }
//+----------------------------------------------------+
//| ������� ��������������� ����� � �������� ����      |
//+----------------------------------------------------+
template<class IntType,class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Merge(  std::string &output_file_name )
  {
   CAutoTimer timer( "merging sorted chunks to output file" );
//--- �������� ����
   CDataChunk<IntType> output_file( m_io_service, RAM_MAX / 4 );
   if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
     {
      std::cerr << "failed to open output file " << output_file_name << std::endl;
      return( false );
     }
//--- ��������������� �����
   CDataChunk<IntType>::PtrArray data_chunks;
//--- �������� ���� ��������� � ���������� � �������
   std::priority_queue<CDataChunkItem<IntType>, std::vector<CDataChunkItem<IntType>>, std::greater<CDataChunkItem<IntType>>> data_items;
//--- ��������� ����� � ���������������� �������
   for( const auto &chunk_name : m_chunks )
     {
      CDataChunk<IntType>::Ptr chunk( new CDataChunk<IntType>( m_io_service, ( RAM_MAX / 4 ) / m_chunks.size() ) );
      if( !chunk->Open( chunk_name, CBinFile::MODE_READ | CBinFile::MODE_TEMP ) )
        {
         std::cerr << "failed to open chunk file " << chunk_name << std::endl;
         return( false );
        }
      data_chunks.push_back( chunk );
     }
//--- ��������� ���� �� ������ ��������� ������� �����
   for( const auto &chunk : data_chunks )
     {
      CDataChunkItem<IntType> chunk_item( chunk );
      if( chunk_item.Next() )
         data_items.push( chunk_item );
     }
//--- �� ������ �������� ����� ������� ������ ����
   while( !data_items.empty() )
     {
      CDataChunkItem<IntType> item = data_items.top();
      data_items.pop();
      if( !output_file.Write( item ) )
        {
         std::cerr << "failed to write to output file" << std::endl;
         return( false );
        }
      if( item.Next() )
         data_items.push( item );
     }
   return( true );
  }
//+----------------------------------------------------+
