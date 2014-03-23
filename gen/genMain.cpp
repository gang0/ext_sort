//+----------------------------------------------------+
//| 23.03.2014                                         |
//+----------------------------------------------------+
#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <random>
#include <string>
//--- 
using namespace std;
//--- 
const long long KB = 1024;
const long long MB = 1024 * KB;
const long long GB = 1024 * MB;
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
                    ~CAutoTimer() { cout << "Timer " << m_name << ": elapsed " << m_timer.End() << " ms" << endl; }
  };
//+----------------------------------------------------+
//| Generate random file                               |
//+----------------------------------------------------+
void generate_random_file( const string &file_name, const size_t file_size )
  {
//--- open the output file
   ofstream fout;
   fout.open( file_name, ios_base::out | ios_base::binary );
   if( !fout )
     {
      cerr << "failed to open output file " << file_name << endl;
      return;
     }
//--- generate numbers
   default_random_engine rnd;
   char buffer[4 * KB];
   size_t buffer_size=0;
   for(size_t i=0; i < file_size / sizeof(int); i++)
     {
      *((unsigned*)(buffer + buffer_size)) = rnd();
      buffer_size += sizeof(int);
      if( buffer_size == sizeof( buffer ) )
        {
         fout.write( buffer, buffer_size );
         buffer_size = 0;
        }
     }
   if( buffer_size > 0 )
      fout.write( buffer, buffer_size );
   fout.close();
  }
//+----------------------------------------------------+
//| Parameters                                         |
//+----------------------------------------------------+
bool parameters( const char* file_name_arg, const char* file_size_arg, string &file_name, size_t &file_size )
  {
   if( file_name_arg == NULL || file_size_arg == NULL )
      return( false );
//--- name
   file_name = file_name_arg;
//--- size
   string size_unit;
   stringstream file_size_arg_s( file_size_arg );
   file_size_arg_s >> file_size >> size_unit;
   if( !size_unit.empty() )
      if( size_unit.compare( "GB" )==0 )
         file_size *= GB;
      else
         if( size_unit.compare( "MB" )==0 )
            file_size *= MB;
         else
            if( size_unit.compare( "KB" )==0 )
               file_size *= KB;
            else
              {
               cerr << "invalid size unit";
               return( false );
              }
   return( true );
  }
//+----------------------------------------------------+
//| Usage                                              |
//+----------------------------------------------------+
void usage()
  {
   cout << "Usage: gen <file_name> <file_size>" << endl;
   cout << '\t' << "file_name - name of the output file" << endl;
   cout << '\t' << "file_size - size of the output file (f.e. 256, 64KB, 512MB, 4GB)" << endl;
  }
//+----------------------------------------------------+
//| Main function                                      |
//+----------------------------------------------------+
int main(int argc,char** argv)
  {
   CAutoTimer timer( "main" );
//--- input parameters
   if( argc != 3 )
     {
      cerr << "invalid parameters" << endl;
      usage();
      return( -1 );
     }
   string file_name;
   size_t file_size = 0;
   if( !parameters( argv[1], argv[2], file_name, file_size ) )
     {
      cerr << "failed to read parameters" << endl;
      return( -1 );
     }
//--- generate random file
   generate_random_file( file_name, file_size );
//--- ok
   return(0);
  }