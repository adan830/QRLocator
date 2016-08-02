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
	FIND_STATE_INIT = 0, //wait for white separacter
	FIND_STATE_1,        //wait for black
	FIND_STATE_2,		 //wait for white
	FIND_STATE_3,        //wait for black
	FIND_STATE_4,        //wait for white
	FIND_STATE_5,        //wait for black
	FIND_STATE_6,        //wait for white
	FIND_STATE_FINISH,
};

//point
typedef struct{
	unsigned int x;
	unsigned int y;
}point;

//rectangle
typedef struct {
	point topleft;
	unsigned width;
	unsigned height;
}rectangle;

typedef struct{
	point points[4];
	point center;
}marker;

//用于匹配白:黑:白:黑:白:黑:白的模式，宽度比为n:1:1:3:1:1:n
typedef struct{
	unsigned int w[5];
	unsigned int last;
	unsigned char stage;
}findstate;

//寻找到的穿过marker的线，根据上下文确定是x方向，还是y方向
typedef struct{
	unsigned int start;   //起始点的坐标
	unsigned int end;
	unsigned int otherd;  //other dimension， when start and end
}markerline;

static void _addStage(int pos, int color, findstate *state)
{
	//if already in FIND_STATE_FINISH state, means current state is not a valid finder line
	//so we discard fist two width( finder pattern always start with a black)
	if (FIND_STATE_FINISH == state->stage){
		state->w[0] = state->w[2];
		state->w[1] = state->w[3];
		state->w[2] = state->w[4];
		state->stage = FIND_STATE_5;
	}
	
	pos <<= SUBPREC;
	
	switch (state->stage){
		case FIND_STATE_INIT:
			if (COLOR_WHITE == color){
				state->stage = FIND_STATE_1;
			}
			break;
		case FIND_STATE_1:
			if (COLOR_BLACK == color){
				state->stage = FIND_STATE_2;
				state->last = pos;
			}
			break;
		case FIND_STATE_2:
			if (COLOR_WHITE == color){
				state->w[0] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_3;
			}
			break;
		case FIND_STATE_3:
			if (COLOR_BLACK == color){
				state->w[1] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_4;
			}
			break;
		case FIND_STATE_4:
			if (COLOR_WHITE == color){
				state->w[2] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_5;
			}
			break;
		case FIND_STATE_5:
			if (COLOR_BLACK == color){
				state->w[3] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_6;
			}
			break;
		case FIND_STATE_6:
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
	
	return;
}

static void _resetState(findstate *state)
{
	memset(state, 0, sizeof(*state));
}

static void _scanImage(Mat &binary, vector<markerline> &xlines, vector<markerline> &ylines)
{
	unsigned char *raw;
	unsigned char pixel;
	int x, y;
	int width, height;
	int ret;
	findstate state;
	markerline mline;
	
	CV_Assert(true == binary.isContinuous());
	width = binary.cols;
	height = binary.rows;
	raw = binary.ptr<uchar>(0);

	for (y = 0; y < height; ++y){
		
		_resetState(&state);
		for (x = 0; x < width; ++x){
			
			pixel = raw[y * width + x];
			_addStage(x, pixel, &state);
				
			//test if we find the marker
			ret = _matchState(&state);
			if (1 == ret){
				_state2MarkerLine(y, &state, &mline);
				xlines.push_back(mline);
				_resetState(&state);
			}
		}//for
	}//for
	
	for (x = 0; x < width; ++x){
		
		_resetState(&state);
		for (y = 0; y < height; ++y){
			
			pixel = raw[y * width + x];
			_addStage(y, pixel, &state);

			//test if we find the marker
			ret = _matchState(&state);
			if (1 == ret){
				_state2MarkerLine(x, &state, &mline);
				ylines.push_back(mline);
				_resetState(&state);
			} 
		}//for
	}//for
	
	return;
}

//过滤掉孤立的线，并把markerline分组，输出每组中最外侧线条组成长方形外框
static void _filteIsolate(vector<markerline> &lines)
{

}

//判断长方形外框是否相交，如果相交找出焦点


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

void LOCATER_ProcessImage(Mat &raw, Mat &binary, Mat &qrimg)
{
	Mat gray;
	vector<markerline> xlines;
	vector<markerline> ylines;

	//gray
	cvtColor(raw, gray, CV_RGB2GRAY);

	//threshold
	adaptiveThreshold(gray, binary, COLOR_WHITE, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 35, 5);
	
	//scan image
	_scanImage(binary, xlines, ylines);

	//draw finder lines
	_drawFinderLines(raw, xlines, 1);
	_drawFinderLines(raw, ylines, 0);

	return;
}

