//+----------------------------------------------------+
//| 22.03.2014                                         |
//+----------------------------------------------------+
//--- stl
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
//--- boost
#include <boost/filesystem.hpp>
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
   std::cout << "Usage: internal_sort <input_file_name> <output_file_name>" << std::endl;
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
   size_t file_size = (size_t) boost::filesystem::file_size( file_name );
//--- размер должен быть кратен размеру данных
   if( file_size % sizeof( unsigned ) != 0 )
     {
      std::cerr << "file size is not a multiple of unsigned 32bit integer size (" << file_size << ")" << std::endl;
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
   CAutoTimer timer( "internal sort" );
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
      std::ifstream input_file( input_file_name, std::ios_base::in | std::ios_base::binary );
      long long data_size = boost::filesystem::file_size( input_file_name );
      if( data_size > std::numeric_limits<unsigned>::max() )
         return( -1 );
      std::unique_ptr<char[]> data( new char[(unsigned)data_size] );
      input_file.read( data.get(), data_size );
      std::sort( (unsigned*) data.get(), (unsigned*) ( data.get() + data_size ) );
      std::ofstream output_file( output_file_name, std::ios_base::out | std::ios_base::binary );
      output_file.write( data.get(), data_size );
     }
   catch( std::exception &ex )
     {
      std::cerr << "unhandled exception caught: " << ex.what() << std::endl;
      return( -1 );
     }
//--- ok
   return( 0 );
  }