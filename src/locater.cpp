#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <string>

using namespace cv;
using namespace std;

#define WINDOW_NAME_RAW  "raw"

typedef struct{
	unsigned int x,
	unsigned int y,
}point;

typedef struct{
	point points[4];
	point center;
}marker;

typedef struct{
	unsigned int w[5];
	unsigned int last;
}findstate;

typedef struct{
	unsigned int start;
	unsigned int end;
	unsigned int coor;
}markerline;

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

static void _findMark(Mat raw, Mat edges, vector<rectagle> &mark)
{

}

int main( int argc, char** argv )
{
	int ret;
	Mat rawImg;
	Mat blur3Img;
	Mat blur5Img;
	Mat cannyEdges;

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
	GaussianBlur(rawImg, blur3Img, Size(3, 3), 0, 0);
	GaussianBlur(rawImg, blur5Img, Size(5, 5), 0, 0);
	
	//canny
	Canny(rawImg, cannyEdges, 150, 150, 3);
	_showImage("cannyraw", cannyEdges);
	imwrite("cannyraw.png", cannyEdges);
	Canny(blur3Img, cannyEdges, 150, 150, 3);
	_showImage("cannyblur3", cannyEdges);
	imwrite("cannyblur3.png", cannyEdges);
	Canny(blur5Img, cannyEdges, 150, 150, 3);
	_showImage("cannyblur5", cannyEdges);
	imwrite("cannyblur5.png", cannyEdges);

	/*
	_showImage("raw", rawImg);
	_showImage("blur", blurImg);
	_showImage("canny", cannyEdges);
	imwrite("blur5.png", blurImg);
	*/

    waitKey(0); // Wait for a keystroke in the window

    return 0;
}
