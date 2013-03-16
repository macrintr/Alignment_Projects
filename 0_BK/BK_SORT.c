/* BK_SORT -----------------------------------------------------------
 *
 * Heap sort.
 *
 * Copyright (c) 1999 Bill Karsh.
 * All rights reserved.
 *
 */


#include	"BK_SORT.h"


/* --------------------------------------------------------------- */
/* Macros -------------------------------------------------------- */
/* --------------------------------------------------------------- */

#define	Slot4( k )										\
	(*(SInt32*)((char*)base + (k)))

#define	oneX4											\
	sizeof(SInt32)






/* SORTSInt32 --------------------------------------------------------
 *
 * Standard Heapsort of zero-based array of 4-byte indices.
 *
 * The indices are moved about in their array.
 *
 * - base
 * Base address of index array.
 *
 * - nItems
 * Number of indices in array.
 *
 * - compare
 * Pointer to comparison proc. It is passed the two indices that
 * need to be compared.
 *
 * The proc should return a signed integer as follows...
 *
 * For ASCENDING array:
 *	- negative:		A < B
 *	- non-negative:	otherwise
 *
 * For DESCENDING array:
 *	- negative:		A > B
 *	- non-negative:	otherwise
 *
 * -----
 * Notes
 * -----
 *
 * You can also sort an array of UInt32 as follows:
 *
 * (1) Cast the array base address to (SInt32*).
 *
 * (2) Define your sort proc as: int proc( UInt32 A, UInt32 B ),
 * then cast it to SORTSInt32Proc.
 *
 * Copyright (c) 1999 Bill Karsh.
 * All rights reserved.
 *
 */

void SORTSInt32(
	SInt32				*base,
	SInt32				nItems,
	SORTSInt32Proc		compare )
{
	SInt32	iX4, jX4, kX4, nX4, temp;

	if( nItems < 2 )
		goto exit;

	kX4 = ((nItems >> 1) + 1) * sizeof(SInt32);
	nX4 = nItems * sizeof(SInt32);
	--base;			/* -1 for zero-offset array */

	for(;;) {

		if( kX4 > oneX4 )
			temp = Slot4( kX4 -= oneX4 );
		else {

			temp = Slot4( nX4 );
			Slot4( nX4 ) = Slot4( oneX4 );

			if( (nX4 -= oneX4) == oneX4 ) {
				Slot4( oneX4 ) = temp;
				break;
			}
		}

		iX4 = kX4;
		jX4 = kX4 + kX4;

		while( jX4 <= nX4 ) {

			if( jX4 < nX4 &&
				compare(
					Slot4( jX4 ),
					Slot4( jX4 + oneX4 ) ) < 0 ) {

				jX4 += oneX4;
			}

			if( compare( temp, Slot4( jX4 ) ) < 0 ) {

				Slot4( iX4 ) = Slot4( jX4 );
				iX4  = jX4;
				jX4 += jX4;
			}
			else
				break;
		}

		Slot4( iX4 ) = temp;
	}

exit:
	return;
}


/* SORTImmedAscending ------------------------------------------------
 *
 * Heapsort zero-based array of 4-byte immediate values
 * into ascending order.
 *
 * The values are moved about in their array.
 *
 * - base
 * Base address of array.
 *
 * - nItems
 * Number of values in array.
 *
 * Copyright (c) 1999 Bill Karsh.
 * All rights reserved.
 *
 */

void SORTImmedAscending( SInt32 *base, UInt32 nItems )
{
	SInt32	temp;
	UInt32	iX4, jX4, kX4, nX4;

	if( nItems < 2 )
		goto exit;

	kX4 = ((nItems >> 1) + 1) * sizeof(SInt32);
	nX4 = nItems * sizeof(SInt32);
	--base;			/* -1 for zero-offset array */

	for(;;) {

		if( kX4 > oneX4 )
			temp = Slot4( kX4 -= oneX4 );
		else {

			temp = Slot4( nX4 );
			Slot4( nX4 ) = Slot4( oneX4 );

			if( (nX4 -= oneX4) == oneX4 ) {
				Slot4( oneX4 ) = temp;
				break;
			}
		}

		iX4 = kX4;
		jX4 = kX4 + kX4;

		while( jX4 <= nX4 ) {

			if( jX4 < nX4 &&
				Slot4( jX4 ) < Slot4( jX4 + oneX4 ) ) {

				jX4 += oneX4;
			}

			if( temp < Slot4( jX4 ) ) {

				Slot4( iX4 ) = Slot4( jX4 );
				iX4  = jX4;
				jX4 += jX4;
			}
			else
				break;
		}

		Slot4( iX4 ) = temp;
	}

exit:
	return;
}


/* SORTImmedDescending -----------------------------------------------
 *
 * Heapsort zero-based array of 4-byte immediate values
 * into descending order.
 *
 * The values are moved about in their array.
 *
 * - base
 * Base address of array.
 *
 * - nItems
 * Number of values in array.
 *
 * Copyright (c) 1999 Bill Karsh.
 * All rights reserved.
 *
 */

void SORTImmedDescending( SInt32 *base, UInt32 nItems )
{
	SInt32	temp;
	UInt32	iX4, jX4, kX4, nX4;

	if( nItems < 2 )
		goto exit;

	kX4 = ((nItems >> 1) + 1) * sizeof(SInt32);
	nX4 = nItems * sizeof(SInt32);
	--base;			/* -1 for zero-offset array */

	for(;;) {

		if( kX4 > oneX4 )
			temp = Slot4( kX4 -= oneX4 );
		else {

			temp = Slot4( nX4 );
			Slot4( nX4 ) = Slot4( oneX4 );

			if( (nX4 -= oneX4) == oneX4 ) {
				Slot4( oneX4 ) = temp;
				break;
			}
		}

		iX4 = kX4;
		jX4 = kX4 + kX4;

		while( jX4 <= nX4 ) {

			if( jX4 < nX4 &&
				Slot4( jX4 + oneX4 ) < Slot4( jX4 ) ) {

				jX4 += oneX4;
			}

			if( Slot4( jX4 ) < temp ) {

				Slot4( iX4 ) = Slot4( jX4 );
				iX4  = jX4;
				jX4 += jX4;
			}
			else
				break;
		}

		Slot4( iX4 ) = temp;
	}

exit:
	return;
}


