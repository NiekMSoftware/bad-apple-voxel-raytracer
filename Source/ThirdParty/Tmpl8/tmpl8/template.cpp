#include "template.h"

// -----------------------------------------------------------
// Helper functions
// -----------------------------------------------------------

void FatalError( const char* fmt, ... )
{
	char t[16384];
	va_list args;
	va_start( args, fmt );
	vsnprintf( t, sizeof( t ), fmt, args );
	va_end( args );
	MessageBox( NULL, t, "Fatal Error", MB_OK );
	exit( 0 );
}

bool FileIsNewer( const char* file1, const char* file2 )
{
	struct _stat64i32 s1, s2;
	if (_stat( file1, &s1 ) != 0) return false;
	if (_stat( file2, &s2 ) != 0) return true;
	return s1.st_mtime > s2.st_mtime;
}

bool FileExists( const char* f )
{
	struct _stat64i32 s;
	return _stat( f, &s ) == 0;
}

bool RemoveFile( const char* f )
{
	return _unlink( f ) == 0;
}

string TextFileRead( const char* _File )
{
	ifstream s( _File );
	string str( (std::istreambuf_iterator<char>( s )), std::istreambuf_iterator<char>() );
	s.close();
	return str;
}

int LineCount( const string s )
{
	int lines = 0;
	for (uint i = 0; i < s.size(); i++) if (s[i] == 10) lines++;
	return lines;
}

void TextFileWrite( const string& text, const char* _File )
{
	ofstream s( _File, ios::binary );
	int size = (int)text.size();
	s.write( (char*)&text[0], size );
	s.close();
}
