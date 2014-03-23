//+----------------------------------------------------+
//| DD.MM.YYYY                                         |
//+----------------------------------------------------+
#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>

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
//| Бинарный файл                                      |
//+----------------------------------------------------+
class CBinFile
  {
public:
   enum EnMode
     {
      MODE_NONE = 0,
      MODE_READ = 0x01,
      MODE_WRITE = 0x02,
      MODE_TEMP = 0x04,
      MODE_RW = MODE_READ | MODE_WRITE,
     };

private:
   string            m_name;
   int               m_mode;
   FILE*             m_stream;

public:
   //--- конструктор/деструктор
                     CBinFile();
                    ~CBinFile();
   //--- открытие/закрытие файла
   bool              Open( const string &name, const int mode);
   void              Close();
   //--- чтение/запись из файла
   bool              Read( char* buffer, size_t &size );
   bool              Write( const char* buffer, size_t &size );
   //--- перемещение указателя в начало
   void              SeekBegin() { if( m_stream != NULL ) fseek( m_stream, 0, SEEK_SET ); }
   //--- удаление файла
   void              Remove() { Close(); if( !m_name.empty() ) remove( m_name.c_str() ); }
  };
typedef vector<CBinFile> CFileArray;
//+----------------------------------------------------+
//| Конструктор                                        |
//+----------------------------------------------------+
CBinFile::CBinFile() : m_mode( 0 ), m_stream( NULL )
  {
  }
//+----------------------------------------------------+
//| Деструктор                                         |
//+----------------------------------------------------+
CBinFile::~CBinFile()
  {
   Close();
  }
//+----------------------------------------------------+
//| Открытие файла                                     |
//+----------------------------------------------------+
bool CBinFile::Open( const string &name, const int mode )
  {
   Close();
//--- запоминаем атрибуты
   m_name = name;
   m_mode = mode;
//--- открываем файл в нужном режиме
   const char* mode_str = NULL;
   if( mode & MODE_WRITE )
      if( mode & MODE_READ ) mode_str = "w+b";
      else
         mode_str = "wSb";
   else
      mode_str = "rSb";
   errno_t err = fopen_s( &m_stream, name.c_str(), mode_str);
   if( err != 0 || m_stream == NULL )
     {
      cerr << "failed to open file " << name << " (" << err << ")" << endl;
      return( false );
     }
//--- 
   return( true );
  }
//+----------------------------------------------------+
//| Закрытие файла                                     |
//+----------------------------------------------------+
void CBinFile::Close()
  {
   if( m_stream != NULL )
     {
      fclose( m_stream );
      m_stream = NULL;
      if( m_mode & MODE_TEMP )
         Remove();
     }
  }
//+----------------------------------------------------+
//| Чтение из файла                                    |
//+----------------------------------------------------+
bool CBinFile::Read( char* buffer, size_t &size )
  {
   if( buffer == NULL || size == 0 )
      return( false );
   if( m_stream == NULL )
      return( false );
   size = fread( buffer, 1, size, m_stream );
   return( true );
  }
//+----------------------------------------------------+
//| Запись в файл                                      |
//+----------------------------------------------------+
bool CBinFile::Write( const char* buffer, size_t &size )
  {
   if( buffer == NULL || size == 0 )
      return( false );
   if( m_stream == NULL )
      return( false );
   size = fwrite( buffer, 1, size, m_stream);
   return( true );
  }
//+----------------------------------------------------+
//| Внешняя сортировка                                 |
//+----------------------------------------------------+
class CExternalSort
  {
public:
   void              Sort( const string &input_file_name, const string &output_file_name );

private:
   //--- разделяет исходный файл на отсортированные части
   bool              Split( CBinFile &input_file, CFileArray &chunks );
   //--- сливает отсортированные части в выходной файл
   bool              Merge( CFileArray &chunks, CBinFile &output_file );
  };
//+----------------------------------------------------+
//| Сортировка                                         |
//+----------------------------------------------------+
void CExternalSort::Sort( const string &input_file_name, const string &output_file_name )
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
   CFileArray chunks;
   chunks.reserve( CHUNKS_MAX );
   if( !Split( input_file, chunks ) )
      return;
//--- сливаем части в выходной файл
   Merge( chunks, output_file );
  }
//+----------------------------------------------------+
//| Разделяет исходный файл на отсортированные части   |
//+----------------------------------------------------+
bool CExternalSort::Split( CBinFile &input_file, CFileArray &chunks )
  {
   CAutoTimer timer( "Split" );
//--- 
   char* buffer = new char[BUFFER_SIZE];
   if( buffer == NULL )
     {
      cerr << "failed to allocate buffer" << endl;
      return( false );
     }
   size_t buffer_len=BUFFER_SIZE;
   while( input_file.Read( buffer, buffer_len ) && buffer_len > 0 )
     {
      chunks.resize(chunks.size() + 1);
      CBinFile &chunk = chunks[chunks.size() - 1];
      std::sort( ( int* )buffer, ( int* ) ( buffer + buffer_len ) );
      std::ostringstream chunk_name;
      chunk_name << "chunk_" << chunks.size() - 1 << ".dat";
      if( !chunk.Open( chunk_name.str(), CBinFile::MODE_RW | CBinFile::MODE_TEMP ) )
         return( false );
      if( !chunk.Write( buffer, buffer_len ) )
         return( false );
     }
   delete[] buffer;
   return( true );
  }
//+----------------------------------------------------+
//| Сливает отсортированные части в выходной файл      |
//+----------------------------------------------------+
bool CExternalSort::Merge( CFileArray &chunks, CBinFile &output_file )
  {
   CAutoTimer timer( "Merge" );
   int top[CHUNKS_MAX]={0};
   bool empty[CHUNKS_MAX]={0};
//--- 
   for( int chunk_index=0; chunk_index < chunks.size(); chunk_index++ )
     {
      chunks[chunk_index].SeekBegin();
      size_t len = sizeof( int );
      if( !chunks[chunk_index].Read( (char*)( top + chunk_index ), len ) || len != sizeof( int ) )
         return( false );
     }
   bool run=true;
   while( run )
     {
      int chunk_index_min=-1;
      for( int chunk_index = 0; chunk_index < chunks.size(); chunk_index++ )
         if( !empty[chunk_index] && ( chunk_index_min < 0 || top[chunk_index] < top[chunk_index_min] ) )
            chunk_index_min=chunk_index;
      if( chunk_index_min < 0 )
         run = false;
      else
        {
         size_t len = sizeof( int );
         if( !output_file.Write( (char*)&top[chunk_index_min], len ) || len != sizeof( int ) )
            return( false );
         if( !chunks[chunk_index_min].Read( (char*)&top[chunk_index_min], len) || len == 0 )
           {
            empty[chunk_index_min] = true;
            chunks[chunk_index_min].Close();
           }
        }
     }
   return( true );
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
//--- 
   try
     {
      CExternalSort ext_sort;
      ext_sort.Sort( input_file_name, output_file_name );
     }
   catch( ... )
     {
      cerr << "unhandled exception caught";
      return( -1 );
     }
//--- ok
   return( 0 );
  }