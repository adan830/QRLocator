#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <set>
#include "debug.h"

using namespace cv;
using namespace std;

#include "locator.h"

#define QR_COLOR_WHITE 0xFF
#define QR_COLOR_BLACK 0x00

#define QR_TO_ACTUAL(cor) ((cor) >> QR_FINDER_SUBPREC)
#define QR_TO_CALC(cor)   ((cor) << QR_FINDER_SUBPREC)

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
typedef struct QRPoint{
	int x;
	int y;
}QRPoint;

//用于匹配白:黑:白:黑:白:黑:白的模式，宽度比为n:1:1:3:1:1:n
typedef struct QRFindState{
	int w[5];
	int last;
	char stage;
}QRFindState;

//寻找到的穿过finder的线，根据上下文确定是x方向，还是y方向

/*A line crossing a finder pattern.
  Whether the line is horizontal or vertical is determined by context.
  The offsts to various parts of the finder pattern are as follows:
    |*****|     |*****|*****|*****|     |*****|
    |*****|     |*****|*****|*****|     |*****|
       ^        ^                 ^        ^
       |        |                 |        |
       |        |                 |       pos[v]+len+eoffs
       |        |                pos[v]+len
       |       pos[v]
      pos[v]-boffs
  Here v is 0 for horizontal and 1 for vertical lines.*/
typedef struct QRFinderLine{
  /*The location of the upper/left endpoint of the line.
    The left/upper edge of the center section is used, since other lines must
     cross in this region.*/
  QRPoint pos;
  /*The length of the center section.
    This extends to the right/bottom of the center section, since other lines
     must cross in this region.*/
  int      len;
  /*The offset to the midpoint of the upper/left section (part of the outside
     ring), or 0 if we couldn't identify the edge of the beginning section.
    We use the midpoint instead of the edge because it can be located more
     reliably.*/
  int      boffs;
  /*The offset to the midpoint of the end section (part of the outside ring),
     or 0 if we couldn't identify the edge of the end section.
    We use the midpoint instead of the edge because it can be located more
     reliably.*/
  int      eoffs;
} QRFinderLine;

/*A cluster of lines crossing a finder pattern (all in the same direction).*/
typedef struct QRFinderCluster{
  /*Pointers to the lines crossing the pattern.*/
  QRFinderLine **lines;
  /*The number of lines in the cluster.*/
  int          nlines;
} QRFinderCluster;

//从图片中寻找finder line时使用
static QRFinderLine g_XLines[QR_CONFIG_MAX_FINDER_LINE];
static int g_XLineSize = 0;
static QRFinderLine g_YLines[QR_CONFIG_MAX_FINDER_LINE];
static int g_YLineSize = 0;

//给finder line分组使用
static char g_LineMark[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderLine* g_XNeighbors[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderLine* g_YNeighbors[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderCluster g_XCluster[QR_CONFIG_MAX_FINDER_LINE/2];
static QRFinderCluster g_YCluster[QR_CONFIG_MAX_FINDER_LINE/2];

static void _addStage(int pos, int color, QRFindState *state)
{
	//if already in FIND_STATE_FINISH state, means current state is not a valid finder line
	//so we discard fist two width( finder pattern always start with a black)
	if (FIND_STATE_FINISH == state->stage){
		state->w[0] = state->w[2];
		state->w[1] = state->w[3];
		state->w[2] = state->w[4];
		state->stage = FIND_STATE_5;
	}
	
	pos = QR_TO_CALC(pos);
	
	switch (state->stage){
		case FIND_STATE_INIT:
			if (QR_COLOR_WHITE == color){
				state->stage = FIND_STATE_1;
			}
			break;
		case FIND_STATE_1:
			if (QR_COLOR_BLACK == color){
				state->stage = FIND_STATE_2;
				state->last = pos;
			}
			break;
		case FIND_STATE_2:
			if (QR_COLOR_WHITE == color){
				state->w[0] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_3;
			}
			break;
		case FIND_STATE_3:
			if (QR_COLOR_BLACK == color){
				state->w[1] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_4;
			}
			break;
		case FIND_STATE_4:
			if (QR_COLOR_WHITE == color){
				state->w[2] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_5;
			}
			break;
		case FIND_STATE_5:
			if (QR_COLOR_BLACK == color){
				state->w[3] = pos - state->last;
				state->last = pos;
				state->stage = FIND_STATE_6;
			}
			break;
		case FIND_STATE_6:
			if (QR_COLOR_WHITE == color){
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

static int _matchState(QRFindState *state)
{
	int unit;
	int half;
	
	if (FIND_STATE_FINISH != state->stage){
		return 0;
	}
	
	unit = (state->w[0] + state->w[1] + state->w[2] +state->w[3] + state->w[4])/7;
	half = unit/2;
	
	if ((state->w[0] > (unit - half)) && (state->w[0] < (unit + half)) &&
	    (state->w[1] > (unit - half)) && (state->w[1] < (unit + half)) &&
		(state->w[2] > (unit*3 - half)) && (state->w[2] < (unit*3 + half)) &&
		(state->w[3] > (unit - half)) && (state->w[3] < (unit + half)) &&
		(state->w[4] > (unit - half)) && (state->w[4] < (unit + half))){
		return 1;
	} else {
		return 0;
	}
}

static void _addXFinderLine(int y, QRFindState *state)
{
	QRFinderLine *fline;

	if (g_XLineSize >= QR_CONFIG_MAX_FINDER_LINE){
		ASSERT(0);
		return;
	}

	fline = g_XLines + g_XLineSize;
	g_XLineSize += 1;

	fline->pos.x = (state->last - state->w[2] - state->w[3] - state->w[4]);
	fline->pos.y = QR_TO_CALC(y);
	fline->len = state->w[2];
	fline->boffs = state->w[1] + state->w[0]/2;
	fline->eoffs = state->w[3] + state->w[4]/2;
	
	return;
}

static void _addYFinderLine(int x, QRFindState *state)
{
	QRFinderLine *fline;

	if (g_YLineSize >= QR_CONFIG_MAX_FINDER_LINE){
		ASSERT(0);
		return;
	}
	
	fline = g_YLines + g_YLineSize;
	g_YLineSize += 1;

	fline->pos.x = QR_TO_CALC(x);
	fline->pos.y = (state->last - state->w[2] - state->w[3] - state->w[4]);	
	fline->len = state->w[2];
	fline->boffs = state->w[1] + state->w[0]/2;
	fline->eoffs = state->w[3] + state->w[4]/2;
	
	return;
}

static void _resetState(QRFindState *state)
{
	memset(state, 0, sizeof(*state));
}

static void _scanImage(Mat &binary)
{
	unsigned char *raw;
	unsigned char pixel;
	int x;
	int y;
	int width;
	int height;
	int ret;
	QRFindState state;
	
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
				_addXFinderLine(y, &state);
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
				_addYFinderLine(x, &state);
			} 
		}//for
	}//for
	
	return;
}

static int _isOneCluster(QRFinderLine *a, QRFinderLine *b)
{
	
}

//过滤掉孤立的线，并把markerline分组，输出每组中最外侧线条组成长方形外框
static int _clusterLines(QRFinderLine *lines, int nline, QRFinderLine** neighbors, QRFinderCluster *cluster)
{
	int i;
	int j;
	int nneighbors;
	int nclusters;
	QRFinderLine *a;
	QRFinderLine *b;
	int thresh;
	int totallen;

	nclusters = 0;
	totallen = 0;
	memset(g_LineMark, 0, sizeof(g_LineMark));
	for (i = 0; i < nline; ++i){
		if (0 != g_LineMark[i]){
			continue;
		}

		nneighbors = 0;
		neighbors[nneighbors++] = lines + i;
		totallen = neighbors[nneighbors - 1]->len;
		
		for (j = i + 1; j < nline; ++j){
			if (0 != g_LineMark[i]){
				continue;
			}

			a = neighbors[nneighbors - 1];
			b = lines + j;

			if (0 == _isOneCluster(a, b)){
				continue;
			}

			neighbors[nneighbors++] = lines + j;
			totallen += b->len;
		}

		if (nneighbors < 3){
			continue;
		}

		cluster[nclusters].lines = neighbors;
		cluster[nclusters].nlines = nneighbors;
		neighbors += nneighbors;
		nclusters += 1;
	}

	return nclusters;
}

//画出finder line
static void _drawXFinderLines(Mat &img)
{
	int i;
	QRFinderLine *line;
	const Scalar green = Scalar(0, 255, 0);

	printf("find [%d] xlines.\r\n", g_XLineSize);

	for (i = 0; i < g_XLineSize; ++i){
		line = g_XLines + i;
		cv::line(img, 
			 Point(QR_TO_ACTUAL(line->pos.x - line->boffs), QR_TO_ACTUAL(line->pos.y)), 
			 Point(QR_TO_ACTUAL(line->pos.x + line->len + line->eoffs), QR_TO_ACTUAL(line->pos.y)), 
			 green);
	}

	return;
}

static void _drawYFinderLines(Mat &img)
{
	int i;
	QRFinderLine *line;
	const Scalar green = Scalar(0, 255, 0);


	printf("find [%d] ylines.\r\n", g_YLineSize);

	for (i = 0; i < g_YLineSize; ++i){
		line = g_YLines + i;
		cv::line(img, 
				Point(QR_TO_ACTUAL(line->pos.x), QR_TO_ACTUAL(line->pos.y - line->boffs)), 
				Point(QR_TO_ACTUAL(line->pos.x), QR_TO_ACTUAL(line->pos.y + line->len + line->eoffs)),
				green);
	}

	return;
}

void QR_ProcessImage(Mat &raw, Mat &binary, Mat &qrimg)
{
	Mat gray;
	Mat elem;

	//gray
	cvtColor(raw, gray, CV_RGB2GRAY);

	//threshold
	adaptiveThreshold(gray, binary, QR_COLOR_WHITE, ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY, 35, 5);
	//imshow("Threadhold", binary);

	//消除噪点
	elem = getStructuringElement(MORPH_ELLIPSE, Size(5, 5));
	morphologyEx(binary, binary, MORPH_CLOSE, elem);
	//imshow("Close", binary);

	//填充空洞
//	morphologyEx(binary, binary, MORPH_OPEN, elem);
	//imshow("Open", binary);

	//scan image
	_scanImage(binary);

	//draw finder lines
	_drawXFinderLines(raw);
	_drawYFinderLines(raw);

	return;
}

