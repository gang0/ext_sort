//+----------------------------------------------------+
//| DD.MM.YYYY                                         |
//+----------------------------------------------------+

#define _WIN32_WINNT 0x0700

#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <queue>
//--- TODO: только для тестов!
#include <random>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/smart_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>
//--- TODO: убрать
using namespace std;
//--- 
#include "BinFile.h"
#include "BufferedAsyncFile.h"
#include "ParallelSort.h"
#include "ExternalSort.h"
// #include "Chunk.h"
// #include "OutputFile.h"
//--- 
const long long KB = 1024;
const long long MB = 1024 * KB;
const long long GB = 1024 * MB;
//--- 
const int CONCURRENCY_MULTIPLIER = 4;
const int CHUNKS_MAX=256;
const size_t RAM_MAX = 256 * MB;
//+----------------------------------------------------+
//| Timer, ms                                          |
//+----------------------------------------------------+
class CTimer
  {
private:
   chrono::time_point<chrono::high_resolution_clock> m_start;

public:
   void              Start() { m_start = chrono::high_resolution_clock::now(); }
   long long         End() { auto end = chrono::high_resolution_clock::now(); return( chrono::duration_cast<chrono::milliseconds>( end - m_start ).count() ); }
  };
//+----------------------------------------------------+
//| Auto timer                                         |
//+----------------------------------------------------+
class CAutoTimer
  {
private:
   CTimer            m_timer;
   string            m_name;

public:
                     CAutoTimer( const string name ) : m_name( name ) { m_timer.Start(); }
                    ~CAutoTimer() { cout << m_name << ": " << m_timer.End() << " ms" << endl; }
  };
//+----------------------------------------------------+
//| Parameters                                         |
//+----------------------------------------------------+
bool parameters( const char* input_file_name_arg, const char* output_file_name_arg, string &input_file_name, string &output_file_name )
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
   cout << "Usage: sort <input_file_name> <output_file_name>" << endl;
   cout << '\t' << "input_file_name - name of the input file" << endl;
   cout << '\t' << "output_file_name - name of the output file" << endl;
  }
//+----------------------------------------------------+
//| Main function                                      |
//+----------------------------------------------------+
int main(int argc,char** argv)
  {
   CAutoTimer timer( "main" );
//--- получим параметры
   if( argc != 3 )
     {
      cerr << "invalid parameters" << endl;
      usage();
      return( -1 );
     }
   string input_file_name, output_file_name;
   if( !parameters( argv[1], argv[2], input_file_name, output_file_name ) )
     {
      cerr << "failed to read parameters" << endl;
      return( -1 );
     }
//--- TODO: проверить размер файла, кратен 4, не более 16 Гб, количество чанков не более 256
//--- 
   try
     {
      //--- тест сортировки
//       int* data = new int[BUFFER_SIZE];
//       default_random_engine rnd;
//       rnd.seed( ( unsigned )time( nullptr ) );
//       for( int i = 0; i < BUFFER_SIZE; i++ )
//          *((unsigned*)(data + i)) = rnd();
//       int* result = new int[BUFFER_SIZE];
//       CParallelQuickSort<> p_sort( io, boost::thread::hardware_concurrency() * 4 );
//       io.post( boost::bind( &CParallelSort<>::Sort, &p_sort, data, data + BUFFER_SIZE, result ) );
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
      //--- TODO: ожидать с таймаутом, возможно ли вынести из блока try/catch
      threads_pool.join_all();
     }
   catch( std::exception &ex )
     {
      cerr << "unhandled exception caught: " << ex.what();
      return( -1 );
     }
//--- ok
   return( 0 );
  }