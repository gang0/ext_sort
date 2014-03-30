//+----------------------------------------------------+
//| 22.03.2014                                         |
//+----------------------------------------------------+
//--- stl
#include <iostream>
#include <fstream>
#include <string>
//+----------------------------------------------------+
//| Parameters                                         |
//+----------------------------------------------------+
bool parameters( const char* first_file_name_arg, const char* second_file_name_arg, std::string &first_file_name, std::string &second_file_name )
  {
   if( first_file_name_arg == NULL || second_file_name_arg == NULL )
      return( false );
//--- name
   first_file_name = first_file_name_arg;
   second_file_name = second_file_name_arg;
   return( true );
  }
//+----------------------------------------------------+
//| Usage                                              |
//+----------------------------------------------------+
void usage()
  {
   std::cout << "Usage: comp <first_file_name> <second_file_name>" << std::endl;
  }
//+----------------------------------------------------+
//| Main function                                      |
//+----------------------------------------------------+
int main(int argc,char** argv)
  {
//--- получим параметры
   if( argc != 3 )
     {
      std::cerr << "invalid parameters" << std::endl;
      usage();
      return( -1 );
     }
   std::string first_file_name, second_file_name;
   if( !parameters( argv[1], argv[2], first_file_name, second_file_name ) )
     {
      std::cerr << "failed to read parameters" << std::endl;
      return( -1 );
     }
//--- сравнение двух бинарных файлов
   try
     {
      bool equal = true;
      std::ifstream first_file( first_file_name, std::ios_base::in | std::ios_base::binary );
      std::ifstream second_file( second_file_name, std::ios_base::in | std::ios_base::binary);
      unsigned first = 0;
      unsigned second = 0;
      while( first_file.read( (char*) &first, sizeof( first ) ) )
        {
         if( !second_file.read( (char*) &second, sizeof( second ) ) || first != second )
           {
            equal = false;
            break;
           }
        }
      if( equal )
         std::cout << "equal" << std::endl;
      else
         std::cout << "different" << std::endl;
     }
   catch( std::exception &ex )
     {
      std::cerr << "unhandled exception caught: " << ex.what() << std::endl;
      return( -1 );
     }
//--- ok
   return( 0 );
  }