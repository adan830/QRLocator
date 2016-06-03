#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <string>

using namespace cv;
using namespace std;

#define WINDOW_NAME_RAW  "raw"

static int _loadImage( char * name, Mat *image)
{
    string imageName;

	if (NULL == name){
		imageName = "../image/2_usmall.png";
	} else {
		imageName = name;
	}
	
    *image = imread(imageName.c_str(), IMREAD_GRAYSCALE); // Read the file
    if( image->empty() )                      // Check for invalid input
    {
        cout <<  "Could not open or find the image" << std::endl ;
        return -1;
    }

	return 0;
}

static void _showImage(const char* name, Mat image)
{
    namedWindow(name, WINDOW_AUTOSIZE); // Create a window for display.
	imshow(name, image);
}

int main( int argc, char** argv )
{
	int ret;
	Mat rawImg;
	Mat blurImg;

	//load image
    if ( argc > 1) {
		ret = _loadImage(argv[1], &rawImg);
    } else {
		ret = _loadImage(NULL, &rawImg);
	}

	if (0 != ret){
		return ret;
	}

	//blur
	GaussianBlur(rawImg, blurImg, Size(5, 5), 0, 0);
	
	//canny

	_showImage("raw", rawImg);
	_showImage("blur", blurImg);

	imwrite("blur5.png", blurImg);

    waitKey(0); // Wait for a keystroke in the window

    return 0;
}