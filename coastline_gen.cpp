/* ========================================================================
    File: @(#)coastline_gen.cpp
   ------------------------------------------------------------------------
    coastline_gen simple coastline generalization
    Copyright (C) 2012-2013 Christoph Hormann <chris_hormann@gmx.de>
   ------------------------------------------------------------------------

    This file is part of coastline_gen

    coastline_gen is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    coastline_gen is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with coastline_gen.  If not, see <http://www.gnu.org/licenses/>.

    Version history:

      0.3: initial public version, February 2013
      0.4: internal, unpublished
      0.5: adding connection and collapse mask support, August 2013

   ========================================================================
 */

const char PROGRAM_TITLE[] = "coastline_gen version 0.5";

#include <cstdlib>
#include <algorithm>
#include <stack>

#define cimg_plugin "CImg_skeleton.h"

#define cimg_use_tiff 1
#define cimg_use_png 1
#define cimg_display 0

#include "skeleton.h"
#include "CImg.h"

using namespace cimg_library;

int main(int argc,char **argv)
{
	std::fprintf(stderr,"%s\n", PROGRAM_TITLE);
	std::fprintf(stderr,"-------------------------------------------------------\n");
	std::fprintf(stderr,"Copyright (C) 2012-2013 Christoph Hormann\n");
	std::fprintf(stderr,"This program comes with ABSOLUTELY NO WARRANTY;\n");
	std::fprintf(stderr,"This is free software, and you are welcome to redistribute\n");
	std::fprintf(stderr,"it under certain conditions; see COPYING for details.\n");

	cimg_usage("Usage: coastline_gen [options]");

	// --- Read command line parameters ---

	// Files
	const char *file_o = cimg_option("-o",(char*)NULL,"output mask file");
	const char *file_i = cimg_option("-i",(char*)NULL,"input mask file");

	const char *file_f = cimg_option("-f",(char*)NULL,"fixed mask file");
	const char *file_c = cimg_option("-c",(char*)NULL,"collapse mask file");

	const float Level = cimg_option("-l",0.5,"threshold level");
	const float SLevel = cimg_option("-ls",0.5,"small feature threshold level");
	const float ILevel = cimg_option("-il",0.06,"island threshold level");

	const int FS = cimg_option("-sf",1,"fixed mask sign (1=repel, -1=attract)");
	const int FR = cimg_option("-rf",2,"fixed mask buffer radius");
	const bool NGConnected = cimg_option("-ngc",false,"do not generalize connected pixels");
	const int FConRad = cimg_option("-fgr",0,"fixed gap connection radius");
	const bool XCon = cimg_option("-xc",false,"extend connections");

	float Radius[8];
	const char *rad_string = cimg_option("-r","4.0:2.5:1.0:0.5:1.0:0.0:0.0","generalization radius (normal:feature:min_water:min_land:island:collapse:collapse_mask:collapse_mask2)");
	int rc = std::sscanf(rad_string,"%f:%f:%f:%f:%f:%f:%f:%f",&Radius[0],&Radius[1],&Radius[2],&Radius[3],&Radius[4],&Radius[5],&Radius[6],&Radius[7]);

	if (rc < 8) Radius[7] = 0.0;
	if (rc < 7) Radius[6] = 0.0;
	if (rc < 6) Radius[5] = 0.0;

	if (Radius[6] <= 0.0) Radius[7] = 0.0;

	int IThr[4];
	const char *is_string = cimg_option("-is","8:16:36:120","island size thresholds (skip:connect:expand:max)");
	std::sscanf(is_string,"%d:%d:%d:%d",&IThr[0],&IThr[1],&IThr[2],&IThr[3]);

	const bool Debug = cimg_option("-debug",false,"generate debug output");

	const bool helpflag = cimg_option("-h",false,"Display this help");
	if (helpflag) std::exit(0);

	if ((file_i == NULL) || (file_o == NULL))
	{
		std::fprintf(stderr,"You must specify input and output mask images files (try '%s -h').\n\n",argv[0]);
		std::exit(1);
	}

	CImg<unsigned char> img_m;
	CImg<unsigned char> img_co;
	CImg<unsigned char> img_l;
	CImg<unsigned char> img_sl;
	CImg<unsigned char> img_sw;
	CImg<unsigned char> img_b;
	CImg<unsigned char> img_d;
	CImg<unsigned char> img_f;
	CImg<int> img_c;

	std::fprintf(stderr,"Loading mask data...\n");
	img_m = CImg<unsigned char>(file_i);

	if (file_c != NULL)
	{
		std::fprintf(stderr,"Loading collapse mask data...\n");
		img_co = CImg<unsigned char>(file_c);
	}

	if (file_f != NULL)
	{
		std::fprintf(stderr,"Loading fixed mask data...\n");
		img_f = CImg<unsigned char>(file_f);

		if ((img_f.width() != img_m.width()) || (img_f.height() != img_m.height()))
		{
			std::fprintf(stderr,"input (-i) and fixed mask (-f) images need to be the same size.\n\n");
			std::exit(1);
		}

		std::fprintf(stderr,"Preprocessing fixed mask data...\n");

		// repel
		if (FS > 0)
		{
			CImg<unsigned char> morph_mask(FR*2 + 1, FR*2 + 1, 1, 1);

			cimg_forXY(morph_mask,px,py)
			{
				int dx = px-(morph_mask.width()/2);
				int dy = py-(morph_mask.height()/2);
				if (std::sqrt(dx*dx+dy*dy) <= FR)
					morph_mask(px,py) = 255;
				else
					morph_mask(px,py) = 0;
			}

			int cnte = 0;
			int cnte2 = 0;
			int cnte3 = 0;
			int cnte4 = 0;

			CImg<unsigned char> img_e = CImg<unsigned char>(img_m.width(), img_m.height(), 1, 1);

			cimg_forXY(img_e,px,py)
				img_e(px,py) = 0;

			// expand connected areas
			if (NGConnected && (FConRad == 0))
			{
				cimg_forXY(img_m,px,py)
				{
					if (img_m(px,py) == 255)
					{
						img_e(px,py) = 255;
						for (int i = 1; i < 9; i++)
						{
							int xn = px + xo[i];
							int yn = py + yo[i];
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_m.width())
										if (yn < img_m.height())
											if (img_f(xn,yn) == 0)
												if (img_m(xn,yn) != 255)
												{
													img_e(xn,yn) = 128;
													cnte++;
												}
						}
					}
				}
			}

			// fix connections to fixed areas
			if (FConRad > 0)
			{
				cimg_forXY(img_m,px,py)
				{
					if (img_m(px,py) == 255)
					if (img_f(px,py) == 0)
					if (img_e(px,py) == 0)
					{
						for (int i = 1; i < 9; i++)
						{
							int xn = px + xo[i];
							int yn = py + yo[i];
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_m.width())
										if (yn < img_m.height())
											if (img_f(xn,yn) != 0)
												if (img_m(xn,yn) != 0)
												if (img_m(xn,yn) != 255)
												{
													img_e(xn,yn) = 128;
													img_m(xn,yn) = 255;
													cnte4++;
												}
						}
					}
				}

				cimg_forXY(img_m,px,py)
				{
					if (img_e(px,py) == 128)
					{
						for (int i = 1; i < 9; i++)
						{
							int xn = px + xo[i];
							int yn = py + yo[i];
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_m.width())
										if (yn < img_m.height())
											if (img_f(xn,yn) != 0)
												if (img_m(xn,yn) != 0)
												if (img_m(xn,yn) != 255)
												{
													img_e(xn,yn) = 80;
													img_m(xn,yn) = 255;
													cnte4++;
												}
						}
					}
				}

				// look for nearest fixed within radius
				for (int d=1; d <= FConRad; d++)
				{
					bool Found = false;
					cimg_forXY(img_m,px,py)
					{
						bool Found = false;
						if (img_m(px,py) == 255)
						if (img_f(px,py) != 0)
						{
							for (int yn=py-d; yn <=py+d; yn++)
								for (int xn=px-d; xn <=px+d; xn++)
									if (!Found)
										if (xn >= 0)
											if (yn >= 0)
												if (xn < img_m.width())
													if (yn < img_m.height())
														if ((std::abs(py-yn) <= d) || (std::abs(px-xn) <= d))
															if (std::sqrt((px-xn)*(px-xn) + (py-yn)*(py-yn)) <= d)
																if (img_f(xn,yn) == 0)
																{
																	unsigned char v = 180;
																	img_e.draw_line(px, py, xn, yn, &v);
																	v = 254;
																	img_m.draw_line(px, py, xn, yn, &v);
																	cnte2++;
																	Found = true;
																	break;
																}
						}
					}
				}

				cimg_forXY(img_m,px,py)
				{
					if ((img_m(px,py) == 255) || (img_e(px,py) > 64))
					{
						for (int i = 1; i < 9; i++)
						{
							int xn = px + xo[i];
							int yn = py + yo[i];
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_m.width())
										if (yn < img_m.height())
											if (img_f(xn,yn) == 0)
												if (img_m(xn,yn) != 255)
												{
													img_e(xn,yn) = 64;
													img_m(xn,yn) = 254;
													cnte3++;
												}
						}
					}
				}

				if (XCon)
					cimg_forXY(img_m,px,py)
					{
						if (img_e(px,py) == 64)
						{
							for (int i = 1; i < 9; i++)
							{
								int xn = px + xo[i];
								int yn = py + yo[i];
								if (xn >= 0)
									if (yn >= 0)
										if (xn < img_m.width())
											if (yn < img_m.height())
												if (img_f(xn,yn) == 0)
													if (img_m(xn,yn) != 255)
													{
														img_e(xn,yn) = 32;
														img_m(xn,yn) = 254;
														cnte2++;
													}
							}
						}
					}

				cimg_forXY(img_m,px,py)
				{
					if (img_m(px,py) == 254)
						img_m(px,py) = 255;
				}
			}

			if (Debug)
				img_e.save("debug-em.tif");

			std::fprintf(stderr,"  %d/%d/%d/%d pixels expanded\n", cnte, cnte2, cnte3, cnte4);

			img_b = img_f.get_erode(morph_mask);

			cimg_forXY(img_f,px,py)
			{
				if (img_f(px,py) > 0)
				{
					if (img_b(px,py) == 0)
					{
						img_f(px,py) = 128;
						if (img_m(px,py) < 255)
							img_m(px,py) = 0;
					}
					else
						img_f(px,py) = 255;
				}

				if (img_m(px,py) > 0)
				{
					if (img_m(px,py) < 255)
					{
						if (img_f(px,py) < 255)
							img_f(px,py) = 64;
						img_m(px,py) = 255;
					}
					else
					{
						img_f(px,py) = 255;
					}
				}
				img_b(px,py) = img_m(px,py);
			}

			if (NGConnected)
			{
				cimg_forXY(img_m,px,py)
				{
					if (img_e(px,py) != 0)
					{
						img_f(px,py) = 254;
					}
				}
			}
		}
		else // attract
		{
			cimg_forXY(img_m,px,py)
			{
				if (img_m(px,py) > 0)
					img_m(px,py) = 255;
			}

			img_b = img_m;

			cimg_forXY(img_f,px,py)
			{
				if ((img_b(px,py) == 0) && (img_f(px,py) != 0))
				{
					bool found_m = false;
					bool found_f = false;
					for (int yn=py-FR; yn <=py+FR; yn++)
					{
						for (int xn=px-FR; xn <=px+FR; xn++)
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_f.width())
										if (yn < img_f.height())
											if (std::sqrt((px-xn)*(px-xn) + (py-yn)*(py-yn)) <= FR)
											{
												if (img_f(xn,yn) == 0) found_f = true;
												if (img_b(xn,yn) != 0) found_m = true;
												if (found_m && found_f) break;
											}
						if (found_m && found_f) break;
					}

					if (found_m && found_f)
					{
						img_m(px,py) = 255;
						for (int i = 1; i < 9; i++)
						{
							int xn = px + xo[i];
							int yn = py + yo[i];
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_f.width())
										if (yn < img_f.height())
											img_m(xn,yn) = 255;
						}
					}
				}
			}

			img_b = img_m;
		}

		if (Debug)
		{
			img_f.save("debug-f.pgm");
			img_m.save("debug-fm.pgm");
		}
	}
	else
	{
		cimg_forXY(img_m,px,py)
		{
			if (img_m(px,py) > 0)
				img_m(px,py) = 255;
		}
		img_b = img_m;
	}

	img_sw = CImg<unsigned char>(img_m.width(), img_m.height(), 1, 1);
	img_d = CImg<unsigned char>(img_m.width(), img_m.height(), 1, 1);
	img_c = CImg<int>(img_m.width(), img_m.height(), 1, 1);

	std::fprintf(stderr,"Measuring land areas...\n");

	size_t cnt_all = 0;
	size_t cnt_land = 0;

	cimg_forXY(img_b,px,py)
	{
		cnt_all++;
		if (img_b(px,py) == 255)
		{
			img_c(px,py) = 1;
			img_d(px,py) = 48;
			cnt_land++;
		}
		else
		{
			img_c(px,py) = 0;
			img_d(px,py) = 0;
		}
	}

	if ((cnt_land == cnt_all) || (cnt_land == 0))
	{
		std::fprintf(stderr,"  data is trivial\n");

		if (file_o != NULL)
		{
			std::fprintf(stderr,"Writing output...\n");
			img_m.save(file_o);
			std::fprintf(stderr,"coastline mask written to file %s\n", file_o);
		}
		return 0;
	}

	if (Debug)
		img_d.save("debug-dx.pgm");

	int cntie = 0;
	int cntie2 = 0;

	std::fprintf(stderr,"Measuring islands...\n");

	// measure islands and mark if too small
	cimg_forXY(img_b,px,py)
	{
		if (img_b(px,py) == 255)
		{
			int c = img_b.floodfill4(px, py, 255, 128);
			if (c < IThr[0])
			{
				img_b.floodfill4(px, py, 128, 64);
				img_m.floodfill4(px, py, 255, 0);
				img_c.floodfill4(px, py, 1, 0);
				cntie++;
			}
			else if (c < IThr[1])
			{
				img_b.floodfill4(px, py, 128, 160);
				img_m.floodfill4(px, py, 255, 0);
				cntie2++;
			}
			else
				img_c.floodfill4(px, py, 1, c);
		}
	}

	if (Debug)
		img_b.save("debug-ib.pgm");

	std::fprintf(stderr,"  found %d/%d small islands.\n", cntie, cntie2);

	if ((Radius[5] > 0.1) || (Radius[6] > 0.1))
	{
		std::fprintf(stderr,"Collapsing thin features (%.2f/%.2f/%.2f)...\n", Radius[5], Radius[6], Radius[7]);

		int RI = int(Radius[6]+0.5);
		CImg<unsigned char> morph_mask(RI*2 + 1, RI*2 + 1, 1, 1);

		cimg_forXY(morph_mask,px,py)
		{
			int dx = px-(morph_mask.width()/2);
			int dy = py-(morph_mask.height()/2);
			if (std::sqrt(dx*dx+dy*dy) <= Radius[6])
				morph_mask(px,py) = 255;
			else
				morph_mask(px,py) = 0;
		}

		CImg<float> img_dist = img_b.get_distance(0);

		CImg<unsigned char> img_e = img_b.get_erode(morph_mask);
		CImg<unsigned char> img_e2 = img_b;
		CImg<unsigned char> img_ex = img_b;

		cimg_forXY(img_b,px,py)
		{
			if (img_e(px,py) > 0) img_e(px,py) = 255;
			if (img_e2(px,py) > 0) img_e2(px,py) = 255;
			if (img_ex(px,py) > 0) img_ex(px,py) = 255;
		}

		cimg_forXY(img_b,px,py)
		{
			if (img_e2(px,py) == 255)
				if (img_dist(px,py) < Radius[7])
					img_e2(px,py) = 180;
		}

		cimg_forXY(img_b,px,py)
		{
			if (img_e2(px,py) == 255)
				if (img_dist(px,py) >= Radius[7])
					img_e2.floodfill4(px, py, 255, 128);
		}

		// disable collapse according to collapse mask
		if (file_c != NULL)
		{
			img_co.dilate(morph_mask);
		}

		if (Debug)
			img_e.save("debug-cl-e.tif");
		if (Debug)
			img_e2.save("debug-cl-e2.tif");

		img_dist = img_e.get_distance(255);

		if (Radius[5] > 0.1)
		{
			CImg<float> img_dist2 = img_b.get_distance(0);

			cimg_forXY(img_b,px,py)
			{
				if (img_ex(px,py) == 255)
					if (img_dist2(px,py) >= Radius[5])
						img_ex.floodfill4(px, py, 255, 128);
			}
		}

		int cntc = 0;
		int cntc2 = 0;
		int cntc3 = 0;

		// collapse thin features
		cimg_forXY(img_m,px,py)
		{
			if (img_b(px,py) == 0) continue;

			if (Radius[5] > 0.1)
			{
				if (img_ex(px,py) != 128)
				{
					img_b(px,py) = 0;
					img_m(px,py) = 0;
					img_c(px,py) = 0;
					img_d(px,py) = 64;
					cntc++;
				}
			}
			if (img_dist(px,py) > Radius[6]*8.0)
			{
				if (file_c != NULL)
				{
					if (img_co(px,py) != 0)
					{
						img_b(px,py) = 0;
						img_m(px,py) = 0;
						img_c(px,py) = 0;
						img_d(px,py) = 128;
						cntc2++;
					}
				}
				else
				{
					img_b(px,py) = 0;
					img_m(px,py) = 0;
					img_c(px,py) = 0;
					img_d(px,py) = 128;
					cntc2++;
				}
			}
			if (img_e2(px,py) != 128)
			//if ((img_e2(px,py) != 180) && (img_dist(px,py) > Radius[6]))
			{
				if (file_c != NULL)
				{
					//if (img_c(px,py) >= IThr[3])
					if (img_co(px,py) != 0)
					{
						img_b(px,py) = 0;
						img_m(px,py) = 0;
						img_c(px,py) = 0;
						img_d(px,py) = 128;
						cntc3++;
					}
				}
				else //if (img_c(px,py) >= IThr[3])
				{
					img_b(px,py) = 0;
					img_m(px,py) = 0;
					img_c(px,py) = 0;
					img_d(px,py) = 128;
					cntc3++;
				}
			}
		}

		if (Debug)
			img_d.save("debug-dcl.tif");

		std::fprintf(stderr,"  %d/%d pixels collapsed.\n", cntc, cntc2, cntc3);
	}

	std::fprintf(stderr,"Analyzing small islands...\n");

	cntie = 0;
	int cxx = 0;

	// look for nearest main land
	for (int d=1; d < Radius[1]-0.0001; d++)
	{
		cimg_forXY(img_b,px,py)
		{
			bool Found = false;
			if (img_c(px,py) == 1)
			if (img_b(px,py) == 160)
			{
				for (int yn=py-d; yn <=py+d; yn++)
					for (int xn=px-d; xn <=px+d; xn++)
						if (!Found)
							if (xn >= 0)
								if (yn >= 0)
									if (xn < img_c.width())
										if (yn < img_c.height())
											if ((std::abs(py-yn) == d) || (std::abs(px-xn) == d))
												if (std::sqrt((px-xn)*(px-xn) + (py-yn)*(py-yn)) <= Radius[1])
													if (img_m(xn,yn) == 255)
													{
														img_b.floodfill4(px, py, 160, 180);
														img_c.floodfill4(px, py, 1, 0);
														const unsigned char v = 255;
														img_b.draw_line(px, py, xn, yn, &v);
														img_b.draw_line(px+1, py, xn+1, yn, &v);
														img_b.draw_line(px-1, py, xn-1, yn, &v);
														cntie++;
														Found = true;
														break;
													}
													else
													{
														cxx++;
														if (img_b(xn,yn) == 0) img_b(xn,yn) = 32;
													}
			}
			else
			{
				img_b(px,py) = std::min(96, (int)img_b(px,py));
			}
		}
	}

	// transfer to main image
	cimg_forXY(img_b,px,py)
	{
		if (img_b(px,py) > 128)
		{
			img_m(px,py) = 255;
			img_d(px,py) = 255;
		}
	}

	if (Debug)
		img_b.save("debug-ib2.pgm");

	std::fprintf(stderr,"  connected %d small islands (%d tests).\n", cntie, cxx);

	std::fprintf(stderr,"Preparing skeletonization...\n");

	img_sl = CImg<unsigned char>(img_m);

	cimg_forXY(img_sl,px,py)
	{
		img_sw(px,py) = 255-img_sl(px,py);
	}

	img_sl.erode(2*Radius[0]);
	img_sw.erode(2*Radius[0]);

	if ((file_f != NULL) && (FS > 0))
	{
		cimg_forXY(img_f,px,py)
		{
			if (img_f(px,py) < 254)
			{
				img_sl(px,py) = 0;
				img_sw(px,py) = 255;
			}
		}
	}

	if (Debug)
	{
		img_sl.save("debug-raw-l.pgm");
		img_sw.save("debug-raw-w.pgm");
	}

	int csl1 = 0;
	int csw1 = 0;
	int csl2 = 0;
	int csw2 = 0;

	cimg_forXY(img_sl,px,py)
	{
		if ((px > 1) && (py > 1) && (px < img_sl.width()-2) && (py < img_sl.height()-2))
		{
			if (img_sl(px,py) > 0)
			{
				img_sl(px,py) = 255;
				csl1++;
			}
			else if (img_m(px,py) > 0)
			{
				img_sl(px,py) = 128;
				csl2++;
			}
			else
				img_sl(px,py) = 0;
		}
		else
			img_sl(px,py) = 0;

		if ((px > 1) && (py > 1) && (px < img_sw.width()-2) && (py < img_sw.height()-2))
		{
			if (img_sw(px,py) > 0)
			{
				img_sw(px,py) = 255;
				csw1++;
			}
			else if (img_m(px,py) == 0)
			{
				img_sw(px,py) = 128;
				csw2++;
			}
			else
				img_sw(px,py) = 0;
		}
		else
			img_sw(px,py) = 0;
	}

	std::fprintf(stderr,"  land: %d fixed, %d variable.\n", csl1, csl2);
	std::fprintf(stderr,"  water: %d fixed, %d variable.\n", csw1, csw2);

	std::fprintf(stderr,"Skeletonizing...\n");

	img_sl.thin(200, true);
	img_sw.thin(200, true);

	if (Debug)
	{
		img_sl.save("debug-skel-l.pgm");
		img_sw.save("debug-skel-w.pgm");
	}

	std::fprintf(stderr,"Processing skeletons...\n");

	{
		CImg<unsigned char> img_snl = img_sl;
		CImg<unsigned char> img_snw = img_sw;

		// mark all junctions
		cimg_forXY(img_snl,px,py)
		{
			if (img_snl(px,py) == 128)
			{
				img_d(px,py) = 128;

				if (img_snl.n_adj(px,py) < 2)
					img_sl(px,py) = 0;
			}
			else
				img_sl(px,py) = 0;

			if (img_snw(px,py) == 128)
			{
				img_d(px,py) = 80;

				if (img_snw.n_adj(px,py) < 2)
					img_sw(px,py) = 0;
			}
			else
				img_sw(px,py) = 0;
		}
	}

	std::fprintf(stderr,"Doing basic smoothing...\n");

	img_b = img_m;
	img_b.blur(Radius[0]);

	if ((file_f != NULL) && (FS > 0))
	{
		cimg_forXY(img_f,px,py)
		{
			if (img_f(px,py) < 254)
			{
				img_b(px,py) = 0;
			}
		}
	}

	CImg<unsigned char> img_sl2 = img_sl;
	CImg<unsigned char> img_sw2 = img_sw;
	CImg<unsigned char> img_swx = img_sw;
	CImg<unsigned char> img_slx = img_sl;

	std::fprintf(stderr,"Shortening primary skeletons...\n");

	for (int j=0; j < Radius[1]*1.6; j++)
	{
		CImg<unsigned char> img_tmpl(img_sl);
		CImg<unsigned char> img_tmpw(img_sw);
		CImg<unsigned char> img_tmpl2(img_sl2);
		CImg<unsigned char> img_tmplx(img_slx);
		CImg<unsigned char> img_tmpw2(img_sw2);
		cimg_forXY(img_sl,px,py)
		{
			if ((px > 2) && (py > 2) && (px < img_sl.width()-3) && (py < img_sl.height()-3))
			{
					if (img_tmplx(px,py) > 0)
						if (img_tmplx.is_end3(px, py))
						{
							img_slx(px,py) = 0;
						}

				if (j < Radius[1]*1.2)
				{
					if (img_tmpl(px,py) > 0)
						if (img_tmpl.is_end3(px, py))
						{
							img_sl(px,py) = 0;
							img_d(px,py) = 200;
						}
					if (img_tmpw(px,py) > 0)
						if (img_tmpw.is_end3(px, py))
						{
							img_sw(px,py) = 0;
							img_d(px,py) = 200;
						}
				}
				if (j < Radius[1]*0.5)
				{
					if (img_tmpw2(px,py) > 0)
						if (img_tmpw2.is_end3(px, py))
						{
							img_sw2(px,py) = 0;
							img_d(px,py) = 255;
						}
				}
			}
		}
	}

	if (Radius[2] == 0)
	{
		cimg_forXY(img_swx,px,py)
		{
			img_swx(px,py) = 0;
		}
	}
	else
	{
		std::fprintf(stderr,"Generating water base skeleton...\n");

		int j=0;

		while (true)
		{
			std::fprintf(stderr,"  iteration %d...\n", j);
			bool Found = false;
			CImg<unsigned char> img_tmpw(img_swx);
			cimg_forXY(img_swx,px,py)
			{
				if ((px > 2) && (py > 2) && (px < img_swx.width()-3) && (py < img_swx.height()-3))
				{
					if (img_tmpw(px,py) > 0)
						if (img_tmpw.is_end3(px, py))
						{
							img_swx(px,py) = 0;
							Found = true;
						}
				}
			}
			if (!Found) break;
			j++;
			if (j > Radius[2]*20) break;
		}
	}


	if (Debug)
	{
		img_sl.save("debug-skel2-l.pgm");
		img_sl2.save("debug-skel2-l2.pgm");
		img_slx.save("debug-skel2-lx.pgm");
		img_sw.save("debug-skel2-w.pgm");
		img_sw2.save("debug-skel2-w2.pgm");
		img_swx.save("debug-skel2-wx.pgm");
	}

	std::fprintf(stderr,"Dilating skeleton...\n");

	float rsum = 0.0;

	while (rsum < Radius[1]+0.01)
	{
		cimg_forXY(img_c,px,py)
		{
			if (img_c(px,py) > 1)
				if (img_c(px,py) < rsum*rsum*4.0)
				if (img_c(px,py) < IThr[2])
				{
					img_sl(px,py) = 255;
					img_sl2(px,py) = 255;
				}
		}

		float r = std::min(float(1.0),Radius[1]-rsum);
		if (r < 0.01) break;
		std::fprintf(stderr,"  step 1 (%.2f)...\n", r);
		img_sl.blur(r);
		img_sw.blur(r);
		if (rsum < Radius[1]*0.5)
			img_sl2.blur(r);
		if (rsum < Radius[1]*0.5)
			img_sw2.blur(r);
		if (rsum < Radius[2])
			img_swx.blur(r);
		if (rsum < Radius[3])
			img_slx.blur(r);

		cimg_forXY(img_sl,px,py)
		{
			if ((rsum < Radius[2]) && (img_swx(px,py) > 10))
				img_swx(px,py)	= 255;
			if ((rsum < Radius[3]) && (img_slx(px,py) > 10))
				img_slx(px,py)	= 255;

			{
				if ((img_sl(px,py) > 10) && (img_sl(px,py) > img_sw(px,py)))
					img_sl(px,py)	= 255;
				else
					img_sl(px,py)	= 0;

				if (rsum < Radius[1]*0.5)
					if ((img_sl2(px,py) > 10) && (img_sl2(px,py) > img_sw(px,py)) && (img_sl2(px,py) > img_sw2(px,py)))
						img_sl2(px,py)	= 255;
					else
						img_sl2(px,py)	= 0;

				if ((img_sw(px,py) > 10) && (img_sw(px,py) > img_sl(px,py)) && (img_sw(px,py) > img_sl2(px,py)))
					img_sw(px,py)	= 255;
				else
					img_sw(px,py)	= 0;

				if (rsum < Radius[1]*0.5)
					if ((img_sw2(px,py) > 10) && (img_sw2(px,py) > img_sl(px,py)) && (img_sw2(px,py) > img_sl2(px,py)))
						img_sw2(px,py)	= 255;
					else
						img_sw2(px,py)	= 0;
			}
		}

		rsum += r;
	}

	if (Debug)
	{
		img_sl.save("debug-skel3-l.pgm");
		img_sl2.save("debug-skel3-l2.pgm");
		img_slx.save("debug-skel3-lx.pgm");
		img_sw.save("debug-skel3-w.pgm");
		img_sw2.save("debug-skel3-w2.pgm");
		img_swx.save("debug-skel3-wx.pgm");
	}

	{
		CImg<int> img_tmpc = img_c.get_dilate(3);

		cimg_forXY(img_m,px,py)
		{
			if ((img_b(px,py) < (((img_tmpc(px,py)<IThr[3])&&(img_tmpc(px,py)>1))?SLevel*255:Level*255)) && (img_sl(px,py) == 0) && (img_sl2(px,py) == 0))
				img_m(px,py) = 0;
			else if (img_slx(px,py) != 0)
				img_m(px,py) = 255;
			else if ((img_sw(px,py) == 0) && (img_sw2(px,py) == 0) && (img_swx(px,py) == 0))
				img_m(px,py) = 255;
			else
				img_m(px,py) = 0;
		}
	}

	if (Debug)
		img_m.save("debug-m.pgm");

	std::fprintf(stderr,"Postprocessing Islands...\n");

	cimg_forXY(img_c,px,py)
	{
		img_b(px,py) = 0;
		if ((img_sw(px,py) == 0) && (img_sw2(px,py) == 0) && (img_swx(px,py) == 0))
		if (img_c(px,py) > 1)
		{
			if (img_c(px,py) < IThr[2])
			{
				img_b(px,py) = 255;
				img_d(px,py) = 180;
			}
			else if (img_c(px,py) < IThr[2]*3)
			{
				img_b(px,py) = 64;
				img_d(px,py) = 160;
			}
			else if (img_c(px,py) < IThr[2]*8)
			{
				img_b(px,py) = 32;
				img_d(px,py) = 140;
			}
		}
	}

	img_b.blur(Radius[4]);

	cimg_forXY(img_b,px,py)
	{
		if (img_sw(px,py) == 0)
		{
			if (img_sw2(px,py) == 0)
			{
				if (img_swx(px,py) == 0)
				{
					if (img_b(px,py) >= ILevel*255)
						img_m(px,py) = 255;
				}
				else
				{
					if (img_b(px,py) >= ILevel*2*255)
						img_m(px,py) = 255;
				}
			}
			else
			{
				if (img_b(px,py) >= ILevel*3*255)
					img_m(px,py) = 255;
			}
		}
	}

	if ((file_f != NULL) && (FS > 0))
	{
		cimg_forXY(img_f,px,py)
		{
			if (img_f(px,py) < 255)
			{
				if (img_f(px,py) == 254)
					img_m(px,py) = 255;
				else
					img_m(px,py) = 0;
			}
		}
	}

	if (Debug)
		img_d.save("debug-d.pgm");

	if (file_o != NULL)
	{
		std::fprintf(stderr,"Writing output...\n");
		img_m.save(file_o);
		std::fprintf(stderr,"generalized mask written to file %s\n", file_o);
	}
}
