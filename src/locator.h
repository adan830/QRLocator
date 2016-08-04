#ifndef _LOCATOR_H_
#define _LOCATOR_H_

/*The number of bits of subpel precision to store image coordinates in.
  This helps when estimating positions in low-resolution images, which may have
   a module pitch only a pixel or two wide, making rounding errors matter a
   great deal.*/
#define QR_FINDER_SUBPREC (2)

//同一个方向上findler line的最大数量
#define QR_CONFIG_MAX_FINDER_LINE   640

extern void QR_ProcessImage(Mat &raw, Mat &binary, Mat &qrimg);

#endif


