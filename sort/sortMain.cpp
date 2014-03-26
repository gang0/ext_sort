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
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

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
const size_t BUFFER_SIZE = 64 * MB;
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
//| �������� ����                                      |
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
   //--- �������� �����
   int               m_mode;
   std::string       m_name;
   //--- �����
   FILE*             m_stream;
   //--- ������������� ������� � ������
   boost::mutex      m_stream_sync;
   //--- ���������� ������������ ����������
   bool              m_handler_complete;
   boost::mutex      m_handler_complete_sync;
   boost::condition_variable m_handler_complete_sync_event;

public:
   //--- �����������/����������
                     CBinFile();
                    ~CBinFile();
   //--- ��������/�������� �����
   bool              Open( const string &name, const int mode);
   void              Close();
   //--- ������/������ �� �����
   bool              Read( char* buffer, size_t* size );
   bool              Write( const char* buffer, size_t* size );
   //--- ����������� ������/������
   bool              ReadAsync( boost::asio::io_service &io, char* buffer, size_t* size );
   bool              WriteAsync( boost::asio::io_service &io, const char* buffer, size_t* size );
   void              Wait();
   //--- ����������� ��������� � ������
   void              SeekBegin() { if( m_stream != NULL ) fseek( m_stream, 0, SEEK_SET ); }
   //--- �������� �����
   void              Remove() { Close(); if( !m_name.empty() ) remove( m_name.c_str() ); }

private:
   const CBinFile&   operator=( const CBinFile &bin_file );
   //--- ��������� ������������ ������/������
   void              ReadAsyncHandler( char* buffer, size_t* size );
   void              WriteAsyncHandler( const char* buffer, size_t* size );
   void              AsyncHandlerCompleted() { m_handler_complete_sync.lock(); m_handler_complete = true; m_handler_complete_sync.unlock(); m_handler_complete_sync_event.notify_all(); }
  };
typedef vector<CBinFile*> CBinFilePtrArray;
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
CBinFile::CBinFile() : m_mode( 0 ), m_stream( nullptr ), m_handler_complete( false )
  {
  }
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
CBinFile::~CBinFile()
  {
   Close();
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
bool CBinFile::Open( const string &name, const int mode )
  {
   Close();
//--- ���������� ��������
   m_name = name;
   m_mode = mode;
//--- ��������� ���� � ������ ������
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
//| �������� �����                                     |
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
//| ������ �� �����                                    |
//+----------------------------------------------------+
bool CBinFile::Read( char* buffer, size_t* size )
  {
   if( buffer == NULL || size == NULL )
      return( false );
   if( m_stream == NULL )
      return( false );
   *size = fread( buffer, 1, *size, m_stream );
   return( true );
  }
//+----------------------------------------------------+
//| ������ � ����                                      |
//+----------------------------------------------------+
bool CBinFile::Write( const char* buffer, size_t* size )
  {
   if( buffer == NULL || size == NULL )
      return( false );
   if( m_stream == NULL )
      return( false );
   *size = fwrite( buffer, 1, *size, m_stream);
   return( true );
  }
//+----------------------------------------------------+
//| ����������� ������                                 |
//+----------------------------------------------------+
bool CBinFile::ReadAsync( boost::asio::io_service &io, char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return( false );
//--- TODO: ����� �� ��������� �������� ������ post
   io.post( boost::bind( &CBinFile::ReadAsyncHandler, this, buffer, size ) );
   return( true );
  }
//+----------------------------------------------------+
//| ����������� ������                                 |
//+----------------------------------------------------+
bool CBinFile::WriteAsync( boost::asio::io_service &io, const char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return( false );
//--- TODO: ��������� ������� ������ post
   io.post( boost::bind( &CBinFile::WriteAsyncHandler, this, buffer, size ) );
   return( true );
  }
//+----------------------------------------------------+
//| �������� ���������� ����������� ��������           |
//+----------------------------------------------------+
void CBinFile::Wait()
  {
   boost::unique_lock<boost::mutex> lock( m_handler_complete_sync );
//--- TODO: ������� �� wait_for
   if( !m_handler_complete )
     {
      m_handler_complete_sync_event.wait( lock );
      m_handler_complete = false;
     }
  }
//+----------------------------------------------------+
//| ���������� ������������ ������                     |
//+----------------------------------------------------+
void CBinFile::ReadAsyncHandler( char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return;
//--- ������ ������
   boost::lock_guard<boost::mutex> lock( m_stream_sync );
   Read( buffer, size );
   AsyncHandlerCompleted();
  }
//+----------------------------------------------------+
//|                                                    |
//+----------------------------------------------------+
void CBinFile::WriteAsyncHandler( const char* buffer, size_t* size )
  {
   if( buffer == nullptr || size == nullptr )
      return;
//--- ����� ������
   boost::lock_guard<boost::mutex> lock( m_stream_sync );
   Write( buffer, size );
   AsyncHandlerCompleted();
  }
//+----------------------------------------------------+
//| ����� ����� � ������������� �������                |
//+----------------------------------------------------+
class CChunk
  {
private:
   CBinFile*         m_file;
   std::unique_ptr<char[]> m_data;
   std::unique_ptr<char[]> m_buffer;
   size_t            m_data_max;
   size_t            m_data_len;
   size_t            m_buffer_len;
   size_t            m_data_current;

public:
   //--- �����������
                     CChunk( const size_t buffer_size ) : m_file( nullptr ), m_data( new char[buffer_size] ), m_buffer( new char[buffer_size] ), m_data_max( buffer_size ), m_data_len( 0 ), m_buffer_len( 0 ), m_data_current( 0 ) {}
   //--- ��������� ����� �����
   bool              SetFile( CBinFile* file );
   //--- �������������
   bool              Initialize( boost::asio::io_service &io );
   void              Wait() { m_file->Wait(); }
   //--- ��������� ���������� �������� �� �����
   bool              NextItem( boost::asio::io_service &io, int &item );
  };
//+----------------------------------------------------+
//| ��������� ����� �����                              |
//+----------------------------------------------------+
bool CChunk::SetFile( CBinFile* file )
  {
   if( file == nullptr )
      return( false );
   m_file = file;
//--- ������������� ������ �� ������ ����� � ������ �����
   m_file->SeekBegin();
//--- 
   m_data_len = 0;
   m_buffer_len = 0;
   m_data_current = 0;
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| �������������                                      |
//+----------------------------------------------------+
bool CChunk::Initialize( boost::asio::io_service &io )
  {
   if( m_file == nullptr )
      return( false );
   m_data_len = m_data_max;
   return( m_file->ReadAsync( io, (char*)m_data.get(), &m_data_len ) );
  }
//+----------------------------------------------------+
//| ��������� ���������� �������� �� �����             |
//+----------------------------------------------------+
bool CChunk::NextItem( boost::asio::io_service &io, int &item )
  {
//--- ���� ����� �� ����� ������, ������
   if( m_data_current == m_data_len )
      return( false );
//--- ���� ������ ������, ���������� ������ ��������� �����
   if( m_data_current == 0 )
     {
      m_buffer_len = m_data_max;
      m_file->ReadAsync( io, m_buffer.get(), &m_buffer_len );
     }
//--- ������ ��������� �������
   item = *(int*)&m_data[m_data_current];
   m_data_current += sizeof( int );
//--- ���� ����� �� ����� ������ ������ ������
   if( m_data_current >= m_data_len )
     {
      m_file->Wait();
      m_data.swap( m_buffer );
      m_data_len = m_buffer_len;
      m_data_current = 0;
     }
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| ������� ����������                                 |
//+----------------------------------------------------+
class CExternalSort
  {
private:
   boost::asio::io_service &m_io_service;
   CBinFilePtrArray  m_chunks;

public:
   //--- �����������/����������
                     CExternalSort( boost::asio::io_service &io ) : m_io_service( io ) {};
   //--- ���������� ����� <input_file_name>, ��������� � ����� <output_file_name>
   void              Sort( std::string input_file_name, std::string output_file_name );

private:
   //--- ��������� �������� ���� �� ��������������� �����
   bool              Split( CBinFile &input_file, CBinFilePtrArray &chunks );
   //--- ������� ��������������� ����� � �������� ����
   bool              Merge( CBinFilePtrArray &chunks, CBinFile &output_file );
  };
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
void CExternalSort::Sort( std::string input_file_name, std::string output_file_name )
  {
//--- ��������� ������� ����
   CBinFile input_file;
   if( !input_file.Open( input_file_name, CBinFile::MODE_READ ) )
      return;
//--- ��������� �������� ����
   CBinFile output_file;
   if( !output_file.Open( output_file_name, CBinFile::MODE_WRITE ) )
      return;
//--- ��������� ������� ���� �� ������������� �����
   if( !Split( input_file, m_chunks ) )
      return;
//--- ������� ����� � �������� ����
   Merge( m_chunks, output_file );
  }
//+----------------------------------------------------+
//| ��������� �������� ���� �� ��������������� �����   |
//+----------------------------------------------------+
bool CExternalSort::Split( CBinFile &input_file, CBinFilePtrArray &chunks )
  {
   CAutoTimer timer( "Split" );
//--- TODO: ��������� ������� ������ ��� ������
   char* buffer = new( std::nothrow ) char[BUFFER_SIZE];
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
//--- ��������� ������ ������ �����
   input_file.ReadAsync( m_io_service, buffer, &buffer_len );
   input_file.Wait();
   while( buffer_len > 0 )
     {
      //--- ������ ��������� ������ ������
      input_file.ReadAsync( m_io_service, buffer_read, &buffer_read_len );
      //--- ������������ ������� ������ ������
      std::sort( ( int* )buffer, ( int* ) ( buffer + buffer_len ) );
      //--- ������� ���� � ������
      //--- ��������� ����� ����
      chunks.push_back( new CBinFile() );
      CBinFile &chunk = *chunks[chunks.size() - 1];
      std::ostringstream chunk_name;
      chunk_name << "chunk_" << chunks.size() - 1 << ".dat";
      if( !chunk.Open( chunk_name.str(), CBinFile::MODE_RW | CBinFile::MODE_TEMP ) )
         return( false );
      //--- ���������� ���������� ���� � ����
      //--- ���� ���������� ������ ����������� �����
      if( chunks.size() > 1 )
         chunks[chunks.size() - 2]->Wait();
      char* buffer_temp = buffer_write;
      buffer_write = buffer;
      buffer_write_len = buffer_len;
      buffer = buffer_temp;
      buffer_len = 0;
      chunk.WriteAsync( m_io_service, buffer_write, &buffer_write_len );
      //--- ������ ������
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
//| ������� ��������������� ����� � �������� ����      |
//+----------------------------------------------------+
struct ChunkItem 
  {
   int               item;
   UINT              index;
   bool              operator<( const ChunkItem &chunk_item ) const { return( this->item > chunk_item.item ); } // NOTE: ���������� ��������, ������� �� ������� ������� ����������
  };
bool CExternalSort::Merge( CBinFilePtrArray &chunks_files, CBinFile &output_file )
  {
   CAutoTimer timer( "Merge" );
   bool empty[CHUNKS_MAX]={0};
   std::vector<CChunk*> chunks;
   std::priority_queue<ChunkItem> top;
//--- �������������� �����, ���������� �������� ������
   chunks.reserve( chunks_files.size() );
   for( int chunk_index = 0; chunk_index < chunks_files.size(); chunk_index++ )
     {
      //--- TODO: ��������� ������ �����
      CChunk* chunk = new CChunk( BUFFER_SIZE / ( 8 ) );
      chunk->SetFile( chunks_files[chunk_index] );
      chunk->Initialize( m_io_service );
      chunks.push_back( chunk );
     }
//--- ��������� ����
   for( int chunk_index=0; chunk_index < chunks.size(); chunk_index++ )
     {
      //--- ���������� ���������� �������������
      chunks[chunk_index]->Wait();
      //--- �������� ������ ������� �����
      ChunkItem item;
      item.index = chunk_index;
      chunks[chunk_index]->NextItem( m_io_service, item.item );
      top.push( item );
     }
   while( !top.empty() )
     {
      ChunkItem item = top.top();
      top.pop();
        {
         size_t len = sizeof( int );
         if( !output_file.Write( (char*)&item.item, &len ) || len != sizeof( int ) )
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
//--- ������� ���������
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
      //--- �������������� ������������� ���������
      boost::asio::io_service io;
      CExternalSort ext_sort( io );
      io.post( boost::bind( &CExternalSort::Sort, &ext_sort, input_file_name, output_file_name ) );
      //--- ������� ��� �������
      boost::thread_group threads_pool;
      int threads_count = boost::thread::hardware_concurrency() * 2 - 1;
      for( int thread_index = 0; thread_index < threads_count; thread_index++ )
         threads_pool.create_thread( boost::bind( &boost::asio::io_service::run, &io ) );
      //--- 
      io.run();
     }
   catch( std::exception &ex )
     {
      cerr << "unhandled exception caught: " << ex.what();
      return( -1 );
     }
//--- ok
   return( 0 );
  }