//+----------------------------------------------------+
//| ����� ����� � ����������� ������������             |
//+----------------------------------------------------+
class CBufferedAsyncFile
  {
public:
   //--- �����������
   typedef unique_ptr<CBufferedAsyncFile> Ptr;
   typedef std::vector<Ptr> PtrArray;

private:
   //--- ����
   CBinFile          m_file;
   //--- ����������� ������
   boost::asio::io_service &m_io_service;
   //--- �����
   std::unique_ptr<char[]> m_buffer;
   const size_t      m_buffer_size;
   //--- ������ ������ � ������
   size_t            m_data_size;
   //--- ���������� ����������� IO
   bool              m_completed;
   boost::mutex      m_completed_sync;
   boost::condition_variable m_completed_cond;

public:
                     CBufferedAsyncFile( boost::asio::io_service &io, const size_t buffer_size );
                    ~CBufferedAsyncFile();
   //--- ��������/�������� �����
   bool              Open( const std::string &file_name, const int mode );
   void              Close();
   //--- ������ ������
   size_t            Read( std::unique_ptr<char[]> &buffer );
   //--- ������ ������
   void              Write( std::unique_ptr<char[]> &buffer, size_t data_size );

private:
   //--- �����������/�������� ���������� ����������� ��������
   void              AsyncComplete();
   void              AsyncWait();
   //--- ����������� ������
   void              ReadAsync();
   void              ReadAsyncHandler();
   //--- ����������� ������
   void              WriteAsync();
   void              WriteAsyncHandler();
  };
//+----------------------------------------------------+
//| �����������                                        |
//+----------------------------------------------------+
CBufferedAsyncFile::CBufferedAsyncFile( boost::asio::io_service &io, const size_t buffer_size ) : m_io_service( io ), m_buffer( new char[buffer_size] ), m_buffer_size( buffer_size ), m_data_size( 0 ), m_completed( true )
  {
  }
//+----------------------------------------------------+
//| ����������                                         |
//+----------------------------------------------------+
CBufferedAsyncFile::~CBufferedAsyncFile()
  {
   Close();
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
bool CBufferedAsyncFile::Open( const std::string &file_name, const int mode )
  {
   Close();
//--- ��������� ����
   if( !m_file.Open( file_name, mode ) )
      return( false );
//--- ���� ���� ������ ��� ������ ��������� �����
   if( mode & CBinFile::MODE_READ )
      ReadAsync();
//--- ok
   return( true );
  }
//+----------------------------------------------------+
//| �������� �����                                     |
//+----------------------------------------------------+
void CBufferedAsyncFile::Close()
  {
//--- ������ ��������� ���������� ����� ����������� ��������� ����� ��������� �����
   AsyncWait();
   m_file.Close();
  }
//+----------------------------------------------------+
//| ������ ������                                      |
//+----------------------------------------------------+
size_t CBufferedAsyncFile::Read( std::unique_ptr<char[]> &buffer )
  {
//--- ���� ���������� ����������� ��������
   AsyncWait();
//--- ������ ������
   buffer.swap( m_buffer );
   size_t data_size = m_data_size;
//--- ��������� ����������� ��������
   ReadAsync();
//--- ���������� ����� ������
   return( data_size );
  }
//+----------------------------------------------------+
//| ������ ������                                      |
//+----------------------------------------------------+
void CBufferedAsyncFile::Write( std::unique_ptr<char[]> &buffer, size_t data_size )
  {
//--- ���� ���������� ����������� ��������
   AsyncWait();
//--- ������ ������
   m_buffer.swap( buffer );
   m_data_size = data_size;
//--- ��������� ����������� ��������
   WriteAsync();
  }
//+----------------------------------------------------+
//| ����������� � ���������� ����������� ��������      |
//+----------------------------------------------------+
void CBufferedAsyncFile::AsyncComplete()
  {
   m_completed_sync.lock();
   m_completed = true;
   m_completed_sync.unlock();
   m_completed_cond.notify_all();
  }
//+----------------------------------------------------+
//| �������� ���������� ����������� ��������           |
//+----------------------------------------------------+
void CBufferedAsyncFile::AsyncWait()
  {
   boost::unique_lock<boost::mutex> lock( m_completed_sync );
//--- TODO: ������� �� ��������
   if( !m_completed )
      m_completed_cond.wait( lock );
  }
//+----------------------------------------------------+
//| ����������� ������                                 |
//+----------------------------------------------------+
void CBufferedAsyncFile::ReadAsync()
  {
   m_completed = false;
   m_data_size = 0;
   m_io_service.post( boost::bind( &CBufferedAsyncFile::ReadAsyncHandler, this ) );
  }
//+----------------------------------------------------+
//| ���������� ������������ ������                     |
//+----------------------------------------------------+
void CBufferedAsyncFile::ReadAsyncHandler()
  {
//--- ������ ���� ������ � �����
   m_data_size = m_file.Read( m_buffer.get(), m_buffer_size );
//--- ���������� � ���������� ��������
   AsyncComplete();
  }
//+----------------------------------------------------+
//| ����������� ������                                 |
//+----------------------------------------------------+
void CBufferedAsyncFile::WriteAsync()
  {
   m_completed = false;
   m_io_service.post( boost::bind( &CBufferedAsyncFile::WriteAsyncHandler, this ) );
  }
//+----------------------------------------------------+
//| ���������� ����������� ������                      |
//+----------------------------------------------------+
void CBufferedAsyncFile::WriteAsyncHandler()
  {
//--- ���������� ���� ������ �� ������
   m_file.Write( m_buffer.get(), m_data_size );
//--- ���������� � ���������� ��������
   AsyncComplete();
  }
//+----------------------------------------------------+
