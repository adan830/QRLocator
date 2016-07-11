#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace cv;
using namespace std;

#define COLOR_WHITE 0xFF
#define COLOR_BLACK 0x00
#define SUBPREC  2

enum{
	FIND_STATE_INIT = 0,
	FIND_STATE_1,
	FIND_STATE_2,
	FIND_STATE_3,
	FIND_STATE_4,
	FIND_STATE_5,
	FIND_STATE_FINISH,
};

typedef struct{
	unsigned int x;
	unsigned int y;
}point;

typedef struct{
	point points[4];
	point center;
}marker;

typedef struct{
	unsigned int w[5];
	unsigned int last;
	unsigned char stage;
}findstate;

typedef struct{
	unsigned int start;
	unsigned int end;
	unsigned int otherd;  //other dimension�� when start and end
}markerline;

static void _addStage(int pos, int color, findstate *state)
{
	//if already in FIND_STATE_FINISH state, means current state is not a valid finder line
	//so we discard fist two width( finder pattern always start with a black)
	if (FIND_STATE_FINISH == state->stage){
		state->w[0] = state->w[2];
		state->w[1] = state->w[3];
		state->w[2] = state->w[4];
		state->stage = FIND_STATE_4;
	}
	
	pos <<= SUBPREC;
	
	switch (state->stage){
		case FIND_STATE_INIT:
			if (COLOR_BLACK == color){
				state->last = pos;
				state->stage = FIND_STATE_1;
			}
			break;
		case FIND_STATE_1:
			if (COLOR_WHITE == color){
				state->w[0] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_2;
			}
			break;
		case FIND_STATE_2:
			if (COLOR_BLACK == color){
				state->w[1] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_3;
			}
			break;
		case FIND_STATE_3:
			if (COLOR_WHITE == color){
				state->w[2] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_4;
			}
			break;
		case FIND_STATE_4:
			if (COLOR_BLACK == color){
				state->w[3] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_5;
			}
			break;
		case FIND_STATE_5:
			if (COLOR_WHITE == color){
				state->w[4] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_FINISH;
			}
			break;
		default:
			memset(state, 0, sizeof(*state));
			assert(0);
			break;
	};
}

static int _matchState(findstate *state)
{
	unsigned int unit;
	
	if (FIND_STATE_FINISH != state->stage){
		return 0;
	}
	
	unit = (state->w[0] + state->w[1] + state->w[2] +state->w[3] + state->w[4])/7;
	
	if ((state->w[0] >= (unit * 0.5)) && (state->w[0] <= (unit * 1.5)) &&
	    (state->w[1] >= (unit * 0.5)) && (state->w[1] <= (unit * 1.5)) &&
		(state->w[2] >= (unit * 2.5)) && (state->w[2] <= (unit * 3.5)) &&
		(state->w[3] >= (unit * 0.5)) && (state->w[3] <= (unit * 1.5)) &&
		(state->w[4] >= (unit * 0.5)) && (state->w[4] <= (unit * 1.5))){
		return 1;
	} else {
		return 0;
	}
}

static void _state2MarkerLine(unsigned otherd, findstate *state, markerline *mline)
{
	mline->end = (state->last >> SUBPREC);
	mline->start = ((state->last - state->w[0] - state->w[1] - state->w[2] - state->w[3] - state->w[4]) >> SUBPREC);
	mline->otherd = (otherd);
	/*
	mline->end = (state->last);
	mline->start = ((state->last - state->w[0] - state->w[1] - state->w[2] - state->w[3] - state->w[4]));
	mline->otherd = (otherd); */
	
	return;
}

static void _resetState(findstate *state)
{
	memset(state, 0, sizeof(*state));
}

static void _scanImage(Mat &rawImg, Mat &edgeImg, vector<markerline> &xlines, vector<markerline> &ylines)
{
	unsigned char *raw, *edges;
	int x, y;
	int width, height;
	int ret;
	findstate state;
	markerline mline;
	unsigned int previous, next;
	
	CV_Assert(rawImg.cols == edgeImg.cols &&
			  rawImg.rows == edgeImg.rows &&
			  true == rawImg.isContinuous() &&
			  true == edgeImg.isContinuous());
	width = rawImg.cols;
	height = rawImg.rows;
	raw = rawImg.ptr<uchar>(0);
	edges = edgeImg.ptr<uchar>(0);

	for (y = 0; y < height; ++y){
		
		_resetState(&state);
		for (x = 0; x < width; ++x){
			
			//if edges
			if (COLOR_WHITE == edges[y * width + x]){
				if (x > 0){
					previous = raw[y * width + x - 1];
				} else {
					previous = raw[y * width + x];
				}
				
				if (x < width - 1){
					next = raw[y * width + x + 1];
				} else {
					next = raw[y * width + x];
				}
				
				//add state
				if (previous > next){
					_addStage(x, COLOR_BLACK, &state);
				} else {
					_addStage(x, COLOR_WHITE, &state);
				}
				
				//test if we find the marker
				ret = _matchState(&state);
				if (1 == ret){
					_state2MarkerLine(y, &state, &mline);
					xlines.push_back(mline);
					_resetState(&state);
				}
			}//if
		}//for
	}//for
	
	for (x = 0; x < width; ++x){
		
		_resetState(&state);
		for (y = 0; y < height; ++y){
			
			//if edges
			if (COLOR_WHITE == edges[y * width + x]){
				if (y > 0){
					previous = raw[(y - 1) * width + x];
				} else {
					previous = raw[y * width + x];
				}
				
				if (y < height - 1){
					next = raw[(y + 1) * width + x];
				} else {
					next = raw[y * width + x];
				}
				
				//add state
				if (previous > next){
					_addStage(y, COLOR_BLACK, &state);
				} else {
					_addStage(y, COLOR_WHITE, &state);
				}
				
				//test if we find the marker
				ret = _matchState(&state);
				if (1 == ret){
					_state2MarkerLine(x, &state, &mline);
					ylines.push_back(mline);
					_resetState(&state);
				} 
			}//if
		}//for
	}//for
	
	return;
}

/*
static void _findMark(Mat raw, Mat edges, vector<markerline> &mark)
{

}
*/

static void _drawFinderLines(Mat &img, vector<markerline> &lines, int x)
{
	int i;
	markerline line;
	const Scalar green = Scalar(0, 255, 0);


	printf("find [%lu] lines.\r\n", lines.size());

	for (i = 0; i < lines.size(); ++i){
		line = lines[i];
		if (1 == x){
			cv::line(img, 
				 Point(line.start, line.otherd), 
				 Point(line.end, line.otherd), 
				 green);
		} else {
			cv::line(img, 
				 Point(line.otherd, line.start), 
				 Point(line.otherd, line.end),
				 green);
		}
	}

	return;
}

void LOCATER_ProcessImage(Mat &raw, Mat &edges, Mat &qrimg)
{
	Mat gray;
	vector<markerline> xlines;
	vector<markerline> ylines;

	//gray
	cvtColor(raw, gray, CV_RGB2GRAY);

	//blur
	GaussianBlur(gray, gray, Size(3, 3), 0, 0);

	//canny
	Canny(gray, edges, 100, 150, 3);

	//scan image
	_scanImage(gray, edges, xlines, ylines);

	//draw finder lines
	_drawFinderLines(raw, xlines, 1);
	_drawFinderLines(raw, ylines, 0);

	return;
}

