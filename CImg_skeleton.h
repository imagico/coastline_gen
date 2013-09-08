// CImg plugin with skeletonization (thinning) functions
// Copyright 2012 Christoph Hormann <chris_hormann@gmx.de>
// dual licensed CeCILL v2.0 and GPL v3

struct Point {
  int x;
  int y;
  int square() const { return x*x + y*y; }
  Point(int x = 0, int y = 0): x(x), y(y) {}
  bool operator<(const Point& pt) const {
    if( square() < pt.square() )
      return true;
    if( pt.square() < square() )
      return false;
    if( x < pt.x )
      return true;
    if( pt.x < x)
      return false;
    return y < pt.y;
  }

  bool operator==(const Point& pt) const {
    if (( x == pt.x ) && ( y == pt.y ))
      return true;
    return false;
  }

  bool operator!=(const Point& pt) const {
    if ( x != pt.x )
      return true;
    if ( y != pt.y )
      return true;
    return false;
  }
};

/*
 * C code from the article
 * "Efficient Binary Image Thinning using Neighborhood Maps"
 * by Joseph M. Cychosz, 3ksnn64@ecn.purdue.edu
 * in "Graphics Gems IV", Academic Press, 1994
 */

/* ---- ThinImage - Thin binary image. -------------------------------- */
/*									*/
/*	Description:							*/
/*	    Thins the supplied binary image using Rosenfeld's parallel	*/
/*	    thinning algorithm.						*/
/*									*/
/*	On Entry:							*/
/*	    image = Image to thin.					*/
/*									*/
/* -------------------------------------------------------------------- */

size_t thin(const T threshold, bool Progress = false)
{
	int xsize, ysize; /* Image resolution		*/
	int pc	= 0;      /* Pass count			*/
	size_t count = 1;	/* Deleted pixel count		*/
	size_t tcount = 0;
	int p, q;         /* Neighborhood maps of adjacent cells */
	int m;            /* Deletion direction mask	*/

	xsize = width();
	ysize = height();

	CImg<unsigned char> qb = CImg<unsigned char>(xsize,1,1,1);
	qb(xsize-1) = 0;		/* Used for lower-right pixel	*/

	while ( count ) {		/* Scan image while deletions	*/
		pc++;
		count = 0;
		for (int i=0; i<4; i++) {
			m = masks[i];
			/* Build initial previous scan buffer.			*/
			p = (*this)(0,0) != 0;
			for (int x = 0 ; x < xsize-1 ; x++ )
		    qb(x) = p = ((p<<1)&0006) | ((*this)(x+1,0) != 0);

			/* Scan image for pixel deletion candidates.		*/

			for (int y = 0 ; y < ysize-1 ; y++ ) {
		    q = qb(0);
		    p = ((q<<3)&0110) | ((*this)(0,y+1) != 0);

		    for (int x = 0 ; x < xsize-1 ; x++ ) {
					q = qb(x);
					p = ((p<<1)&0666) | ((q<<3)&0110) | ((*this)(x+1,y+1) != 0);
					qb(x) = p;
					if  ( ((p&m) == 0) && xdelete[p] ) {
						if ((*this)(x,y) < threshold)
						{
							count++;
							(*this)(x,y) = 0;
						}
					}
		    }

		    /* Process right edge pixel.			*/
		    p = (p<<1)&0666;
		    if	( (p&m) == 0 && xdelete[p] ) {
					if ((*this)(xsize-1,y) < threshold)
					{
						count++;
						(*this)(xsize-1,y) = 0;
					}
		    }
			}

			/* Process bottom scan line.				*/
			for (int x = 0 ; x < xsize ; x++ ) {
		    q = qb(x);
		    p = ((p<<1)&0666) | ((q<<3)&0110);
		    if	( (p&m) == 0 && xdelete[p] ) {
					if ((*this)(x,ysize-1) < threshold)
					{
						count++;
						(*this)(x,ysize-1) = 0;
					}
		    }
			}
		}

		if (Progress)
			std::fprintf (stderr, "thin(): pass %d, %d pixels deleted.\n", pc, count);
		tcount += count;
	}
	return tcount;
}

size_t floodfill4(int x, int y, T val, T val_fill)
{
	std::stack<Point> Q;
	int cnt = 0;

	Q.push(Point(x,y));
	while (!Q.empty())
	{
		Point n = Q.top();
		Q.pop();
		if ((*this)(n.x,n.y) == val)
		{
			(*this)(n.x,n.y) = val_fill;
			cnt++;
			for (int i = 0; i < 4; i++)
			{
				int xn = n.x + x4[i];
				int yn = n.y + y4[i];
				if (xn >= 0)
					if (yn >= 0)
						if (xn < width())
							if (yn < height())
								Q.push(Point(xn, yn));
			}
		}
	}
	return cnt;
}

bool is_end3(int x, int y)
{
	int ncnt = 0;
	int xcnt = 0;
	int prev = (*this)(x+xo[8],y+yo[8]);
	for (int i = 1; i < 9; i++)
	{
		int xn = x + xo[i];
		int yn = y + yo[i];
		if (xn >= 0)
			if (yn >= 0)
				if (xn < width())
					if (yn < height())
						if ((*this)(xn,yn) != 0)
						{
							ncnt++;
							if (ncnt > 3) return false;
							prev = 1;
						}
						else
						{
							if (prev != 0) xcnt ++;
							if (xcnt > 1) return false;
							prev = 0;
						}

	}
	return true;
}

int n_adj(int x, int y)
{
	int ncnt = 0;
	for (int i = 1; i < 9; i++)
	{
		int xn = x + xo[i];
		int yn = y + yo[i];
		if (xn >= 0)
			if (yn >= 0)
				if (xn < width())
					if (yn < height())
						if ((*this)(xn,yn) != 0)
						{
							ncnt++;
						}
	}
	return ncnt;
}
