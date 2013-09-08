
coastline_gen simple coastline generalization
=============================================

This text describes the use of the `coastline_gen` tool that implements a
simple raster based algorithm for generalizing coastlines or other polygon 
features for map rendering.

Compiling the program
---------------------

coastline_gen requires the [CImg](http://cimg.sourceforge.net/) image 
processing library to be built and should be compilable on all platforms 
supported by the library.  You need to download this library separately.  
Copying CImg.h to the source directory is sufficient.

The package includes a makefile to simplify the built process.

Program options
---------------

the program offers the following command line options:

* `-i` Input land water mask image file (required)
* `-o` Output image file name for the generalized land water mask (required)
* `-f` Input image file containing a mask to restrict generalization.  Has to be the same size as main input (optional)
* `-c` Input image file containing a mask to collapse small features.  Has to be the same size as main input (optional)
* `-sf` specifies how to interpret the fixed mask: 1: mask repels generalized features; -1: mask attracts generalized features.
* `-rf` influence radius of the fixed mask in pixels
* `-l` allows to set a bias in the coastline position.  Larger values move the coastline to the land.  Default: `0.5`
* `-ls` separate bias level for small features.  Default: `0.5`
* `-il` island threshold level.  Default: `0.06`
* `-r` Radius values for various generalization steps in pixels.  Fractional values are allowed here.  a total of five values separated by colons, their meaning is `normal:feature:min_water:min_land:island:collapse:collapse_mask:collapse_mask2`. Default: `4.0:2.5:1.0:0.5:1.0:0.0:0.0`.
* `-is` Size thresholds for special treatment of islands in pixels.  First value: remove island smaller than this.  Second: try to connect these islands to main land if smaller than this.  Third: enlarge islands up to this size, Fourth: upper limit of island treatment (used for `-ls`).  Default: `8:16:36:120`
* `-ngc` Do not generalized connection pixels in the input data.  Default: off
* `-fcr` Radius for fixing gaps between connection pixels and fixed mask.  Default: `0`
* `-xc` Extend connections.  Default: off
* `-debug` Generate a large number of image files from intermediate steps in the current directory for debugging.  Default: off
* `-h` show available options

All image files are expected to be byte valued grayscale images with land pixel values > 0 and water pixel value 0.  Version 0.5 interprets values of 255
as connection pixels meaning they represent areas connected to the fixed mask (see below) You can 
use any file format supported by CImg.  When using GeoTiff files you will get some warnings that can be safely ignored.  Note though that
coordinate system information in any file is not transferred to the output.  If necessary you have to do that yourself.

The fixed mask image (option `-f`) is interpreted inversely, i.e. pixel values of 0 are 'active' while values of 255 are 'inactive'.  This way the
coastline mask can be used as is as a fixed mask for generalization of other land features.

In the collapse mask image (option `-c`) values of 255 represent areas where small features ('islands') are to be collapsed depending 
on the radius settings.

The program operates purely on the raster image and is unaware of the geographic coordinate system the data is supplied in.  All parameters are specified 
in pixels as basic units.

Using the program
-----------------

Since `coastline_gen` operates on raster images normal use of the program includes the following steps:

1. Rasterizing the land polygons, for example using [gdal_rasterize](http://www.gdal.org/gdal_rasterize.html).
2. Running `coastline_gen`.
3. Vectorizing the resulting image, for example using [potrace](http://potrace.sourceforge.net/).

The makefile included in the source package contains a test target demonstrating this using sample data from 
[OpenStreetMap](http://www.openstreetmap.org/).  Running this test requires wget, [GDAL](http://www.gdal.org/) and 
[potrace](http://potrace.sourceforge.net/).

Legal stuff
-----------

This program is licensed under the GNU GPL version 3.

Copyright 2012-2013 Christoph Hormann

