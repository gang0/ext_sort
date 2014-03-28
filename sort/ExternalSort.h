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
   CBinFilePtrArray  m_chunks;

public:
   //--- конструктор/деструктор
                     CExternalSort( boost::asio::io_service &io, const int concurrency_level ) : m_io_service( io ), m_parallel_sort( io, concurrency_level ) {};
   //--- сортировка файла <input_file_name>, результат в файле <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- разделяет исходный файл на отсортированные части
   bool              Split( CBinFile &input_file, CBinFilePtrArray &chunks );
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
   CBinFile input_file;
   if( !input_file.Open( input_file_name, CBinFile::MODE_READ ) )
      return;
//--- открываем выходной файл
   CBinFile output_file;
   if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
      return;
//--- разделяем входной файл на сортированные части
   if( !Split( input_file, m_chunks ) )
      return;
//--- сливаем части в выходной файл
   Merge( m_chunks, output_file );
  }
//+----------------------------------------------------+
//| Разделяет исходный файл на отсортированные части   |
//+----------------------------------------------------+
template<class IntType, class ParallelSort>
bool CExternalSort<IntType, ParallelSort>::Split( CBinFile &input_file, CBinFilePtrArray &chunks )
  {
   CAutoTimer timer( "Split" );
//--- TODO: корректно очищать память при выходе
//--- TODO: в случае аварийного выхода, другие потоки могут использовать эти буферы
   char* buffer = new( std::nothrow ) char[BUFFER_SIZE];
   char* buffer_result = new( std::nothrow ) char[BUFFER_SIZE];
   char* buffer_read = new( std::nothrow ) char[BUFFER_SIZE];
   char* buffer_write = new( std::nothrow ) char[BUFFER_SIZE];
   size_t buffer_len = BUFFER_SIZE;
   size_t buffer_read_len = BUFFER_SIZE;
   size_t buffer_write_len = BUFFER_SIZE;
   if( buffer == NULL )
     {
      cerr << "failed to allocate buffer" << endl;
      return( false );
     }
//--- синхронно читаем первую часть
   input_file.ReadAsync( m_io_service, buffer, &buffer_len );
   input_file.Wait();
   while( buffer_len > 0 )
     {
      //--- читаем следующую порцию данных
      input_file.ReadAsync( m_io_service, buffer_read, &buffer_read_len );
      //--- обрабатываем текущую порцию данных
        {
         CAutoTimer timer( "sort" );
         if( !m_parallel_sort.Sort( (IntType*) buffer, (IntType*) ( buffer + buffer_len ), (IntType*) buffer_result ) )
            return( false );
        }
      memcpy( buffer, buffer_result, BUFFER_SIZE );
//       std::sort( ( int* )buffer, ( int* ) ( buffer + buffer_len ) );
      //--- создаем файл с чанком
      //--- добавляем новый чанк
      chunks.push_back( new CBinFile() );
      CBinFile &chunk = *chunks[chunks.size() - 1];
      std::ostringstream chunk_name;
      chunk_name << "chunk_" << chunks.size() - 1 << ".dat";
      if( !chunk.Open( chunk_name.str(), CBinFile::MODE_RW | CBinFile::MODE_TEMP ) )
         return( false );
      //--- асинхронно записываем чанк в файл
      //--- ждем завершение записи предыдущего чанка
      if( chunks.size() > 1 )
         chunks[chunks.size() - 2]->Wait();
      char* buffer_temp = buffer_write;
      buffer_write = buffer;
      buffer_write_len = buffer_len;
      buffer = buffer_temp;
      buffer_len = 0;
      chunk.WriteAsync( m_io_service, buffer_write, &buffer_write_len );
      //--- меняем буферы
      input_file.Wait();
      buffer_temp = buffer;
      buffer = buffer_read;
      buffer_len = buffer_read_len;
      buffer_read = buffer_temp;
      buffer_read_len = BUFFER_SIZE;
     }
   chunks[chunks.size() - 1]->Wait();
   delete[] buffer;
   delete[] buffer_read;
   delete[] buffer_write;
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
