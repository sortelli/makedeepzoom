# makedeepzoom

This application will create both DeepZoom Image (DZI) files, and DeepZoom Collection (DZC) files.

See [OpenSeaDragon](https://github.com/openseadragon/openseadragon)
for an open source JavaScript application that can display DZI
images.

[Microsoft Pivot](http://www.microsoft.com/silverlight/pivotviewer/) can display DeepZoom Collections.

You will need to check
[archive.org](https://web.archive.org/web/20100815081124/http://www.silverlight.net/learn/pivotviewer/)
for the original documentation and downloads of Microsoft Pivot.

## Building / Installing

Build and install using the typical autoconf/make commands:

    % ./configure
    % make
    % make install

To install to a specific directory:

    % ./configure --prefix ~/my/local/dir
    % make
    % make install

## Usage

    usage: makedeepzoom [OPTIONS] image [image, image, ...] 
      OPTIONS
        -v             Print makedeepzoom version.
        -d             Enable debug output.
        -c <filename>  DeepZoom Collection (DZC) file to update with new DZI data.
        -t <integer>   DeepZoom Image (DZI) tile size.  Default is 256 pixels.
        -a <double>    Force DeepZoom Image (DZI) aspect ratio (width / height) to a particular ratio.
                       Default value is 0, which will use the original aspect ratio of each image.
        -f (jpg|png)   Force DeepZoom Image (DZI) format. Default is 'jpg'.
        -n <integer>   Starting index for DeepZoom Collection (DZC) number. Default is 0.
        -m <integer>   Depth of DeepZoom Collection (DZC). Default is 8.
        -o <integer>   DeepZoom Image (DZI) overlap pixels. Default is 1.
        -x             Use '.xml' file extension for DZI files. Default is '.dzi'.
    
        image [image, ...]   List of image files to convert into DeepZoom Image (DZI) files.
    
      Example usage: makedeepzoom -c dzc/foo.dzc images/*.jpg

## Examples

### Create DZIs for one or more images

To create one or more DZI images:

    % ./makedeepzoom image1.jpg image2.jpg image3.jpg

### Create DZIs and a DZC for one or more images

    % ./makedeepzoom -c dzc/collection.dzc image1.jpg image2.jpg image3.jpg

### Update existing DeepZoom Collection with a new image

    % ./makedeppzoom -c dzc/collection.dzc -n <next_image_number> next_image.jpg

## License

The MIT License (MIT)

Copyright (c) 2014 Joe Sortelli

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
