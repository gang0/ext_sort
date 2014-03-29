//+----------------------------------------------------+
//| Внешняя сортировка                                 |
//+----------------------------------------------------+
template<class IntType = unsigned, class ParallelSort = CParallelQuickSort<IntType>>
class CExternalSort
  {
private:
   //--- асинхронный сервис
   boost::asio::io_service &m_io_service;
   //--- параллельная сортировка
   ParallelSort      m_parallel_sort;
   //--- сортировка
   CBufferedAsyncFile::PtrArray m_chunks;

public:
   //--- конструктор/деструктор
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_io_service( io ), m_parallel_sort( io, concurrency_level ) {};
   //--- сортировка файла <input_file_name>, результат в файле <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- разделяет исходный файл на отсортированные части
   bool              Split( CBufferedAsyncFile &input_file, CBufferedAsyncFile::PtrArray &chunks );
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
   CBufferedAsyncFile input_file( m_io_service, BUFFER_SIZE );
   if( !input_file.Open( input_file_name, CBinFile::MODE_READ ) )
      return;
//--- открываем выходной файл
//    CBinFile output_file;
//    if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
//       return;
//--- разделяем входной файл на сортированные части
   if( !Split( input_file, m_chunks ) )
      return;
//--- сливаем части в выходной файл
//    Merge( m_chunks, output_file );
  }
//+----------------------------------------------------+
//| Разделяет исходный файл на отсортированные части   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Split( CBufferedAsyncFile &input_file, CBufferedAsyncFile::PtrArray &chunks )
  {
   CAutoTimer timer( "split" );
//--- интерфейс для записи чанков
   CBufferedAsyncFile chunk_file( m_io_service, BUFFER_SIZE );
   int chunk_file_index = 0;
//--- буфер для начальных и отсортированных данных
   std::unique_ptr<char[]> unsorted_data( new char[BUFFER_SIZE] );
   std::unique_ptr<char[]> sorted_data( new char[BUFFER_SIZE] );
//--- читаем порцию данных
   size_t data_size = input_file.Read( unsorted_data );
   while( data_size > 0 )
     {
      //--- обрабатываем текущую порцию данных
        {
         CAutoTimer timer( "sort" );
         if( !m_parallel_sort.Sort( (IntType*) unsorted_data.get(), (IntType*) ( unsorted_data.get() + data_size ), (IntType*) sorted_data.get() ) )
            return( false );
        }
      //--- добавляем новый чанк
      std::ostringstream chunk_name;
      chunk_name << "chunk_" << chunk_file_index++ << ".dat";
      chunk_file.Open( chunk_name.str(), CBinFile::MODE_WRITE );
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
