//+----------------------------------------------------+
//| 22.03.2014                                         |
//+----------------------------------------------------+
//--- C
#include <stdio.h>
#include <stdlib.h>
//--- STL
#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <queue>
#include <functional>
//--- boost
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
//--- 
#include "BinFile.h"
#include "BufferedAsyncFile.h"
#include "DataChunk.h"
#include "ParallelSort.h"
#include "ExternalSort.h"
//--- 
const long long KB = 1024;
const long long MB = 1024 * KB;
const long long GB = 1024 * MB;
//+----------------------------------------------------+
//| ќпредел€ет количество потоков дл€ обработки        |
//+----------------------------------------------------+
const int CONCURRENCY_MULTIPLIER = 4;  
//+----------------------------------------------------+
//| ƒоступна€ пам€ть                                   |
//+----------------------------------------------------+
const long long RAM_MAX = 256 * MB;
//+----------------------------------------------------+
//| ћаксимальное количество файловых чанков            |
//+----------------------------------------------------+
const int CHUNKS_MAX = 256;
//+----------------------------------------------------+
//| Timer, ms                                          |
//+----------------------------------------------------+
class CTimer
  {
private:
   std::chrono::time_point<std::chrono::high_resolution_clock> m_start;

public:
   void              Start() { m_start = std::chrono::high_resolution_clock::now(); }
   long long         End() { auto end = std::chrono::high_resolution_clock::now(); return( std::chrono::duration_cast<std::chrono::milliseconds>( end - m_start ).count() ); }
  };
//+----------------------------------------------------+
//| Auto timer                                         |
//+----------------------------------------------------+
class CAutoTimer
  {
private:
   CTimer            m_timer;
   std::string       m_name;

public:
                     CAutoTimer( const std::string name ) : m_name( name ) { std::cout << m_name << " started" << std::endl; m_timer.Start(); }
                    ~CAutoTimer() { std::cout << m_name << " completed in " << m_timer.End() << " ms" << std::endl; }
  };
//+----------------------------------------------------+
//| Parameters                                         |
//+----------------------------------------------------+
bool parameters( const char* input_file_name_arg, const char* output_file_name_arg, std::string &input_file_name, std::string &output_file_name )
  {
   if( input_file_name_arg == NULL || output_file_name_arg == NULL )
      return( false );
//--- name
   input_file_name = input_file_name_arg;
   output_file_name = output_file_name_arg;
   return( true );
  }
//+----------------------------------------------------+
//| Usage                                              |
//+----------------------------------------------------+
void usage()
  {
   std::cout << "Usage: external_sort <input_file_name> <output_file_name>" << std::endl;
   std::cout << '\t' << "input_file_name - name of the input file" << std::endl;
   std::cout << '\t' << "output_file_name - name of the output file" << std::endl;
  }
//+----------------------------------------------------+
//| ѕроверка файла                                     |
//+----------------------------------------------------+
bool file_check( const std::string &file_name )
  {
//--- провер€ем наличие файла
   if( !boost::filesystem::exists( file_name ) )
     {
      std::cerr << "file is not exist " << file_name << std::endl;
      return( false );
     }
//--- провер€ем размер
   long long file_size = boost::filesystem::file_size( file_name );
//--- размер должен быть кратен размеру данных
   if( file_size % sizeof( unsigned ) != 0 )
     {
      std::cerr << "file size is not a multiple of unsigned 32bit integer size (" << file_size << ")" << std::endl;
      return( false );
     }
//--- размер не должен превышать 16 Gb
   if( file_size > 16 * GB )
     {
      std::cerr << "file size exceeds maximum size of 16 GB (" << file_size << ")" << std::endl;
      return( false );
     }
//--- ограничиваем количество чанков
   if( file_size / ( RAM_MAX / 4 ) > CHUNKS_MAX )
     {
      std::cerr << "file size exceeds maximum size of " << CHUNKS_MAX * RAM_MAX / 4 << " (" << file_size << ")" <<std::endl;
      return( false );
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| Main function                                      |
//+----------------------------------------------------+
int main(int argc,char** argv)
  {
   CAutoTimer timer( "external sort" );
//--- получим параметры
   if( argc != 3 )
     {
      std::cerr << "invalid parameters" << std::endl;
      usage();
      return( -1 );
     }
   std::string input_file_name, output_file_name;
   if( !parameters( argv[1], argv[2], input_file_name, output_file_name ) )
     {
      std::cerr << "failed to read parameters" << std::endl;
      return( -1 );
     }
//--- проверим файл
   if( !file_check( input_file_name ) )
      return( -1 );
//--- сортировка
   try
     {
      //--- подготавливаем многопоточное окружение
      int concurrency_level = boost::thread::hardware_concurrency() * CONCURRENCY_MULTIPLIER;
      boost::asio::io_service io;
      CExternalSort<> ext_sort( io, concurrency_level );
      io.post( boost::bind( &CExternalSort<>::Sort, &ext_sort, input_file_name, output_file_name ) );
      //--- создаем пул потоков
      boost::thread_group threads_pool;
      for( int thread_index = 0; thread_index < concurrency_level; thread_index++ )
         threads_pool.create_thread( boost::bind( &boost::asio::io_service::run, &io ) );
      //--- 
      io.run();
      //--- TODO: ожидать с таймаутом
      threads_pool.join_all();
     }
   catch( std::exception &ex )
     {
      std::cerr << "unhandled exception caught: " << ex.what() << std::endl;
      return( -1 );
     }
//--- ok
   return( 0 );
  }