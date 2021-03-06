//+----------------------------------------------------+
//| Author: Maxim Ulyanov <ulyanov.maxim@gmail.com>    |
//+----------------------------------------------------+
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
      MODE_TEMP = 0x04
     };

private:
   //--- �������� �����
   std::string       m_name;
   int               m_mode;
   //--- �����
   FILE*             m_stream;

public:
   //--- �����������/����������
                     CBinFile();
                    ~CBinFile();
   //--- ��������
   std::string       Name() { return( m_name ); }
   int               Mode() { return( m_mode ); }
   //--- ��������/�������� �����
   bool              Open( const std::string &name, const int mode);
   void              Close();
   //--- ������/������ �� �����
   size_t            Read( char* buffer, const size_t buffer_size );
   size_t            Write( const char* buffer, const size_t buffer_size );
   //--- �������� �����
   void              Remove() { Close(); if( !m_name.empty() ) remove( m_name.c_str() ); }
  };
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
CBinFile::CBinFile() : m_mode( 0 ), m_stream( nullptr )
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
bool CBinFile::Open( const std::string &name, const int mode )
  {
   Close();
//--- ���������� ��������
   m_name = name;
   m_mode = mode;
//--- ��������� ���� � ������ ������
   const char* mode_str = nullptr;
   if( mode & MODE_WRITE )
      if( mode & MODE_READ ) mode_str = "w+b";
      else
         mode_str = "wSb";
   else
      mode_str = "rSb";
   if( ( m_stream = fopen( name.c_str(), mode_str) ) == nullptr )
      return( false );
//--- 
   return( true );
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
void CBinFile::Close()
  {
   if( m_stream != nullptr )
     {
      fclose( m_stream );
      m_stream = nullptr;
      if( m_mode & MODE_TEMP )
         Remove();
     }
  }
//+----------------------------------------------------+
//| ������ �� �����                                    |
//+----------------------------------------------------+
size_t CBinFile::Read( char* buffer, const size_t buffer_size )
  {
   if( buffer == nullptr || buffer_size == 0 )
      return( 0 );
   if( m_stream == nullptr )
      return( 0 );
   return( fread( buffer, 1, buffer_size, m_stream ) );
  }
//+----------------------------------------------------+
//| ������ � ����                                      |
//+----------------------------------------------------+
size_t CBinFile::Write( const char* buffer, const size_t buffer_size )
  {
   if( buffer == nullptr || buffer_size == 0 )
      return( 0 );
   if( m_stream == nullptr )
      return( 0 );
   return( fwrite( buffer, 1, buffer_size, m_stream ) );
  }
//+----------------------------------------------------+
