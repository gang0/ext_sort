//+----------------------------------------------------+
//| DD.MM.YYYY                                         |
//+----------------------------------------------------+
#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <random>
#include <string>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
//--- 
using namespace std;
//--- 
const long long KB = 1024;
const long long MB = 1024 * KB;
const long long GB = 1024 * MB;
//--- 
const int CHUNKS_MAX=256;
const size_t BUFFER_SIZE = 128 * MB;
#pragma warning(disable:4996)
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
//|                                                    |
//+----------------------------------------------------+
void split( const string &file_name, FILE** chunks,int &chunks_total)
  {
   if( chunks == NULL )
      return;
   char* buffer = new( nothrow ) char[BUFFER_SIZE];
   if( buffer == NULL )
     {
      cerr << "failed to allocate buffer" << endl;
      return;
     }
   FILE* stream = fopen( file_name.c_str(), "rSb");
   if( stream == NULL )
     {
      cerr << "failed to open file " << file_name << endl;
      return;
     }
   size_t buffer_len=0;
   chunks_total=0;
   while( ( buffer_len = fread( buffer, sizeof( char ), BUFFER_SIZE, stream ) ) > 0 )
     {
      std::sort( ( int* )buffer, ( int* ) ( buffer + buffer_len ) );
      std::ostringstream chunk_name;
      chunk_name << "chunk_" << chunks_total << ".dat";
      chunks[chunks_total] = fopen( std::string( chunk_name.str()).c_str(), "w+b" );
      size_t chunk_len=fwrite( buffer, sizeof( char ), buffer_len, chunks[chunks_total]);
      chunks_total++;
     }
   fclose(stream);
   delete[] buffer;
  }
//+----------------------------------------------------+
//|                                                    |
//+----------------------------------------------------+
void merge(FILE** chunks, const int chunks_total)
  {
   int top[CHUNKS_MAX]={0};
   bool empty[CHUNKS_MAX]={0};
//--- 
   for( int chunk_index=0; chunk_index < chunks_total; chunk_index++ )
     {
      fseek( chunks[chunk_index], 0, SEEK_SET );
      fread( top + chunk_index, sizeof(int), 1, chunks[chunk_index] );
     }
//--- 
   FILE* stream = fopen( "2.dat", "wSb" );
   bool run=true;
   while( run )
     {
      int chunk_index_min=-1;
      for( int chunk_index = 0; chunk_index < chunks_total; chunk_index++ )
         if( !empty[chunk_index] && ( chunk_index_min < 0 || top[chunk_index] < top[chunk_index_min] ) )
            chunk_index_min=chunk_index;
      if( chunk_index_min < 0 )
         run = false;
      else
        {
         fwrite( &top[chunk_index_min], sizeof(int), 1, stream );
         if( fread( &top[chunk_index_min], sizeof(int), 1, chunks[chunk_index_min] ) == 0 )
           {
            empty[chunk_index_min] = true;
            fclose( chunks[chunk_index_min] );
            chunks[chunk_index_min] = NULL;
           }
        }
     }
   fclose( stream );
  }
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
//--- split
   FILE* chunks[CHUNKS_MAX]={0};
   int chunks_total=0;
   split( input_file_name, chunks, chunks_total );
//--- merge
   merge( chunks, chunks_total );
//--- ok
   return(0);
  }