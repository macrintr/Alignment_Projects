//
// Get TAffineTable in given z range.
//

#include	"GenDefs.h"
#include	"Cmdline.h"
#include	"CRegexID.h"
#include	"File.h"
#include	"TrakEM2_UTL.h"
#include	"TAffine.h"


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* Types --------------------------------------------------------- */
/* --------------------------------------------------------------- */

/* --------------------------------------------------------------- */
/* CArgs_xml ----------------------------------------------------- */
/* --------------------------------------------------------------- */

class CArgs_xml {

private:
	// re_id used to extract tile id from image name.
	// "/N" used for EM projects, "_N_" for APIG images,
	// "_Nex.mrc" typical for Leginon files.
	CRegexID	re_id;

public:
	char	*infile;
	int		zmin, zmax;
	bool	layers;

public:
	CArgs_xml()
	{
		infile	= NULL;
		zmin	= 0;
		zmax	= 32768;
		layers	= false;
	};

	void SetCmdLine( int argc, char* argv[] );

	int IDFromPatch( TiXmlElement* p );
};

/* --------------------------------------------------------------- */
/* Statics ------------------------------------------------------- */
/* --------------------------------------------------------------- */

static CArgs_xml	gArgs;
static FILE*		flog = NULL;






/* --------------------------------------------------------------- */
/* SetCmdLine ---------------------------------------------------- */
/* --------------------------------------------------------------- */

void CArgs_xml::SetCmdLine( int argc, char* argv[] )
{
// start log

	flog = FileOpenOrDie( "XMLGetTF.log", "w" );

// log start time

	time_t	t0 = time( NULL );
	char	atime[32];

	strcpy( atime, ctime( &t0 ) );
	atime[24] = '\0';	// remove the newline

	fprintf( flog, "Start: %s ", atime );

// parse command line args

	char	*pat;

	re_id.Set( "_N_" );

	if( argc < 4 ) {
		printf( "Usage: XMLGetTF <xml-file1> -zmin=i -zmax=j.\n" );
		exit( 42 );
	}

	for( int i = 1; i < argc; ++i ) {

		// echo to log
		fprintf( flog, "%s ", argv[i] );

		if( argv[i][0] != '-' )
			infile = argv[i];
		else if( GetArgStr( pat, "-p=", argv[i] ) )
			re_id.Set( pat );
		else if( GetArg( &zmin, "-zmin=%d", argv[i] ) )
			;
		else if( GetArg( &zmax, "-zmax=%d", argv[i] ) )
			;
		else if( IsArg( "-layers", argv[i] ) )
			layers = true;
		else {
			printf( "Did not understand option [%s].\n", argv[i] );
			exit( 42 );
		}
	}

	fprintf( flog, "\n" );

	re_id.Compile( flog );

	fflush( flog );
}

/* -------------------------------------------------------------- */
/* IDFromPatch -------------------------------------------------- */
/* -------------------------------------------------------------- */

int CArgs_xml::IDFromPatch( TiXmlElement* p )
{
	const char	*name = p->Attribute( "title" );
	int			id;

	if( !re_id.Decode( id, name ) ) {
		fprintf( flog, "No tile-id found in '%s'.\n", name );
		exit( 42 );
	}

	return id;
}

/* --------------------------------------------------------------- */
/* GetSortedTAffines --------------------------------------------- */
/* --------------------------------------------------------------- */

class TS {
// Use for sorting TForms
public:
	TAffine	T;
	int		id;
public:
	TS( int _i, TAffine &_T )	{id=_i; T=_T;};

	bool operator < (const TS &rhs) const
		{return id < rhs.id;};
};


static void GetSortedTAffines(
	vector<TS>		&ts,
	TiXmlElement*	layer )
{
	TiXmlElement*	p = layer->FirstChildElement( "t2_patch" );

	for( ; p; p = p->NextSiblingElement() ) {

		TAffine	T;
		int		id = gArgs.IDFromPatch( p );

		T.ScanTrackEM2( p->Attribute( "transform" ) );
		ts.push_back( TS( id, T ) );
	}

	sort( ts.begin(), ts.end() );
}

/* --------------------------------------------------------------- */
/* GetWholeLayerTF ----------------------------------------------- */
/* --------------------------------------------------------------- */

static void GetWholeLayerTF()
{
/* ---- */
/* Open */
/* ---- */

	XML_TKEM		xml( gArgs.infile, flog );
	TiXmlElement*	layer	= xml.GetFirstLayer();

/* --- */
/* Get */
/* --- */

	char		buf[256];
	const char	*st = FileNamePtr( gArgs.infile );

	sprintf( buf, "%.*s_LayerTF.txt", strlen( st ) - 4, st );

	FILE	*f = FileOpenOrDie( buf, "w", flog );

	fprintf( f, "Z\tA\tdA\td0\tT0\tT1\tX\tT3\tT4\tY\n" );

	double	zave0 = 999, zaveprev = 0;

	for( ; layer; layer = layer->NextSiblingElement() ) {

		int	z = atoi( layer->Attribute( "z" ) );

		if( z > gArgs.zmax )
			break;

		if( z < gArgs.zmin )
			continue;

		vector<TS>	ts;
		double		t[6] = {0,0,0,0,0,0}, zave;

		GetSortedTAffines( ts, layer );

		int	nt = ts.size();

		for( int i = 0; i < nt; ++i ) {

			const TAffine &T = ts[i].T;

			t[0] += T.t[0];
			t[1] += T.t[1];
			t[2] += T.t[2];
			t[3] += T.t[3];
			t[4] += T.t[4];
			t[5] += T.t[5];
		}

		t[0] /= nt;
		t[1] /= nt;
		t[2] /= nt;
		t[3] /= nt;
		t[4] /= nt;
		t[5] /= nt;

		zave = TAffine( t ).GetRadians() * 180/PI;

		if( zave0 == 999 )
			zave0 = zave;

		fprintf( f, "%d\t%g\t%g\t%g"
		"\t%g\t%g\t%g\t%g\t%g\t%g\n",
		z, zave, zave - zaveprev, zave - zave0,
		t[0], t[1], t[2], t[3], t[4], t[5] );

		zaveprev = zave;
	}

	fclose( f );
}

/* --------------------------------------------------------------- */
/* PrintTAffines ------------------------------------------------- */
/* --------------------------------------------------------------- */

static void PrintTAffines( FILE *f, const vector<TS> &ts, int z )
{
	int	nt = ts.size();

	for( int i = 0; i < nt; ++i ) {

		const double *t = ts[i].T.t;

		fprintf( f, "%d\t%d\t1\t%f\t%f\t%f\t%f\t%f\t%f\n",
		z, ts[i].id, t[0], t[1], t[2], t[3], t[4], t[5] );
	}
}

/* --------------------------------------------------------------- */
/* GetEachTileTF ------------------------------------------------- */
/* --------------------------------------------------------------- */

static void GetEachTileTF()
{
/* ---- */
/* Open */
/* ---- */

	XML_TKEM		xml( gArgs.infile, flog );
	TiXmlElement*	layer	= xml.GetFirstLayer();

/* --- */
/* Get */
/* --- */

	char		buf[256];
	const char	*st = FileNamePtr( gArgs.infile );

	sprintf( buf, "%.*s_TF.txt", strlen( st ) - 4, st );

	FILE	*f = FileOpenOrDie( buf, "w", flog );

	for( ; layer; layer = layer->NextSiblingElement() ) {

		int	z = atoi( layer->Attribute( "z" ) );

		if( z > gArgs.zmax )
			break;

		if( z < gArgs.zmin )
			continue;

		vector<TS>	ts;

		GetSortedTAffines( ts, layer );
		PrintTAffines( f, ts, z );
	}

	fclose( f );
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
/* Process */
/* ------- */

	if( gArgs.layers )
		GetWholeLayerTF();
	else
		GetEachTileTF();

/* ---- */
/* Done */
/* ---- */

exit:
	fprintf( flog, "\n" );
	fclose( flog );

	return 0;
}


