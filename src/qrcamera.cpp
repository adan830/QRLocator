#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
using namespace std;

#include <stdio.h>
#include "locator.h"

int main( int argc, char* argv[])
{
	int key;
	VideoCapture capture(0);
	Mat raw;
	Mat qrcode;
	Mat binary;

	capture >> raw;
	printf("Raw image size [%d * %d]\n", raw.cols, raw.rows);

	key = 0;
	while( 'q' != key){
		//capture
		capture >> raw;

		//processing
		QR_ProcessImage(raw, binary, qrcode);

		//show image
		imshow("RAW", raw);
		if (false == qrcode.empty()){
			imshow("QR", qrcode);
		}

		key = waitKey(1);
	}

	return 0;
}


