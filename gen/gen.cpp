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
//| Generate random file                               |
//+----------------------------------------------------+
template<class IntType>
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
   default_random_engine rnd( (unsigned) time( nullptr ) );
   uniform_int_distribution<IntType> dist( std::numeric_limits<IntType>::min(), std::numeric_limits<IntType>::max() );
   char buffer[4 * KB];
   size_t buffer_size=0;
   for(size_t i=0; i < file_size / sizeof(IntType); i++)
     {
      *((IntType*)(buffer + buffer_size)) = dist( rnd );
      buffer_size += sizeof(IntType);
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
   generate_random_file<unsigned>( file_name, file_size );
//--- ok
   return(0);
  }