

#include	"File.h"

#include	<string.h>

#include	<vector>
using namespace std;



class LineRec {

public:
	char	line[2048];
	int		id, x, y, z;

public:
	bool Get( FILE *f );
};



bool LineRec::Get( FILE *f )
{
	if( !fgets( line, sizeof(line), f ) )
		return false;

	char	*u = strrchr( line, '_' );

	if( !u )
		return false;

	if( 4 != sscanf( u+1, "%dex.mrc\t%d\t%d\t%d", &id, &x, &y, &z ) )
		return false;

	return true;
}


static void FlushPends( vector<LineRec> &pends, FILE *fou )
{
	int	np = pends.size();

	if( np ) {

		for( int i = 0; i < np; ++i )
			fprintf( fou, pends[i].line );

		pends.clear();
	}
}


int main( int argc, char **argv )
{
// open infile

	char	buf[2048];
	FILE	*fin = FileOpenOrDie( argv[1], "r" );

// set s to point at extension or trailing NULL

	char	*s = strrchr( argv[1], '.' );

	if( !s )
		s = argv[1] + strlen( argv[1] );

// compose outname & open

	sprintf( buf, "%.*s_v2%s", s - argv[1], argv[1], s );
	FILE	*fou = FileOpenOrDie( buf, "w" );

// read lines

	vector<LineRec>	pends;
	int				zcur = -1;

	for(;;) {

		LineRec	L;

		if( !L.Get( fin ) ) {

			FlushPends( pends, fou );
			break;
		}
		else if( L.z != zcur ) {

			FlushPends( pends, fou );
			pends.push_back( L );
			zcur = L.z;
		}
		else if( !pends.size() )
			fprintf( fou, L.line );
		else if( L.id < pends[0].id ) {
			pends.clear();
			fprintf( fou, L.line );
		}
		else if( L.id == pends[pends.size()-1].id )
			;
		else
			pends.push_back( L );
	}

// close files

exit:
	fclose( fin );
	fclose( fou );
}

