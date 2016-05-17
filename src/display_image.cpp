#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <string>

using namespace cv;
using namespace std;

#define THRESHOLD_WINDOWN_NAME  "Threshold"

Mat g_grey;
Mat g_threshold;

void on_threshold_change(int pos, void* userdata)
{
	threshold(g_grey, g_threshold, pos, 255, THRESH_BINARY);
	imshow(THRESHOLD_WINDOWN_NAME, g_threshold);
}

int main( int argc, char** argv )
{
    Mat image;
	int defthreshold;

    string imageName("../image/2_usmall.png"); // by default
    if( argc > 1)
    {
        imageName = argv[1];
    }

    image = imread(imageName.c_str(), IMREAD_COLOR); // Read the file
    if( image.empty() )                      // Check for invalid input
    {
        cout <<  "Could not open or find the image" << std::endl ;
        return -1;
    }

	cvtColor(image, g_grey, CV_BGR2GRAY);

    namedWindow(THRESHOLD_WINDOWN_NAME, WINDOW_AUTOSIZE ); // Create a window for display.
	defthreshold = 128;
	createTrackbar(THRESHOLD_WINDOWN_NAME, "Threshold", &defthreshold, 255, on_threshold_change);
	on_threshold_change(128, 0);

    waitKey(0); // Wait for a keystroke in the window

    return 0;
}
