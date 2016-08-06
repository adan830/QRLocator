#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <stdio.h>
#include <iostream>
#include <string>

using namespace cv;
using namespace std;

#include "locator.h"
#include "decoder.h"

static int _loadImage( char * name, Mat *image)
{
    string imageName;

	if (NULL == name){
		imageName = "../image/2_usmall.png";
	} else {
		imageName = name;
	}
	
    *image = imread(imageName.c_str()); // Read the file
    if( image->empty() )                      // Check for invalid input
    {
        cout <<  "Could not open or find the image" << std::endl ;
        return -1;
    }

	return 0;
}

int main( int argc, char** argv )
{
	int ret;
	Mat raw;
	Mat edges;
	Mat qrcode;

	//load image
    if ( argc > 1) {
		ret = _loadImage(argv[1], &raw);
    } else {
		ret = _loadImage(NULL, &raw);
	}

	if (0 != ret){
		return ret;
	}

	QR_CreateDecoder();

	//processing
	QR_ProcessImage(raw, edges, qrcode);

	imshow("RAW", raw);
	imshow("EDGES", edges);

	if (false == qrcode.empty()){
		imshow("QR", qrcode);
		QR_Decode(qrcode.data, qrcode.cols, qrcode.rows);
	}

    waitKey(0); // Wait for a keystroke in the window

    return 0;
} 


