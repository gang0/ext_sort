//+----------------------------------------------------+
//| Внешняя сортировка                                 |
//+----------------------------------------------------+
template<class IntType = unsigned, class ParallelSort = CParallelQuickSort<IntType>>
class CExternalSort
  {
private:
   //--- размер промежуточных буферов
   const size_t      m_buffer_size;
   //--- асинхронный сервис
   boost::asio::io_service &m_io_service;
   //--- параллельная сортировка
   ParallelSort      m_parallel_sort;
   //--- имена файлов с чанками
   std::vector<std::string> m_chunks;

public:
   //--- конструктор/деструктор
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_buffer_size( RAM_MAX / 4 ), m_io_service( io ), m_parallel_sort( io, concurrency_level ) {};
   //--- сортировка файла <input_file_name>, результат в файле <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- формирование имени файла для следующего чанка
   std::string       ChunkNextName( const std::string &input_file_name );
   //--- добавление чанка
   void              ChunkAdd( const std::string &chunk_name );
   //--- разделяет исходный файл на отсортированные части
   bool              Split( std::string &input_file_name );
   //--- сливает отсортированные части в выходной файл
   bool              Merge( std::string &output_file_name );
  };
//+----------------------------------------------------+
//| Сортировка                                         |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::Sort( std::string input_file_name, std::string output_file_name )
  {
//--- разделяем входной файл на сортированные части
   if( !Split( input_file_name ) )
      return;
//--- сливаем части в выходной файл
   Merge( output_file_name );
  }
//+----------------------------------------------------+
//| Формирование имени файла для следующего чанка      |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
std::string CExternalSort<IntType, ParallelSort>::ChunkNextName( const std::string &input_file_name )
  {
//--- формируем имя на основе имени входного файла, расширение файла оставляем
   std::string chunk_name( input_file_name );
   chunk_name.append( "_" );
//--- формируем строковый индекс
   std::ostringstream chunk_index;
   chunk_index.width( 3 );
   chunk_index.fill( '0' );
   chunk_index << m_chunks.size();
//--- добавляем индекс
   chunk_name.append( chunk_index.str() );
   return( chunk_name );
  }
//+----------------------------------------------------+
//| Добавление чанка                                   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::ChunkAdd( const std::string &chunk_name )
  {
   m_chunks.push_back( chunk_name );
  }
//+----------------------------------------------------+
//| Разделяет исходный файл на отсортированные части   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Split( std::string &input_file_name )
  {
   CAutoTimer timer( "input file splitting to sorted chunks" );
//--- исходный файл
   CBufferedAsyncFile input_file( m_io_service, m_buffer_size );
   if( !input_file.Open( input_file_name, CBinFile::MODE_READ ) )
     {
      std::cerr << "failed to open input file " << input_file_name << std::endl;
      return( false );
     }
//--- интерфейс для записи чанков
   CBufferedAsyncFile chunk_file( m_io_service, m_buffer_size );
   int chunk_file_index = 0;
//--- буфер для начальных и отсортированных данных
   std::unique_ptr<char[]> unsorted_data( new char[m_buffer_size] );
   std::unique_ptr<char[]> sorted_data( new char[m_buffer_size] );
//--- читаем порцию данных
   size_t data_size = input_file.Read( unsorted_data );
   while( data_size > 0 )
     {
      //--- проверим размер данных
      if( data_size > m_buffer_size || data_size % sizeof( IntType ) != 0 )
        {
         std::cerr << "invalid size of data" << std::endl;
         return( false );
        }
      //--- сортируем
      if( !m_parallel_sort.Sort( (IntType*) unsorted_data.get(), (IntType*) ( unsorted_data.get() + data_size ), (IntType*) sorted_data.get() ) )
         return( false );
      //--- добавляем новый чанк
      std::string chunk_name = ChunkNextName( input_file.Name() );
      if( chunk_file.Open( chunk_name, CBinFile::MODE_WRITE ) )
         ChunkAdd( chunk_name );
      else
         return( false );
      //--- асинхронно записываем чанк в файл
      chunk_file.Write( sorted_data, data_size );
      //--- читаем следующую порцию
      data_size = input_file.Read( unsorted_data );
     }
   return( true );
  }
//+----------------------------------------------------+
//| Сливает отсортированные части в выходной файл      |
//+----------------------------------------------------+
template<class IntType,class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Merge(  std::string &output_file_name )
  {
   CAutoTimer timer( "merging sorted chunks to output file" );
//--- выходной файл
   CDataChunk<IntType> output_file( m_io_service, RAM_MAX / 4 );
   if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
     {
      std::cerr << "failed to open output file " << output_file_name << std::endl;
      return( false );
     }
//--- отсортированные чанки
   CDataChunk<IntType>::PtrArray data_chunks;
//--- бинарная куча элементов с наименьшим в вершине
   std::priority_queue<CDataChunkItem<IntType>, std::vector<CDataChunkItem<IntType>>, std::greater<CDataChunkItem<IntType>>> data_items;
//--- открываем чанки с отсортированными частями
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
//--- заполняем кучу из первых элементов каждого чанка
   for( const auto &chunk : data_chunks )
     {
      CDataChunkItem<IntType> chunk_item( chunk );
      if( chunk_item.Next() )
         data_items.push( chunk_item );
     }
//--- на каждой итерации берем элемент сверху кучи
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
