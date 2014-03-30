//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+
//+----------------------------------------------------+
//| Параллельная сортировка                            |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CParallelSort
  {
private:
   //--- асинхронный сервис
   boost::asio::io_service &m_io_service;
   //--- уровень параллелизма
   const int         m_concurrency_level;

public:
                     CParallelSort( boost::asio::io_service &io, const int concurrency_level ) : m_io_service( io ), m_concurrency_level( concurrency_level ) {}
   virtual          ~CParallelSort() {}
   //--- сортировка данных (метод не потокобезопасен, нельзя вызывать этот метод для одного объекта из нескольких потоков)
   bool              Sort( IntType* begin, IntType* end, IntType* result) { return( SortImpl( begin, end, result ) ); }

protected:
   //--- асинхронный сервис
   boost::asio::io_service &IOService() { return( m_io_service ); }
   //--- максимальный уровень параллелизма
   int               ConcurrencyLevel() const { return( m_concurrency_level ); }

private:
   //--- реализация сортировки
   virtual bool      SortImpl( IntType* begin, IntType* end, IntType* result ) = 0;
  };
//+----------------------------------------------------+
//| Параллельная сортировка с линейным слиянием        |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CParallelSortLinearMerge : public CParallelSort<IntType>
  {
private:
   //--- количество отсортированных частей
   int               m_chunks_sorted;
   boost::mutex      m_chunks_sorted_sync;
   boost::condition_variable m_chunks_sorted_cond;

public:
                     CParallelSortLinearMerge( boost::asio::io_service &io, const int concurrency_level );

private:
   //--- реализация сортировки
   virtual bool      SortImpl( IntType* begin, IntType* end, IntType* result );
   //--- однопоточная сортировка данных
   void              SerialSort( IntType* chunk_begin, IntType* chunk_end );
   void              SerialSortComplete() { m_chunks_sorted_sync.lock(); m_chunks_sorted++; m_chunks_sorted_sync.unlock(); m_chunks_sorted_cond.notify_all(); }
   //--- начало/ожидание завершения сортировки
   void              SortStart() { m_chunks_sorted_sync.lock(); m_chunks_sorted = 0; m_chunks_sorted_sync.unlock(); }
   void              SortWait( const int chunks_count );
  };
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
template<class IntType>
CParallelSortLinearMerge<IntType>::CParallelSortLinearMerge( boost::asio::io_service &io, const int concurrency_level ) : CParallelSort<IntType>( io, concurrency_level ), m_chunks_sorted( 0 )
  {
  }
//+----------------------------------------------------+
//| Сортировка данных                                  |
//+----------------------------------------------------+
template<class IntType>
bool CParallelSortLinearMerge<IntType>::SortImpl( IntType* begin, IntType* end, IntType* result )
  {
   if( begin == nullptr || end == nullptr || result == nullptr || begin > end )
      return( false );
//--- TODO: вынести этапы сортировки в отдельные методы
//--- разбиваем данные на несколько промежутков в зависимости от степени параллелизма
//--- TODO: возможно стоит разбивать на более мелкие части
   const int chunks_count = CParallelSort<IntType>::ConcurrencyLevel();
   std::vector<IntType*> bound;
   for( int bound_index = 0; bound_index < chunks_count; bound_index++ )
      bound.push_back( begin + bound_index * ( end - begin ) / chunks_count );
   bound.push_back( end );
//--- запускаем параллельную сортировку промежутков
   SortStart();
     {
      for( int chunk_index = 0; chunk_index < chunks_count; chunk_index++ )
         CParallelSort<IntType>::IOService().post( boost::bind( &CParallelSortLinearMerge::SerialSort, this, bound[chunk_index], bound[chunk_index + 1] ) );
     }
   SortWait( chunks_count );
//--- сливаем отсортированные промежутки
   std::vector<IntType*> chunk( bound );
   chunk.pop_back();
   for( IntType* current = result; current < result + ( end - begin ); current++ )
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
//| Однопоточная сортировка данных                     |
//+----------------------------------------------------+
template<class IntType>
void CParallelSortLinearMerge<IntType>::SerialSort( IntType* chunk_begin, IntType* chunk_end )
  {
   if( chunk_begin == nullptr || chunk_end == nullptr || chunk_begin > chunk_end )
      return;
   std::sort( chunk_begin, chunk_end );
//--- уведомляем об окончании обработки
   SerialSortComplete();
  }
//+----------------------------------------------------+
//| Ожидание завершения сортировки                     |
//+----------------------------------------------------+
template<class IntType>
void CParallelSortLinearMerge<IntType>::SortWait( const int chunks_count )
  {
   boost::unique_lock<boost::mutex> lock( m_chunks_sorted_sync );
//--- TODO: использовать wait с таймаутом
   while( m_chunks_sorted < chunks_count )
      m_chunks_sorted_cond.wait( lock );
  }
//+----------------------------------------------------+
//| Параллельная быстрая сортировка                    |
//+----------------------------------------------------+
template<class IntType = unsigned>
class CParallelQuickSort : public CParallelSort<IntType>
  {
private:
   //--- минимальный размер чанка
   size_t            m_chunk_min_size;
   //--- количество отсортированных частей
   int               m_chunks_total;
   int               m_chunks_sorted;
   boost::mutex      m_chunks_sorted_sync;
   boost::condition_variable m_chunks_sorted_cond;

public:
                     CParallelQuickSort( boost::asio::io_service &io, const int concurrency_level );

private:
   //--- реализация сортировки данных
   virtual bool      SortImpl( IntType* begin, IntType* end, IntType* result );
   //--- быстрая сортировка
   void              QuickSort( IntType* begin, IntType* end );
   //--- уведомление о добавлении неотсортированной части
   void              QuickSortAdd() { m_chunks_sorted_sync.lock(); m_chunks_total++; m_chunks_sorted_sync.unlock(); }
   //--- уведомление о завершении сортировки части
   void              QuickSortComplete() { m_chunks_sorted_sync.lock(); m_chunks_sorted++; m_chunks_sorted_sync.unlock(); m_chunks_sorted_cond.notify_all(); }
   //--- начало/ожидание завершения сортировки
   void              SortStart() { m_chunks_sorted_sync.lock(); m_chunks_total = 0; m_chunks_sorted = 0; m_chunks_sorted_sync.unlock(); }
   void              SortWait();
  };
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
template<class IntType>
CParallelQuickSort<IntType>::CParallelQuickSort( boost::asio::io_service &io, const int concurrency_level ) : CParallelSort<IntType>( io, concurrency_level ), m_chunks_total( 0 ), m_chunks_sorted( 0 )
  {
  }
//+----------------------------------------------------+
//| Сортировка данных                                  |
//+----------------------------------------------------+
template<class IntType>
bool CParallelQuickSort<IntType>::SortImpl( IntType* begin, IntType* end, IntType* result )
  {
   if( begin == nullptr || end == nullptr || result == nullptr || begin > end )
      return( false );
//--- определяем минимальный размер чанка, который сортируем параллельно
//--- TODO: возможно стоит разбивать на более мелкие части
   m_chunk_min_size = ( end - begin ) / CParallelSort<IntType>::ConcurrencyLevel();
//--- запускаем параллельную сортировку
   SortStart();
     {
      QuickSortAdd();
      //--- указатель за последним элементом данных смещаем влево на элемент данных
      CParallelSort<IntType>::IOService().post( boost::bind( &CParallelQuickSort::QuickSort, this, begin, end ) );
     }
   SortWait();
//--- копируем результат
   memcpy( result, begin, (end - begin) * sizeof( IntType ) );
   return( true );
  }
//+----------------------------------------------------+
//| Быстрая сортировка                                 |
//+----------------------------------------------------+
template<class IntType>
void CParallelQuickSort<IntType>::QuickSort( IntType* begin, IntType* end )
  {
   if( begin == nullptr || end == nullptr || begin > end )
      return;
//--- если размер данных меньше минимального, сортируем последовательно
   if( ( size_t )( end - begin ) < m_chunk_min_size )
      std::sort( begin, end );
   else
     {
      //--- TODO: выбирать порог случайно
      IntType pivot = begin[( end - begin ) / 2];
      IntType* left = begin;
      IntType* right = end - 1;
      do
        {
         while( *left < pivot )
            left++;
         while( *right > pivot )
            right--;
         if( left <= right )
           {
            if( left < right )
               std::swap( *left, *right );
            left++;
            right--;
           }
        } while( left <= right );
      if( begin < right )
        {
         QuickSortAdd();
         CParallelSort<IntType>::IOService().post( boost::bind( &CParallelQuickSort::QuickSort, this, begin, right + 1 ) );
        }
      if( left < end - 1 )
        {
         QuickSortAdd();
         CParallelSort<IntType>::IOService().post( boost::bind( &CParallelQuickSort::QuickSort, this, left, end ) );
        }
     }
   QuickSortComplete();
  }
//+----------------------------------------------------+
//| Ожидание завершения сортировки                     |
//+----------------------------------------------------+
template<class IntType>
void CParallelQuickSort<IntType>::SortWait()
  {
   boost::unique_lock<boost::mutex> lock( m_chunks_sorted_sync );
//--- TODO: использовать wait с таймаутом
   while( m_chunks_sorted < m_chunks_total )
      m_chunks_sorted_cond.wait( lock );
  }
//+----------------------------------------------------+
