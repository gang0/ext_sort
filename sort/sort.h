//+----------------------------------------------------+
//| ������������ ����������                            |
//+----------------------------------------------------+
class CParallelSort
  {
private:
   //--- ����������� ������
   boost::asio::io_service &m_io_service;
   //--- ������� ������������
   const int         m_concurrency_level;
   //--- ���������� ��������������� ������
   int               m_chunks_sorted;
   boost::mutex      m_chunks_sorted_sync;
   boost::condition_variable m_chunks_sorted_cond;

public:
                     CParallelSort( boost::asio::io_service &io, const int concurrency_level );
   //--- ���������� ������
   bool              Sort( boost::asio::io_service &io, int* begin, int* end, int* result);
   //--- ������������ ���������� ������
   void              SerialSort( int* chunk_begin, int* chunk_end );
   void              SerialSortComplete() { m_chunks_sorted_sync.lock(); m_chunks_sorted++; m_chunks_sorted_sync.unlock(); m_chunks_sorted_cond.notify_all(); }
   //--- �������� ���������� ������������ ����������
   void              SerialSortWait( const int chunks_count );
  };
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
CParallelSort::CParallelSort( boost::asio::io_service &io, const int concurrency_level ) : m_io_service( io ), m_concurrency_level( concurrency_level ), m_chunks_sorted( 0 )
  {
  }
//+----------------------------------------------------+
//| ���������� ������                                  |
//+----------------------------------------------------+
bool CParallelSort::Sort( boost::asio::io_service &io, int* begin, int* end, int* result )
  {
   if( begin == nullptr || end == nullptr || result == nullptr || begin > end )
      return( false );
//--- ��������� ������ �� ��������� ����������� � ����������� �� ������� ������������
   const int chunks_count = m_concurrency_level;
   std::vector<int*> bound;
   for( int bound_index = 0; bound_index < chunks_count; bound_index++ )
      bound.push_back( begin + bound_index * ( end - begin ) / chunks_count );
   bound.push_back( end );
//--- ��������� ������������ ���������� �����������
   for( int chunk_index = 0; chunk_index < chunks_count; chunk_index++ )
      io.post( boost::bind( &CParallelSort::SerialSort, this, bound[chunk_index], bound[chunk_index + 1] ) );
   SerialSortWait( chunks_count );
//--- ������� ��������������� ����������
   std::vector<int*> chunk( bound );
   chunk.pop_back();
   for( int* current = result; current < result + ( end - begin ); current++ )
     {
      int chunk_min = -1;
      for( int chunk_index = 0; chunk_index < chunk.size(); chunk_index++ )
         if( chunk[chunk_index] < bound[chunk_index + 1] && ( chunk_min < 0 || *chunk[chunk_index] < *chunk[chunk_min] ) )
            chunk_min = chunk_index;
      *current = *chunk[chunk_min];
      chunk[chunk_min]++;
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| ������������ ���������� ������                     |
//+----------------------------------------------------+
void CParallelSort::SerialSort( int* chunk_begin, int* chunk_end )
  {
   if( chunk_begin == nullptr || chunk_end == nullptr || chunk_begin > chunk_end )
      return;
   std::sort( chunk_begin, chunk_end );
//--- ���������� �� ��������� ���������
   SerialSortComplete();
  }
//+----------------------------------------------------+
//| �������� ���������� ������������ ����������        |
//+----------------------------------------------------+
void CParallelSort::SerialSortWait( const int chunks_count )
  {
   boost::unique_lock<boost::mutex> lock( m_chunks_sorted_sync );
   while( m_chunks_sorted < chunks_count )
      m_chunks_sorted_cond.wait( lock );
  }
//+----------------------------------------------------+
