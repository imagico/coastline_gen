# makefile for coastline_gen

CXX=g++

CXXFLAGS = -O3 -I.

LDFLAGS = -lm -ltiff -lpng

PROGRAMS = coastline_gen

.PHONY: all clean

all: $(PROGRAMS)

coastline_gen.o: coastline_gen.cpp skeleton.h CImg_skeleton.h
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_OGR) -o $@ $<

coastline_gen: coastline_gen.o
	$(CXX) -o $@ $< $(LDFLAGS)

clean:
	rm -f *.o $(PROGRAMS)

test:
	@echo "This test uses sample data from OpenStreetmap."
	@echo "This data is Copyright OpenStreetMap contributors and licensed under ODBL."
	test -r "greece.sqlite" || wget -O "greece.sqlite" "http://www.imagico.de/coastline_gen/greece.sqlite"
	# greece.sqlite is generated from an OSMCoastline extract using the following
	#ogr2ogr -f "SQLite" -spat 2100000 4000000 3400000 5200000 -clipsrc spat_extent "greece.sqlite" "land_polygons_3857.shp"
	gdal_rasterize -te 2100000 4000000 3400000 5200000 -tr 500 500 -burn 255 -ot Byte "greece.sqlite" "greece.tif"
	./coastline_gen -i "greece.tif" -o "greece_gen.pgm"
	potrace -t 8 -i -b geojson -o "greece_gen.json" "greece_gen.pgm" -x 500 -L 2100000 -B 4000000
