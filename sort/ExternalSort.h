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
   //--- ������� ����
   CBufferedAsyncFile m_input_file;
   //--- �������� ����
   CDataChunk<IntType> m_output_file;
   //--- ����� ������ � �������
   std::vector<std::string> m_chunks;

public:
   //--- �����������/����������
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_buffer_size( RAM_MAX / 4 ), m_io_service( io ), m_parallel_sort( io, concurrency_level ), m_input_file( io, m_buffer_size ), m_output_file( io, RAM_MAX / 16 /*TODO: ���������� ������*/) {};
   //--- ���������� ����� <input_file_name>, ��������� � ����� <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- ������������ ����� ����� ��� ���������� �����
   std::string       ChunkNextName();
   //--- ���������� �����
   void              ChunkAdd( const std::string &chunk_name );
   //--- ��������� �������� ���� �� ��������������� �����
   bool              Split( CBufferedAsyncFile &input_file );
   //--- ������� ��������������� ����� � �������� ����
   bool              Merge( CDataChunk<IntType> &output_file );
  };
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::Sort( std::string input_file_name, std::string output_file_name )
  {
//--- ��������� ������� ����
   if( !m_input_file.Open( input_file_name, CBinFile::MODE_READ ) )
      return;
//--- ��������� �������� ����
   if( !m_output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
      return;
//--- ��������� ������� ���� �� ������������� �����
   if( !Split( m_input_file ) )
      return;
//--- ������� ����� � �������� ����
   Merge( m_output_file );
  }
//+----------------------------------------------------+
//| ������������ ����� ����� ��� ���������� �����      |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
std::string CExternalSort<IntType, ParallelSort>::ChunkNextName()
  {
//--- ��������� ��� �� ������ ����� �������� �����, ���������� ����� ���������
   std::string chunk_name( m_input_file.Name() );
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
bool CExternalSort<IntType, ParallelSort>::Split( CBufferedAsyncFile &input_file )
  {
   CAutoTimer timer( "split" );
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
        {
         CAutoTimer timer( "sort" );
         if( !m_parallel_sort.Sort( (IntType*) unsorted_data.get(), (IntType*) ( unsorted_data.get() + data_size ), (IntType*) sorted_data.get() ) )
            return( false );
        }
      //--- ��������� ����� ����
      std::string chunk_name = ChunkNextName();
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
bool CExternalSort<IntType, ParallelSort>::Merge( CDataChunk<IntType> &output_file )
  {
   CAutoTimer timer( "Merge" );
   CDataChunk<IntType>::PtrArray data_chunks;
//--- TODO: ���������� �������� ��������!
   std::priority_queue<CDataChunkItem<IntType>> data_items;
//--- ��������� ����� � ���������������� �������
   for( const auto &chunk_name : m_chunks )
     {
      //--- TODO: ��������� ������ �����
      CDataChunk<IntType>::Ptr chunk( new CDataChunk<IntType>( m_io_service, RAM_MAX / 16 ) );
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
