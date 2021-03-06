//
// Convert Z-file tform data to viewable text or xml.
//
// Z-file organized data can be:
// - IDB (inpath not specified)
// - X_A_TXT, X_H_TXT
// - X_A_MET, X_H_MET
// - X_A_BIN, X_H_BIN
//

#include	"Cmdline.h"
#include	"Disk.h"
#include	"File.h"
#include	"PipeFiles.h"
#include	"TrakEM2_UTL.h"
#include	"Timer.h"

#include	<string.h>

using namespace ns_pipergns;


/* --------------------------------------------------------------- */
/* CArgs --------------------------------------------------------- */
/* --------------------------------------------------------------- */

class CArgs {

public:
	string		idb;
	const char	*inpath,
				*meta;
	double		forcew,
				forceh,
				degcw,
				xml_trim;
	int			zilo,
				zihi,
				type,	// {'T','M','X','B'}
				xml_type,
				xml_min,
				xml_max;

public:
	CArgs()
	{
		inpath		= NULL;
		meta		= NULL;
		forcew		= 0.0;
		forceh		= 0.0;
		degcw		= 0.0;
		xml_trim	= 0.0;
		zilo		= 0;
		zihi		= 32768;
		type		= 'X';
		xml_type	= 0;
		xml_min		= 0;
		xml_max		= 0;
	};

	void SetCmdLine( int argc, char* argv[] );
};

/* --------------------------------------------------------------- */
/* CMeta --------------------------------------------------------- */
/* --------------------------------------------------------------- */

class CMeta {
// Tool to map paths from idb back to original mrc images.
// This is known from the layout meta file we started with.
private:
	FILE			*f;
	CLineScan		LS;
	map<int,string>	mz;
public:
	CMeta() : f(NULL) {};
	virtual ~CMeta();
	void Init( const char *meta );
	void MapZ( int z );
	const char* Path( const Til2Img* t2i );
};

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */

static CArgs	gArgs;
static CMeta	gMeta;
static FILE*	flog = NULL;
static Rgns		R;
static int		gW, gH;
static bool		isAff	= true;






/* --------------------------------------------------------------- */
/* SetCmdLine ---------------------------------------------------- */
/* --------------------------------------------------------------- */

void CArgs::SetCmdLine( int argc, char* argv[] )
{
// start log

	flog = FileOpenOrDie( "xview.log", "w" );

// log start time

	time_t	t0 = time( NULL );
	char	atime[32];

	strcpy( atime, ctime( &t0 ) );
	atime[24] = '\0';	// remove the newline

	fprintf( flog, "Start: %s ", atime );

// parse command line args

	if( argc < 3 ) {
		printf(
		"Usage: xview inpath -idb=idbpath -z=i,j\n" );
		exit( 42 );
	}

	vector<double>	vd;
	vector<int>		vi;
	const char		*pchar;

	for( int i = 1; i < argc; ++i ) {

		// echo to log
		fprintf( flog, "%s ", argv[i] );

		if( argv[i][0] != '-' ) {
			inpath = argv[i];
			isAff = (NULL != strstr( FileNamePtr( inpath ), "X_A" ));
		}
		else if( GetArgStr( pchar, "-idb=", argv[i] ) )
			idb = pchar;
		else if( GetArgStr( meta, "-meta=", argv[i] ) )
			;
		else if( GetArgList( vd, "-forceWH=", argv[i] ) ) {

			if( 2 == vd.size() ) {
				forcew = vd[0];
				forceh = vd[1];
				fprintf( flog, "WH [%f %f]\n", forcew, forceh );
			}
			else {
				fprintf( flog,
				"Bad format in -forceWH [%s].\n", argv[i] );
				exit( 42 );
			}
		}
		else if( GetArg( &degcw, "-degcw=%lf", argv[i] ) )
			;
		else if( GetArg( &type, "-type=%c", argv[i] ) )
			;
		else if( GetArgList( vi, "-z=", argv[i] ) ) {

			if( 2 == vi.size() ) {
				zilo = vi[0];
				zihi = vi[1];
				fprintf( flog, "z [%d %d]\n", zilo, zihi );
			}
			else {
				fprintf( flog,
				"Bad format in -z [%s].\n", argv[i] );
				exit( 42 );
			}
		}
		else if( GetArg( &xml_trim, "-xmltrim=%lf", argv[i] ) )
			;
		else if( GetArg( &xml_type, "-xmltype=%d", argv[i] ) )
			;
		else if( GetArg( &xml_min, "-xmlmin=%d", argv[i] ) )
			;
		else if( GetArg( &xml_max, "-xmlmax=%d", argv[i] ) )
			;
		else {
			printf( "Did not understand option [%s].\n", argv[i] );
			exit( 42 );
		}
	}

	fprintf( flog, "\n\n" );
	fflush( flog );
}

/* --------------------------------------------------------------- */
/* CMeta --------------------------------------------------------- */
/* --------------------------------------------------------------- */

// Just close the file, a formality.
//
CMeta::~CMeta()
{
	if( f )
		fclose( f );
}


// Open file and get ready (cache one line from next z-block).
//
void CMeta::Init( const char *meta )
{
// Open file

	if( !meta )
		return;

	f = FileOpenOrDie( meta, "r" );

// Cache first line

	if( LS.Get( f ) <= 0 ) {
		fprintf( flog, "Meta: Empty file [%s].\n", meta );
		exit( 42 );
	}
}


// Read lines for this z into map<id,path>.
//
// End with one line from next z-block.
//
void CMeta::MapZ( int z )
{
	if( !f )
		return;

	mz.clear();

// Gone too far?

	int	linez;

	sscanf( LS.line, "%d", &linez );

	if( linez > z ) {
not_found:;
		fprintf( flog, "Meta: Z [%d] not found.\n", z );
		exit( 42 );
	}

// Move up to requested z

	while( linez < z ) {

		if( LS.Get( f ) <= 0 )
			goto not_found;

		sscanf( LS.line, "%d", &linez );

		if( linez > z )
			goto not_found;
	}

// Map lines while this z

	char	path[2048];
	int		id;

	do {

		// map this line

		sscanf( LS.line,
			"%d%d"
			"%*lf%*lf%*lf%*lf%*lf%*lf"
			"%*d%*d%*d%s\n",
			&linez, &id, path );

		mz[id] = path;

		// get next one, or make fake one with huge z

		if ( LS.Get( f ) > 0 )
			sscanf( LS.line, "%d", &linez );
		else
			sprintf( LS.line, "%d", linez = 0x7FFFFFFF );

	} while( linez == z );
}


// Return path pointer either from meta data or idb.
//
const char* CMeta::Path( const Til2Img* t2i )
{
	if( f ) {

		map<int,string>::iterator	it = mz.find( t2i->id );

		if( it != mz.end() )
			return it->second.c_str();
		else {
			fprintf( flog, "Meta: missing line for [%s].\n",
			FileNamePtr( t2i->path.c_str() ) );
			exit( 42 );
		}
	}
	else
		return t2i->path.c_str();
}

/* --------------------------------------------------------------- */
/* GetXY_Aff ----------------------------------------------------- */
/* --------------------------------------------------------------- */

static void GetXY_Aff( DBox &B, const TAffine &Trot )
{
	B.L = BIGD;
	B.R = -BIGD;
	B.B = BIGD;
	B.T = -BIGD;

	TAffine			T;
	vector<Point>	cnr;
	Set4Corners( cnr, gW, gH );

	for( int z = gArgs.zilo; z <= gArgs.zihi; ++z ) {

		if( !R.Init( gArgs.idb, z, flog ) )
			continue;

		if( !R.Load( gArgs.inpath ) )
			continue;

		for( int j = 0; j < R.nr; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			vector<Point>	c( 4 );
			memcpy( &c[0], &cnr[0], 4*sizeof(Point) );
			T = Trot * X_AS_AFF( R.x, j );
			T.Transform( c );

			for( int k = 0; k < 4; ++k ) {
				B.L = fmin( B.L, c[k].x );
				B.R = fmax( B.R, c[k].x );
				B.B = fmin( B.B, c[k].y );
				B.T = fmax( B.T, c[k].y );
			}
		}
	}

	B.R = ceil( B.R - B.L + 1 );
	B.T = ceil( B.T - B.B + 1 );
}

/* --------------------------------------------------------------- */
/* GetXY_Hmy ----------------------------------------------------- */
/* --------------------------------------------------------------- */

static void GetXY_Hmy( DBox &B, const THmgphy &Trot )
{
	B.L = BIGD;
	B.R = -BIGD;
	B.B = BIGD;
	B.T = -BIGD;

	THmgphy			T;
	vector<Point>	cnr;
	Set4Corners( cnr, gW, gH );

	for( int z = gArgs.zilo; z <= gArgs.zihi; ++z ) {

		if( !R.Init( gArgs.idb, z, flog ) )
			continue;

		if( !R.Load( gArgs.inpath ) )
			continue;

		for( int j = 0; j < R.nr; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			vector<Point>	c( 4 );
			memcpy( &c[0], &cnr[0], 4*sizeof(Point) );
			T = Trot * X_AS_HMY( R.x, j );
			T.Transform( c );

			for( int k = 0; k < 4; ++k ) {
				B.L = fmin( B.L, c[k].x );
				B.R = fmax( B.R, c[k].x );
				B.B = fmin( B.B, c[k].y );
				B.T = fmax( B.T, c[k].y );
			}
		}
	}

	B.R = ceil( B.R - B.L + 1 );
	B.T = ceil( B.T - B.B + 1 );
}

/* --------------------------------------------------------------- */
/* Update_Aff ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static void Update_Aff( const TAffine &Trot, const DBox &B )
{
	for( int j = 0; j < R.nr; ++j ) {

		if( !FLAG_ISUSED( R.flag[j] ) )
			continue;

		TAffine&	T = X_AS_AFF( R.x, j );

		T = Trot * T;
		T.AddXY( -B.L, -B.B );
	}
}

/* --------------------------------------------------------------- */
/* Update_Hmy ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static void Update_Hmy( const THmgphy &Trot, const DBox &B )
{
	THmgphy	M( 1,0,-B.L, 0,1,-B.B, 0,0 );

	for( int j = 0; j < R.nr; ++j ) {

		if( !FLAG_ISUSED( R.flag[j] ) )
			continue;

		THmgphy&	T = X_AS_HMY( R.x, j );

		T = M * (Trot * T);
	}
}

/* --------------------------------------------------------------- */
/* WriteM_Aff ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteM_Aff()
{
	char	buf[64];
	sprintf( buf, "X_A_MET/X_A_%d.txt", R.z );
	FILE	*f = FileOpenOrDie( buf, "w", flog );

	map<int,int>::iterator	mi, en = R.m.end();

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			TAffine&	T = X_AS_AFF( R.x, j );

			fprintf( f,
			"%d\t%d"
			"\t%f\t%f\t%f\t%f\t%f\t%f"
			"\t%d\t%d\t%d\t%s\n",
			id, j - j0 + 1,
			T.t[0], T.t[1], T.t[2], T.t[3], T.t[4], T.t[5],
			t2i->col, t2i->row, t2i->cam, gMeta.Path( t2i ) );
		}
	}

	fclose( f );
}

/* --------------------------------------------------------------- */
/* WriteM_Hmy ---------------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteM_Hmy()
{
	char	buf[64];
	sprintf( buf, "X_H_MET/X_H_%d.txt", R.z );
	FILE	*f = FileOpenOrDie( buf, "w", flog );

	map<int,int>::iterator	mi, en = R.m.end();

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			THmgphy&	T = X_AS_HMY( R.x, j );

			fprintf( f,
			"%d\t%d"
			"\t%f\t%f\t%f\t%f\t%f\t%f\t%.12g\t%.12g"
			"\t%d\t%d\t%d\t%s\n",
			id, j - j0 + 1,
			T.t[0], T.t[1], T.t[2],
			T.t[3], T.t[4], T.t[5],
			T.t[6], T.t[7],
			t2i->col, t2i->row, t2i->cam, gMeta.Path( t2i ) );
		}
	}

	fclose( f );
}

/* --------------------------------------------------------------- */
/* WriteXMLHead -------------------------------------------------- */
/* --------------------------------------------------------------- */

static FILE* WriteXMLHead( int &oid, const DBox &B )
{
	FILE	*f = FileOpenOrDie(
					(isAff ? "Affine.xml" : "Hmgphy.xml"),
					"w", flog );

	oid = 3;

	fprintf( f, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n" );

	TrakEM2WriteDTD( f );

	fprintf( f, "<trakem2>\n" );

	fprintf( f,
	"\t<project\n"
	"\t\tid=\"0\"\n"
	"\t\ttitle=\"Project\"\n"
	"\t\tmipmaps_folder=\"trakem2.mipmaps/\"\n"
	"\t\tn_mipmap_threads=\"8\"\n"
	"\t/>\n" );

	fprintf( f,
	"\t<t2_layer_set\n"
	"\t\toid=\"%d\"\n"
	"\t\ttransform=\"matrix(1,0,0,1,0,0)\"\n"
	"\t\ttitle=\"Top level\"\n"
	"\t\tlayer_width=\"%.2f\"\n"
	"\t\tlayer_height=\"%.2f\"\n"
	"\t>\n",
	oid++, B.R, B.T );

	return f;
}

/* --------------------------------------------------------------- */
/* WriteXMLLyr_Aff ----------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteXMLLyr_Aff( FILE *f, int &oid )
{
	map<int,int>::iterator	mi, en = R.m.end();
	int						offset = int(2 * gArgs.xml_trim + 0.5);

	fprintf( f,
	"\t\t<t2_layer\n"
	"\t\t\toid=\"%d\"\n"
	"\t\t\tthickness=\"0\"\n"
	"\t\t\tz=\"%d\"\n"
	"\t\t>\n",
	oid++, R.z );

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			char	title[128];

			if( t2i->col != -999 ) {
				sprintf( title, "%d.%d-%d_%d.%d.%d",
					R.z, id, j - j0 + 1,
					t2i->col, t2i->row, t2i->cam );
			}
			else
				sprintf( title, "%d.%d-%d", R.z, id, j - j0 + 1 );

			TAffine&	T = X_AS_AFF( R.x, j );

			// fix origin : undo trimming
			Point	o( gArgs.xml_trim, gArgs.xml_trim );
			T.Transform( o );

			fprintf( f,
			"\t\t\t<t2_patch\n"
			"\t\t\t\toid=\"%d\"\n"
			"\t\t\t\twidth=\"%d\"\n"
			"\t\t\t\theight=\"%d\"\n"
			"\t\t\t\ttransform=\"matrix(%f,%f,%f,%f,%f,%f)\"\n"
			"\t\t\t\ttitle=\"%s\"\n"
			"\t\t\t\ttype=\"%d\"\n"
			"\t\t\t\tfile_path=\"%s\"\n"
			"\t\t\t\to_width=\"%d\"\n"
			"\t\t\t\to_height=\"%d\"\n",
			oid++, gW - offset, gH - offset,
			T.t[0], T.t[3], T.t[1], T.t[4], o.x, o.y,
			title, gArgs.xml_type, gMeta.Path( t2i ),
			gW - offset, gH - offset );

			if( gArgs.xml_min < gArgs.xml_max ) {

				fprintf( f,
				"\t\t\t\tmin=\"%d\"\n"
				"\t\t\t\tmax=\"%d\"\n"
				"\t\t\t/>\n",
				gArgs.xml_min, gArgs.xml_max );
			}
			else
				fprintf( f, "\t\t\t/>\n" );
		}
	}

	fprintf( f, "\t\t</t2_layer>\n" );
}

/* --------------------------------------------------------------- */
/* TopLeft ------------------------------------------------------- */
/* --------------------------------------------------------------- */

static void TopLeft( Point &o, const THmgphy &T )
{
	vector<Point>	cnr( 4 );

	cnr[0] = Point(      gArgs.xml_trim,      gArgs.xml_trim );
	cnr[1] = Point( gW-1-gArgs.xml_trim,      gArgs.xml_trim );
	cnr[2] = Point( gW-1-gArgs.xml_trim, gH-1-gArgs.xml_trim );
	cnr[3] = Point(      gArgs.xml_trim, gH-1-gArgs.xml_trim );

	T.Transform( cnr );

	o.x = BIGD;
	o.y = BIGD;

	for( int k = 0; k < 4; ++k ) {
		o.x = fmin( o.x, cnr[k].x );
		o.y = fmin( o.y, cnr[k].y );
	}
}

/* --------------------------------------------------------------- */
/* WriteXMLLyr_Hmy ----------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteXMLLyr_Hmy( FILE *f, int &oid )
{
	map<int,int>::iterator	mi, en = R.m.end();
	int						offset = int(2 * gArgs.xml_trim + 0.5);

	fprintf( f,
	"\t\t<t2_layer\n"
	"\t\t\toid=\"%d\"\n"
	"\t\t\tthickness=\"0\"\n"
	"\t\t\tz=\"%d\"\n"
	"\t\t>\n",
	oid++, R.z );

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			char	title[128];

			if( t2i->col != -999 ) {
				sprintf( title, "%d.%d-%d_%d.%d.%d",
					R.z, id, j - j0 + 1,
					t2i->col, t2i->row, t2i->cam );
			}
			else
				sprintf( title, "%d.%d-%d", R.z, id, j - j0 + 1 );

			THmgphy&	T = X_AS_HMY( R.x, j );

			// fix origin : undo trimming
			Point	o;
			TopLeft( o, T );

			fprintf( f,
			"\t\t\t<t2_patch\n"
			"\t\t\t\toid=\"%d\"\n"
			"\t\t\t\twidth=\"%d\"\n"
			"\t\t\t\theight=\"%d\"\n"
			"\t\t\t\ttransform=\"matrix(1,0,0,1,%f,%f)\"\n"
			"\t\t\t\ttitle=\"%s\"\n"
			"\t\t\t\ttype=\"%d\"\n"
			"\t\t\t\tfile_path=\"%s\"\n"
			"\t\t\t\to_width=\"%d\"\n"
			"\t\t\t\to_height=\"%d\"\n",
			oid++, gW - offset, gH - offset,
			o.x, o.y,
			title, gArgs.xml_type, gMeta.Path( t2i ),
			gW - offset, gH - offset );

			if( gArgs.xml_min < gArgs.xml_max ) {

				fprintf( f,
				"\t\t\t\tmin=\"%d\"\n"
				"\t\t\t\tmax=\"%d\"\n"
				"\t\t\t>\n",
				gArgs.xml_min, gArgs.xml_max );
			}
			else
				fprintf( f, "\t\t\t>\n" );

			fprintf( f,
			"\t\t\t\t<ict_transform"
			" class=\"mpicbg.trakem2.transform.HomographyModel2D\""
			" data=\"%f %f %f %f %f %f %.12g %.12g 1\"/>\n"
			"\t\t\t</t2_patch>\n",
			T.t[0], T.t[1], T.t[2],
			T.t[3], T.t[4], T.t[5],
			T.t[6], T.t[7] );
		}
	}

	fprintf( f, "\t\t</t2_layer>\n" );
}

/* --------------------------------------------------------------- */
/* WriteXMLTail -------------------------------------------------- */
/* --------------------------------------------------------------- */

static FILE* WriteXMLTail( FILE *f )
{
	if( f ) {
		fprintf( f, "\t</t2_layer_set>\n" );
		fprintf( f, "</trakem2>\n" );
		fclose( f );
	}

	return NULL;
}

/* --------------------------------------------------------------- */
/* WriteBillHead ------------------------------------------------- */
/* --------------------------------------------------------------- */

static FILE* WriteBillHead()
{
	FILE	*f = FileOpenOrDie(
			(isAff ? "billfile_Affine.txt" : "billfile_Hmgphy.txt"),
			"w", flog );

	return f;
}

/* --------------------------------------------------------------- */
/* WriteBillLyr_Aff ---------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteBillLyr_Aff( FILE *f )
{
	map<int,int>::iterator	mi, en = R.m.end();

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			TAffine&	T = X_AS_AFF( R.x, j );

			fprintf( f,
			"%d\t%d\t%d"
			"\t%f\t%f\t%f\t%f\t%f\t%f"
			"\t%d\t%d\t%d\t%s\n",
			R.z, id, j - j0 + 1,
			T.t[0], T.t[1], T.t[2], T.t[3], T.t[4], T.t[5],
			t2i->col, t2i->row, t2i->cam, gMeta.Path( t2i ) );
		}
	}
}

/* --------------------------------------------------------------- */
/* WriteBillLyr_Hmy ---------------------------------------------- */
/* --------------------------------------------------------------- */

static void WriteBillLyr_Hmy( FILE *f )
{
	map<int,int>::iterator	mi, en = R.m.end();

	for( mi = R.m.begin(); mi != en; ) {

		const Til2Img	*t2i;

		int	id		= mi->first,
			j0		= mi->second,
			jlim	= (++mi == en ? R.nr : mi->second);

		if( !IDBT2ICacheNGet1( t2i, gArgs.idb, R.z, id, flog ) )
			continue;

		for( int j = j0; j < jlim; ++j ) {

			if( !FLAG_ISUSED( R.flag[j] ) )
				continue;

			THmgphy&	T = X_AS_HMY( R.x, j );

			fprintf( f,
			"%d\t%d\t%d"
			"\t%f\t%f\t%f\t%f\t%f\t%f\t%.12g\t%.12g"
			"\t%d\t%d\t%d\t%s\n",
			R.z, id, j - j0 + 1,
			T.t[0], T.t[1], T.t[2],
			T.t[3], T.t[4], T.t[5],
			T.t[6], T.t[7],
			t2i->col, t2i->row, t2i->cam, gMeta.Path( t2i ) );
		}
	}
}

/* --------------------------------------------------------------- */
/* WriteBillTail ------------------------------------------------- */
/* --------------------------------------------------------------- */

static FILE* WriteBillTail( FILE *f )
{
	if( f )
		fclose( f );

	return NULL;
}

/* --------------------------------------------------------------- */
/* ConvertA ------------------------------------------------------ */
/* --------------------------------------------------------------- */

static void ConvertA()
{
	TAffine	Trot;
	DBox	B;
	FILE	*fout = NULL;
	int		oid;

	if( gArgs.forcew ) {
		B.L	= 0;
		B.B	= 0;
		B.R	= gArgs.forcew;
		B.T	= gArgs.forceh;
	}
	else if( gArgs.degcw || gArgs.type == 'X' ) {
		Trot.NUSetRot( gArgs.degcw * PI/180.0 );
		GetXY_Aff( B, Trot );
	}

	if( gArgs.type == 'T' )
		DskCreateDir( "X_A_TXT", flog );
	else if( gArgs.type == 'M' )
		DskCreateDir( "X_A_MET", flog );
	else if( gArgs.type == 'X' )
		fout = WriteXMLHead( oid, B );
	else if( gArgs.type == 'B' )
		fout = WriteBillHead();

	for( int z = gArgs.zilo; z <= gArgs.zihi; ++z ) {

		if( !R.Init( gArgs.idb, z, flog ) )
			continue;

		if( !R.Load( gArgs.inpath ) )
			continue;

		gMeta.MapZ( z );

		if( gArgs.forcew )
			;
		else if( gArgs.degcw || gArgs.type == 'X' )
			Update_Aff( Trot, B );

		if( gArgs.type == 'T' )
			R.SaveTXT( "X_A_TXT" );
		else if( gArgs.type == 'M' )
			WriteM_Aff();
		else if( gArgs.type == 'X' )
			WriteXMLLyr_Aff( fout, oid );
		else if( gArgs.type == 'B' )
			WriteBillLyr_Aff( fout );
	}

	if( gArgs.type == 'X' )
		fout = WriteXMLTail( fout );
	else if( gArgs.type == 'B' )
		fout = WriteBillTail( fout );
}

/* --------------------------------------------------------------- */
/* ConvertH ------------------------------------------------------ */
/* --------------------------------------------------------------- */

static void ConvertH()
{
	THmgphy	Trot;
	DBox	B;
	FILE	*fout = NULL;
	int		oid;

	if( gArgs.forcew ) {
		B.L	= 0;
		B.B	= 0;
		B.R	= gArgs.forcew;
		B.T	= gArgs.forceh;
	}
	else if( gArgs.degcw || gArgs.type == 'X' ) {
		Trot.NUSetRot( gArgs.degcw * PI/180.0 );
		GetXY_Hmy( B, Trot );
	}

	if( gArgs.type == 'T' )
		DskCreateDir( "X_H_TXT", flog );
	else if( gArgs.type == 'M' )
		DskCreateDir( "X_H_MET", flog );
	else if( gArgs.type == 'X' )
		fout = WriteXMLHead( oid, B );
	else if( gArgs.type == 'B' )
		fout = WriteBillHead();

	for( int z = gArgs.zilo; z <= gArgs.zihi; ++z ) {

		if( !R.Init( gArgs.idb, z, flog ) )
			continue;

		if( !R.Load( gArgs.inpath ) )
			continue;

		gMeta.MapZ( z );

		if( gArgs.forcew )
			;
		else if( gArgs.degcw || gArgs.type == 'X' )
			Update_Hmy( Trot, B );

		if( gArgs.type == 'T' )
			R.SaveTXT( "X_H_TXT" );
		else if( gArgs.type == 'M' )
			WriteM_Hmy();
		else if( gArgs.type == 'X' )
			WriteXMLLyr_Hmy( fout, oid );
		else if( gArgs.type == 'B' )
			WriteBillLyr_Hmy( fout );
	}

	if( gArgs.type == 'X' )
		fout = WriteXMLTail( fout );
	else if( gArgs.type == 'B' )
		fout = WriteBillTail( fout );
}

/* --------------------------------------------------------------- */
/* main ---------------------------------------------------------- */
/* --------------------------------------------------------------- */

int main( int argc, char* argv[] )
{
/* ------------------ */
/* Parse command line */
/* ------------------ */

	gArgs.SetCmdLine( argc, argv );

/* ------- */
/* Convert */
/* ------- */

	clock_t	t0 = StartTiming();

	if( !IDBGetImageDims( gW, gH, gArgs.idb, flog ) )
		exit( 42 );

	gMeta.Init( gArgs.meta );

	if( isAff )
		ConvertA();
	else
		ConvertH();

	StopTiming( flog, "Convert", t0 );

/* ---- */
/* Done */
/* ---- */

	fprintf( flog, "\n" );
	fclose( flog );

	return 0;
}


