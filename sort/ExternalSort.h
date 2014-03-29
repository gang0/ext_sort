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
   //--- входной файл
   CBufferedAsyncFile m_input_file;
   //--- имена файлов с чанками
   std::vector<std::string> m_chunks;

public:
   //--- конструктор/деструктор
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_buffer_size( RAM_MAX / 4 ), m_io_service( io ), m_parallel_sort( io, concurrency_level ), m_input_file( io, m_buffer_size ) {};
   //--- сортировка файла <input_file_name>, результат в файле <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- формирование имени файла для следующего чанка
   std::string       ChunkNextName();
   //--- добавление чанка
   void              ChunkAdd( const std::string &chunk_name );
   //--- разделяет исходный файл на отсортированные части
   bool              Split( CBufferedAsyncFile &input_file );
   //--- сливает отсортированные части в выходной файл
   bool              Merge( CBinFilePtrArray &chunks, CBinFile &output_file );
  };
//+----------------------------------------------------+
//| Сортировка                                         |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
void CExternalSort<IntType, ParallelSort>::Sort( std::string input_file_name, std::string output_file_name )
  {
//--- открываем входной файл
   if( !m_input_file.Open( input_file_name, CBinFile::MODE_READ ) )
      return;
//--- открываем выходной файл
//    CBinFile output_file;
//    if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
//       return;
//--- разделяем входной файл на сортированные части
   if( !Split( m_input_file ) )
      return;
//--- сливаем части в выходной файл
//    Merge( m_chunks, output_file );
  }
//+----------------------------------------------------+
//| Формирование имени файла для следующего чанка      |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
std::string CExternalSort<IntType, ParallelSort>::ChunkNextName()
  {
//--- формируем имя на основе имени входного файла, расширение файла оставляем
   std::string chunk_name( m_input_file.Name() );
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
bool CExternalSort<IntType, ParallelSort>::Split( CBufferedAsyncFile &input_file )
  {
   CAutoTimer timer( "split" );
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
      //--- сортируем
        {
         CAutoTimer timer( "sort" );
         if( !m_parallel_sort.Sort( (IntType*) unsorted_data.get(), (IntType*) ( unsorted_data.get() + data_size ), (IntType*) sorted_data.get() ) )
            return( false );
        }
      //--- добавляем новый чанк
      std::string chunk_name = ChunkNextName();
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
struct ChunkItem 
  {
   int               item;
   UINT              index;
   bool              operator<( const ChunkItem &chunk_item ) const { return( this->item > chunk_item.item ); } // NOTE: сравниваем наоборот, сначала из очереди выходят наименьшие
  };
template<class IntType,class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Merge( CBinFilePtrArray &chunks_files, CBinFile &output_file )
  {
   CAutoTimer timer( "Merge" );
   bool empty[CHUNKS_MAX]={0};
   std::vector<CChunk*> chunks;
   std::priority_queue<ChunkItem> top;
//--- инициализируем чанки, асинхронно заполняя буферы
   chunks.reserve( chunks_files.size() );
   for( int chunk_index = 0; chunk_index < chunks_files.size(); chunk_index++ )
     {
      //--- TODO: вычислять размер чанка
      CChunk* chunk = new CChunk( BUFFER_SIZE / ( 8 ) );
      chunk->SetFile( chunks_files[chunk_index] );
      chunk->Initialize( m_io_service );
      chunks.push_back( chunk );
     }
//--- заполняем кучу
   for( int chunk_index=0; chunk_index < chunks.size(); chunk_index++ )
     {
      //--- дожидаемся заверешния инициализации
      chunks[chunk_index]->Wait();
      //--- получаем первый элемент чанка
      ChunkItem item;
      item.index = chunk_index;
      chunks[chunk_index]->NextItem( m_io_service, item.item );
      top.push( item );
     }
   COutputFile out_file( BUFFER_SIZE / 8 );
   out_file.SetFile( &output_file );
   while( !top.empty() )
     {
      ChunkItem item = top.top();
      top.pop();
        {
         size_t len = sizeof( int );
         if( !out_file.WriteItem( m_io_service, item.item ) )
            return( false );
         if( chunks[item.index]->NextItem( m_io_service, item.item ) )
            top.push( item );
         else
            empty[item.index] = true;
        }
     }
   return( true );
  }
//+----------------------------------------------------+
