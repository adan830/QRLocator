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
typedef int QRPoint[2];


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

typedef struct QRFinderCenter{
	QRPoint pos; //finder 中心的位置
	int     len; //finder 中心黑块的宽度
} QRFinderCenter;

const Scalar g_Green = Scalar(0, 255, 0);
const Scalar g_Red = Scalar(0, 0, 255);

//从图片中寻找finder line时使用
static QRFinderLine g_XLines[QR_CONFIG_MAX_FINDER_LINE];
static int g_XLineSize = 0;
static QRFinderLine g_YLines[QR_CONFIG_MAX_FINDER_LINE];
static int g_YLineSize = 0;

//给cluster lines 使用
static char g_LineMark[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderLine* g_XNeighbors[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderLine* g_YNeighbors[QR_CONFIG_MAX_FINDER_LINE];
static QRFinderCluster g_XClusters[QR_CONFIG_MAX_FINDER_LINE/2];
static int g_nXClusters;
static QRFinderCluster g_YClusters[QR_CONFIG_MAX_FINDER_LINE/2];
static int g_nYClusters;

//给cross clusters 使用
static QRFinderCluster* g_XCNeighbors[QR_CONFIG_MAX_FINDER_LINE/2];
static QRFinderCluster* g_YCNeighbors[QR_CONFIG_MAX_FINDER_LINE/2];

//给finder center使用
static QRFinderCenter g_Centers[QR_CONFIG_MAX_FINDER_CENTER];
static int g_nCenters = 0;

static void _drawFinderLines(Mat &img, QRFinderLine* lines, int lsize, int _v);

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

	fline->pos[0] = (state->last - state->w[2] - state->w[3] - state->w[4]);
	fline->pos[1] = QR_TO_CALC(y);
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

	fline->pos[0] = QR_TO_CALC(x);
	fline->pos[1] = (state->last - state->w[2] - state->w[3] - state->w[4]);	
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

//过滤掉孤立的线，并把markerline分组，输出每组中最外侧线条组成长方形外框
static int _clusterLines(QRFinderLine *lines, int nline, QRFinderLine** neighbors, QRFinderCluster *cluster, int _v)
{
	int i;
	int j;
	int nneighbors;
	int nclusters;
	QRFinderLine *a;
	QRFinderLine *b;
	int thresh;
	int len;

	nclusters = 0;
	len = 0;
	memset(g_LineMark, 0, sizeof(g_LineMark));
	for (i = 0; i < nline; ++i){
		if (0 != g_LineMark[i]){
			continue;
		}

		nneighbors = 0;
		neighbors[nneighbors++] = lines + i;
		len = neighbors[nneighbors - 1]->len;
		
		for (j = i + 1; j < nline; ++j){
			if (0 != g_LineMark[i]){
				continue;
			}

			a = neighbors[nneighbors - 1];
			b = lines + j;

			thresh=a->len+7>>2;
			if(abs(a->pos[1-_v]-b->pos[1-_v])>thresh)break;
			if(abs(a->pos[_v]-b->pos[_v])>thresh)continue;
			if(abs(a->pos[_v]+a->len-b->pos[_v]-b->len)>thresh)continue;
			if(a->boffs>0&&b->boffs>0&&
			 abs(a->pos[_v]-a->boffs-b->pos[_v]+b->boffs)>thresh){
			  continue;
			}
			if(a->eoffs>0&&b->eoffs>0&&
			 abs(a->pos[_v]+a->len+a->eoffs-b->pos[_v]-b->len-b->eoffs)>thresh){
			  continue;
			}

			neighbors[nneighbors++] = lines + j;
			ASSERT(nneighbors < QR_CONFIG_MAX_FINDER_LINE);
			len += b->len;
		}

		if (nneighbors < 3){
			continue;
		}

		len = ((len<<1)+nneighbors)/(nneighbors<<1);
		if (QR_TO_CALC(nneighbors)*5 >= len){
			cluster[nclusters].lines = neighbors;
			cluster[nclusters].nlines = nneighbors;
			for(j=0;j<nneighbors;j++)g_LineMark[neighbors[j]-lines]=1;
			neighbors += nneighbors;
			nclusters += 1;
			ASSERT(nclusters < QR_CONFIG_MAX_FINDER_LINE/2);
		}
	}

	return nclusters;
}

/*Determine if a horizontal line crosses a vertical line.
  _hline: The horizontal line.
  _vline: The vertical line.
  Return: A non-zero value if the lines cross, or zero if they do not.*/
static int _linesAreCrossing(const QRFinderLine *_hline,
 const QRFinderLine *_vline){
  return
   _hline->pos[0]<=_vline->pos[0]&&_vline->pos[0]<_hline->pos[0]+_hline->len&&
   _vline->pos[1]<=_hline->pos[1]&&_hline->pos[1]<_vline->pos[1]+_vline->len;
}

static void _calcMiddleLine(QRFinderCluster** clusters, int nCluster, int _v, QRFinderLine* line)
{
	QRFinderCluster* c;
	int i;
	int j;
	int len;
	int start;
	int count;
	int min;
	int max;

	start = 0;
	len = 0;
	count = 0;
	min = 0x7FFFFFFF;
	max = 0;
	for (i = 0; i < nCluster; ++i){
		c = clusters[i];
		for (j = 0; j < c->nlines; ++j){
			start += c->lines[j]->pos[_v];
			len += c->lines[j]->len;
			count += 1;

			if (c->lines[j]->pos[1-_v] > max){
				max = c->lines[j]->pos[1-_v];
			}

			if (c->lines[j]->pos[1-_v] < min){
				min = c->lines[j]->pos[1-_v];
			}
		}
	}

	line->pos[_v] = (start + (count >> 2))/count;
	line->pos[1-_v] = (min + max + 1)/2;
	line->len = (len + (count >> 2))/count;
	line->boffs = 0;
	line->eoffs = 0;
	
	return;
}

static int _findCrossing(QRFinderCenter *centers, int centerSize, 
						  QRFinderCluster* xClusters, int nxCluster, 
						  QRFinderCluster* yClusters, int nyCluster,
						  QRFinderCluster** xNeighbors,
						  QRFinderCluster** yNeighbors)
{
	int i;
	int j;
	char *xMark = g_LineMark;
	char *yMark = g_LineMark + QR_CONFIG_MAX_FINDER_LINE/2;
	QRFinderLine *a;
	QRFinderLine *b;
	QRFinderLine xMiddleLine;
	QRFinderLine yMiddleLine;
	int nxNeighbors;	
	int nyNeighbors;
	int nCenters;

	memset(g_LineMark, 0 ,sizeof(g_LineMark));
	nCenters = 0;
	
	for (i = 0; i < nxCluster; ++i){
		if (1 == xMark[i]){
			continue;
		}

		nyNeighbors = 0;
		a = xClusters[i].lines[xClusters[i].nlines>>1];
		for (j = 0; j < nyCluster; ++j){
			if (1 == yMark[j]){
				continue;
			}

			b = yClusters[j].lines[yClusters[j].nlines>>1];
			if (_linesAreCrossing(a, b)){
				yMark[j],
				yNeighbors[nyNeighbors++] = yClusters + j;
			}
		}

		if (nyNeighbors > 0){
			_calcMiddleLine(yNeighbors, nyNeighbors, 1, &yMiddleLine);
			nxNeighbors = 0;
			xNeighbors[nxNeighbors++] = xClusters+i;
			
			for (j = i + 1; j < nxCluster; ++j){
				if (1 == xMark[i]){
					continue;
				}

				a = xClusters[j].lines[xClusters[j].nlines>>1];
				if (_linesAreCrossing(a, &yMiddleLine)){
					xMark[j]=1;
					xNeighbors[nxNeighbors++] = xClusters + j;
				}
			}

			_calcMiddleLine(xNeighbors, nxNeighbors, 0, &xMiddleLine);

			if (nCenters < centerSize){
				centers[nCenters].pos[0] = yMiddleLine.pos[0];
				centers[nCenters].pos[1] = xMiddleLine.pos[1];
				centers[nCenters].len = (yMiddleLine.len + xMiddleLine.len) / 2;
				nCenters += 1;
			} else {
				ASSERT(0);
			}
		}
	}

	return nCenters;
}

static void _drawXFinderLine(Mat &img, QRFinderLine *line)
{
	cv::line(img, 
			 Point(QR_TO_ACTUAL(line->pos[0] - line->boffs), QR_TO_ACTUAL(line->pos[1])), 
			 Point(QR_TO_ACTUAL(line->pos[0] + line->len + line->eoffs), QR_TO_ACTUAL(line->pos[1])), 
			 g_Green);

	return;
}

static void _drawYFinderLine(Mat &img, QRFinderLine *line)
{
	cv::line(img, 
			Point(QR_TO_ACTUAL(line->pos[0]), QR_TO_ACTUAL(line->pos[1] - line->boffs)), 
			Point(QR_TO_ACTUAL(line->pos[0]), QR_TO_ACTUAL(line->pos[1] + line->len + line->eoffs)),
			g_Green);

	return;
}

//画出finder line
static void _drawFinderLines(Mat &img, QRFinderLine* lines, int lsize, int _v)
{
	int i;

	printf("find [%d] lines.\r\n", lsize);

	for (i = 0; i < lsize; ++i){
		if (0 == _v){
			_drawXFinderLine(img, lines + i);
		} else {
			_drawYFinderLine(img, lines + i);
		}
	}

	return;
}

//画出cluster
static void _drawCluster(Mat &img, QRFinderCluster *cluster, int clusterSize, int _v)
{
	int i;
	int j;

	for (i = 0; i < clusterSize; ++i){
		for (j = 0; j < cluster->nlines; ++j){
			if (0 == _v){
				_drawXFinderLine(img, cluster->lines[j]);
			} else {
				_drawYFinderLine(img, cluster->lines[j]);
			}
		}
		cluster += 1;
	}

	return;
}

static void _drawCenters(Mat &img, QRFinderCenter *centers, int nCenters)
{
	int i;

	for (i = 0; i < nCenters; ++i){

		cv::line(img, 
				 Point(QR_TO_ACTUAL(centers[i].pos[0]) - 3, QR_TO_ACTUAL(centers[i].pos[1])), 
				 Point(QR_TO_ACTUAL(centers[i].pos[0]) + 3, QR_TO_ACTUAL(centers[i].pos[1])),
				 g_Red);

		cv::line(img, 
				 Point(QR_TO_ACTUAL(centers[i].pos[0]), QR_TO_ACTUAL(centers[i].pos[1]) - 3), 
				 Point(QR_TO_ACTUAL(centers[i].pos[0]), QR_TO_ACTUAL(centers[i].pos[1]) + 3),
				 g_Red);		
	}

	return;
}

//过滤finder line
static void _findCenters(void)
{	
	//分组
	g_nXClusters = _clusterLines(g_XLines, g_XLineSize, g_XNeighbors, g_XClusters, 0);
	g_nYClusters = _clusterLines(g_YLines, g_YLineSize, g_YNeighbors, g_YClusters, 1);

	
	//判断cluster是否交叉
	g_nCenters = _findCrossing(g_Centers, sizeof(g_Centers)/sizeof(g_Centers[0]),
    						  g_XClusters, g_nXClusters,
    						  g_YClusters, g_nYClusters,
    						  g_XCNeighbors, g_YCNeighbors);

	return;
}

//找出QR的外框并进行剪裁
static int _findQRSquare(Mat &raw, Mat &qrimg)
{	
	int minx;
	int miny;
	int maxx;
	int maxy;
	int i;
	int len;

	if (g_nCenters < 3){
		return -1;
	}

	minx = 0x7FFFFFFF;
	miny = 0x7FFFFFFF;
	maxx = 0;
	maxy = 0;
	len = 0;

	//找出最大边框
	for (i = 0; i < g_nCenters; ++i){
		if (g_Centers[i].pos[0] < minx){
			minx = g_Centers[i].pos[0];
		}

		if (g_Centers[i].pos[0] > maxx){
			maxx = g_Centers[i].pos[0];
		}

		if (g_Centers[i].pos[1] < miny){
			miny = g_Centers[i].pos[1];
		}

		if (g_Centers[i].pos[1] > maxy){
			maxy = g_Centers[i].pos[1];
		}

		len += g_Centers[i].len;
	}

	//finder中心黑块的凭据长度
	len /= g_nCenters;
	len = len*8/3;

	len = QR_TO_ACTUAL(len);
	minx = QR_TO_ACTUAL(minx);
	miny = QR_TO_ACTUAL(miny);
	maxx = QR_TO_ACTUAL(maxx);
	maxy = QR_TO_ACTUAL(maxy);

	//将裁剪边界扩张一定尺寸
	if (minx - len >= 0){
		minx = minx - len;
	} else {
		minx = 0;
	}
	
	if (miny - len >= 0){
		miny = miny - len;
	} else {
		miny = 0;
	}
	
	if (maxx + len < raw.cols){
		maxx = maxx + len;
	} else {
		maxx = raw.cols;
	}

	if (maxy + len < raw.rows){
		maxy = maxy + len;
	} else {
		maxy = raw.rows;
	}

	{
		Mat _tmp(raw, Rect(minx, miny, maxx - minx, maxy - miny));
		qrimg = _tmp.clone();
	}

	return 0;
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
	elem = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
	morphologyEx(binary, binary, MORPH_CLOSE, elem);
	//imshow("Close", binary);

	//scan image
	_scanImage(binary);

	//find centers
	_findCenters();

	//find qr square
	_findQRSquare(gray, qrimg);

	//画出finder line
	//_drawFinderLines(raw, g_XLines, g_XLineSize, 0);
	//_drawFinderLines(raw, g_YLines, g_YLineSize, 1);	

	//画出cluster
	_drawCluster(raw, g_XClusters, g_nXClusters, 0);
	_drawCluster(raw, g_YClusters, g_nYClusters, 1);

	//画出点
	_drawCenters(raw, g_Centers, g_nCenters);

	return;
}

