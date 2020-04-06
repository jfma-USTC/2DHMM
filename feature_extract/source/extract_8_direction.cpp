// EngOCR.cpp : 定义控制台应用程序的入口点。
//
#if WIN32 
#include "opencv2/opencv.hpp" 
//#include "cv.h"
//#include "highgui.h"
#else
#include <opencv2/opencv.hpp>
#endif

//#include <vld.h>

#include <stdio.h>
#include <string>
#include <iostream>

using namespace std;
using namespace cv;

//#define VS2017
//#define SHOW_IMG
#define NORM_COLS 32
#define NORM_ROWS 64
//#define NORM_D 1024
#define  ARAN 80

float * gauss_win = NULL;

#define swap16(x)  ( (((x)<<8) & 0xff00) | (((x)>>8) & 0x0ff) )
#define swap32(x)  ( (((x)<<24) & 0xff000000) | (((x)<<8) & 0x0ff0000) |  (((x)>>8) & 0x0ff00) | (((x)>>24) & 0x0ff) )


bool sortByM1(Rect m, Rect n)
{
	return m.x < n.x;
}

int calcLineHeight(Mat imgOri, vector<Point> &points)
{
	//Mat tempImg = imgOri.clone();
	Mat binImg = imgOri.clone();
	cv::threshold(imgOri, binImg, 0, 255, CV_THRESH_OTSU);
	binImg = 255 - binImg;
	Mat tempImg = binImg.clone();
	//imshow("1",tempImg);
	//waitKey();
	vector<vector<Point> > contour;
	findContours(tempImg, contour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	vector<Rect> rectVec_rank;
	rectVec_rank.clear();
	for (int k = 0; k < contour.size(); k++)
	{
		Rect currRect = boundingRect(contour[k]);
		if (currRect.width > 10 || currRect.height > 10)
		{
			rectVec_rank.push_back(currRect);
		}
	}

	std::sort(rectVec_rank.begin(), rectVec_rank.end(), sortByM1);

	vector<Rect> rectVec;
	rectVec.clear();
	for (int k = 0; k < rectVec_rank.size(); k++)
	{
		Rect currRect = rectVec_rank[k];
		if (currRect.width > 10 || currRect.height > 10)
		{
			if (rectVec.empty())
			{
				rectVec.push_back(currRect);
			}
			else
			{
				Rect lastRect = rectVec[rectVec.size() - 1];
				int x1_start = lastRect.x;
				int x1_end = lastRect.x + lastRect.width;
				int x2_start = currRect.x;
				int x2_end = currRect.x + currRect.width;

				int startMax = x1_start > x2_start ? x1_start : x2_start;
				int endMin = x1_end < x2_end ? x1_end : x2_end;

				if (startMax - endMin < 5)
				{
					//float widthMin = lastRect.width < currRect.width ? lastRect.width : currRect.width;
					//if( end - start > 0.0*widthMin )
					//{
					int xFuseStart = lastRect.x < currRect.x ? lastRect.x : currRect.x;
					int yFuseStart = lastRect.y < currRect.y ? lastRect.y : currRect.y;
					int xFuseEnd = (lastRect.x + lastRect.width) > (currRect.x + currRect.width) ? (lastRect.x + lastRect.width) : (currRect.x + currRect.width);
					int yFuseEnd = (lastRect.y + lastRect.height) > (currRect.y + currRect.height) ? (lastRect.y + lastRect.height) : (currRect.y + currRect.height);
					rectVec.pop_back();
					rectVec.push_back(Rect(xFuseStart, yFuseStart, xFuseEnd - xFuseStart, yFuseEnd - yFuseStart));
					//}
					//else
					//{
					//	rectVec.push_back(currRect);
					//}
				}
				else
				{
					rectVec.push_back(currRect);
				}
			}
		}
	}

	points.clear();
	int xPre = 0;
	int yPre = 0;
	if (rectVec.empty())
	{
		yPre = imgOri.rows / 2;
	}
	else
	{
		yPre = rectVec[0].y + rectVec[0].height / 2;
	}

	for (int i = 0; i < rectVec.size(); i++)
	{
		int xCore = 0;
		int yCore = 0;
		int foregroundPixelCnt = 0;
		for (int m = rectVec[i].y; m < rectVec[i].y + rectVec[i].height; m++)
		{
			for (int n = rectVec[i].x; n < rectVec[i].x + rectVec[i].width; n++)
			{
				if (binImg.at<uchar>(m, n) == 255)
				{
					foregroundPixelCnt++;
					xCore += n;
					yCore += m;
				}
			}
		}
		if (foregroundPixelCnt <= 0)
		{
			foregroundPixelCnt = 1;
			printf("foregroundPixelCnt <= 0 \n");
		}
		xCore /= foregroundPixelCnt;
		yCore /= foregroundPixelCnt;

		float slope = (float)(yCore - yPre) / (float)(rectVec[i].x - xPre);
		for (int j = xPre; j < rectVec[i].x; j++)
		{
			points.push_back(Point(j, yPre + (j - xPre)*slope));
		}
		for (int j = rectVec[i].x; j < rectVec[i].x + rectVec[i].width; j++)
		{
			points.push_back(Point(j, yCore));
		}

		xPre = rectVec[i].x + rectVec[i].width;
		yPre = yCore;
	}
	for (int i = xPre; i < imgOri.cols; i++)
	{
		points.push_back(Point(i, yPre));
	}

	int sumArea = 0;
	int sumWidth = 0;
	for (int i = 0; i < rectVec.size(); i++)
	{
		sumArea += rectVec[i].height*rectVec[i].width;
		sumWidth += rectVec[i].width;
	}
	sumWidth = sumWidth > 0 ? sumWidth : 1;
	int lineHeight = sumArea / sumWidth;

	lineHeight = lineHeight > 0 ? lineHeight : imgOri.rows;

	return lineHeight;
}

Mat lineNormbyCentroid(Mat imgOri, vector<Point> &points)
{
	int lineHeight = calcLineHeight(imgOri, points);
	float ratio = 60.0 / lineHeight;
	int normHeight = imgOri.rows*ratio;
	int normWidth = imgOri.cols*ratio;
	Mat normImg(normHeight, normWidth, CV_8UC1);
	cv::resize(imgOri, normImg, normImg.size());
#ifdef SHOW_IMG
	imwrite("./binary.bmp", normImg);
	namedWindow("Otsu", 0);
	imshow("Otsu", normImg);
#endif

	int topPad = 100;
	int bottomPad = 100;
	int leftPad = 40;
	int rightPad = 40;

	Mat normPadImg(normHeight + topPad + bottomPad, normWidth + leftPad + rightPad, CV_8UC1);
	cv::copyMakeBorder(normImg, normPadImg, topPad, bottomPad, leftPad, rightPad, cv::BORDER_CONSTANT, cv::Scalar(255));

	vector<Point> normPoints;
	normPoints.clear();
	for (int i = 0; i < leftPad; i++)
	{
		int	y = points[0].y;
		normPoints.push_back(Point(i, y*ratio + topPad));
	}

	for (int i = 0; i < points.size(); i++)
	{
		int pointNowX = points[i].x * ratio + leftPad;
		if (pointNowX == normPoints[normPoints.size() - 1].x)
		{
			continue;
		}
		else
		{
			for (int j = normPoints[normPoints.size() - 1].x + 1; j <= pointNowX; j++)
			{
				normPoints.push_back(Point(j, points[i].y*ratio + topPad));
			}
		}
	}

	Point point(normPoints[normPoints.size() - 1].x + 1, normPoints[normPoints.size() - 1].y);
	for (int i = 0; i < rightPad; i++)
	{
		normPoints.push_back(Point(point.x + i, point.y));
	}

	points.clear();
	for (int i = 0; i < normPoints.size(); i++)
	{
		points.push_back(normPoints[i]);
	}

	return normPadImg;
}

void HanningFilter(Mat srcImg, Mat dstImg)
{
	int rows = srcImg.rows;
	int cols = srcImg.cols;

	Mat tempSrc(rows, cols, CV_8UC1);
	Mat tempImg(rows, cols, CV_8UC1);

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			tempSrc.data[i * cols + j] = 255 - srcImg.data[i * cols + j];
		}
	}

	for (int j = 0; j < cols; j++)
	{
		float w = 0.5 * (1 - cos(2 * CV_PI * (j / float(cols))));

		for (int i = 0; i < rows; i++)
		{
			tempImg.data[i * cols + j] = int(tempSrc.data[i * cols + j] * w);
		}
	}

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			tempSrc.data[i * cols + j] = 255 - tempImg.data[i * cols + j];
		}
	}

	tempSrc.copyTo(dstImg);
}

void GetImgInfo(string srcImgPath, string &dirPath, string &imgName, string &imgType)
{
	int nPos1 = srcImgPath.find_last_of("\\");
	int nPos2 = srcImgPath.find_last_of("/");
	int nPos = nPos1;

	if (nPos1 < nPos2)
	{
		nPos = nPos2;
	}

	dirPath = srcImgPath.substr(0, nPos);
	string name = srcImgPath.substr(nPos + 1, srcImgPath.length() - 1);
	nPos = name.find_last_of(".");
	imgName = name.substr(0, nPos);
	imgType = name.substr(nPos + 1, name.length() - 1);
}

Mat aspect_ratio_adaptive_normalization(Mat img, int w, int h, int L)
{
	int max_x = -1;
	int max_y = -1;
	int min_x = 9999999;
	int min_y = 9999999;

	for (int i = 0; i < h; i++)//行数
	{
		for (int j = 0; j < w; j++)//列数
		{
			if (img.ptr(i)[j] < 255)
			{
				if (i > max_y) max_y = i;
				if (i < min_y) min_y = i;
				if (j > max_x) max_x = j;
				if (j < min_x) min_x = j;
			}
		}

	}

	int x_center = (max_x + min_x) / 2;
	int y_center = (max_y + min_y) / 2;

	Mat img_tmp(L, L, CV_8UC1);    //注意释放
	if (max_x < min_x || max_y < min_y)
	{
		img_tmp.setTo(Scalar(255));
		return img_tmp;
	}

	int W1 = max_x - min_x + 1;
	int H1 = max_y - min_y + 1;//bound box

	int min_tmp = (W1 < H1) ? W1 : H1;
	int max_tmp = (W1 > H1) ? W1 : H1;
	double R1 = min_tmp / ((double)(max_tmp));
	double R2 = R1;

	int W2, H2;
	if (W1 < H1)
	{
		H2 = L;
		W2 = L * R2;
	}
	else
	{
		W2 = L;
		H2 = L * R2;
	}

	for (int i_x = 0; i_x < L; i_x++)//第几列
	{
		for (int i_y = 0; i_y < L; i_y++)//第几行
		{
			int x_old = (i_x - L / 2 + W2 / 2)*(W1 - 1) / (W2 - 1) + min_x;
			int y_old = (i_y - L / 2 + H2 / 2)*(H1 - 1) / (H2 - 1) + min_y;
			if (x_old < w && x_old >= 0 && y_old < h && y_old >= 0)
			{
				img_tmp.ptr(i_y)[i_x] = img.ptr(y_old)[x_old];

			}
			else
			{
				img_tmp.ptr(i_y)[i_x] = 255;
			}
		}
	}
	return img_tmp;
}

float * calc_gauss_window(int N)  //for gaussion blur
{
	assert(N % 2 == 0);
	float sigma = 3.6;
	float * gauss_mask = new float[N*N]; //注意释放
	for (int i = -N / 2; i < N / 2; i++)
	{
		for (int j = -N / 2; j < N / 2; j++)
		{
			gauss_mask[(i + N / 2)*N + (j + N / 2)] = exp(-(i * i + j * j) / (2 * sigma * sigma)) / (2 * 3.1415926 * sigma * sigma);
		}
	}
	return gauss_mask;
}

void calcOtsu(Mat grayimg, int width, int height) //计算一个阈值，二值化
{
	int thresholdValue = 1; // 阈值
	int ihist[256]; // 图像直方图，256个点

	int n, n1, n2;
	double m1, m2, sum, csum, fmax, sb;

	// 对直方图置零
	memset(ihist, 0, sizeof(ihist));

	//获取直方图
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < height; j++)
		{
			//unsigned char temp=grayimg[i*width+j];
			unsigned char temp = grayimg.ptr(j)[i];
			//int temp_int =int (temp);
			//fprintf(stdout,"%c",temp);
			ihist[temp]++;
			//fprintf(stdout,"%c\n",temp);
			//fprintf(stdout,"%d %d %d \n",i,j,temp_int);

		}
	}

	//获取大津阈值
	sum = csum = 0.0;
	n = 0;
	for (int k = 0; k <= 255; k++)
	{
		sum += (double)k * (double)ihist[k];   // x*f(x) 质量矩//所有像素值之和
		n += ihist[k];                       //f(x) 质量//不用求，总和==图像大小
	}

	fmax = -1.0;
	n1 = 0;
	for (int k = 0; k < 255; k++)
	{
		n1 += ihist[k];
		if (!n1) { continue; }
		n2 = n - n1;
		if (n2 == 0) { break; }
		csum += (double)k *ihist[k];
		m1 = csum / n1;//第一类的均值
		m2 = (sum - csum) / n2;//第二类的均值
		sb = (double)n1 *(double)n2 *(m1 - m2) * (m1 - m2);
		if (sb > fmax)
		{
			fmax = sb;
			thresholdValue = k;
		}
	}

	for (int i = 0; i < n; i++)
	{
		int ww = i / grayimg.cols;//第几行
		int hh = i % grayimg.cols;//第几列

		if (grayimg.ptr(ww)[hh] > thresholdValue)
		{
			grayimg.ptr(ww)[hh] = 255;
		}
		else
		{
			grayimg.ptr(ww)[hh] = 0;
		}
	}

}

float * grad_feature_extract(Mat img, int width, int height, float* gauss_wds)//列数 行数
{
	int size_wds = 32;
	//Mat aran_img_mat = aspect_ratio_adaptive_normalization_without_boundbox(img,width,height,64);
	Mat aran_img_mat(NORM_ROWS, NORM_COLS, CV_8UC1);
	resize(img, aran_img_mat, Size(NORM_COLS, NORM_ROWS));

#ifdef SHOW_IMG
	//imwrite("./64x32.bmp",aran_img_mat);
	namedWindow("aran_img_64*32",0);
	imshow("aran_img_64*32",aran_img_mat);
	//waitKey();
#endif

	unsigned char* aran_img = new unsigned char[64 * 32];
	for (int i = 0; i < 64; i++)
	{
		for (int j = 0; j < 32; j++)
		{
			aran_img[i * 32 + j] = aran_img_mat.ptr(i)[j];
		}

	}
	//_CrtDumpMemoryLeaks();
	//aran_img_mat.release();
	float direct_img_1[64 * 32];
	float direct_img_2[64 * 32];
	float direct_img_3[64 * 32];
	float direct_img_4[64 * 32];
	float direct_img_5[64 * 32];
	float direct_img_6[64 * 32];
	float direct_img_7[64 * 32];
	float direct_img_8[64 * 32];
	for (int i = 0; i < 64 * 32; i++)
	{
		direct_img_1[i] = direct_img_2[i] = direct_img_3[i] = direct_img_4[i] = direct_img_5[i] = direct_img_6[i] = direct_img_7[i] = direct_img_8[i] = 0;
	}
	float dfx = 0;
	float dfy = 0;
	float abs_dfx = 0;
	float abs_dfy = 0;
	float a1;
	float a2;
	for (int i = 1; i < 63; i++)
	{
		for (int j = 1; j < 31; j++)
		{
			dfx = (aran_img[(i + 1) * 32 + j - 1] + 2 * aran_img[(i + 1) * 32 + j] + aran_img[(i + 1) * 32 + j + 1] - aran_img[(i - 1) * 32 + j - 1] - 2 * aran_img[(i - 1) * 32 + j] - aran_img[(i - 1) * 32 + j + 1]) / 8;
			dfy = (aran_img[(i - 1) * 32 + j + 1] + 2 * aran_img[(i) * 32 + j + 1] + aran_img[(i + 1) * 32 + j + 1] - aran_img[(i - 1) * 32 + j - 1] - 2 * aran_img[(i) * 32 + j - 1] - aran_img[(i + 1) * 32 + j - 1]) / 8;

			abs_dfx = fabs(dfx);
			abs_dfy = fabs(dfy);
			if (abs_dfx < 0.0001 && abs_dfy < 0.0001)
			{
				a1 = 0;
				a2 = 0;
			}
			else
			{
				a1 = fabs(abs_dfx - abs_dfy); // / sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
				float min_df = (abs_dfx < abs_dfy) ? abs_dfx : abs_dfy;
				a2 = sqrt(2.0)*min_df; // /sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
			}

			if (dfx >= 0 && abs_dfy <= abs_dfx)
			{
				direct_img_7[i * 32 + j] += a1;
			}
			else if (dfx < 0 && abs_dfy <= abs_dfx)
			{
				direct_img_3[i * 32 + j] += a1;
			}
			else if (dfy >= 0 && abs_dfy > abs_dfx)
			{
				direct_img_5[i * 32 + j] += a1;
			}
			else if (dfy < 0 && abs_dfy > abs_dfx)
			{
				direct_img_1[i * 32 + j] += a1;
			}

			if (dfx >= 0 && dfy >= 0)
			{
				direct_img_6[i * 32 + j] += a2;
			}
			else if (dfx >= 0 && dfy < 0)
			{
				direct_img_8[i * 32 + j] += a2;
			}
			else if (dfx < 0 && dfy < 0)
			{
				direct_img_2[i * 32 + j] += a2;
			}
			else if (dfx < 0 && dfy >= 0)
			{
				direct_img_4[i * 32 + j] += a2;
			}
		}
	}

	delete[] aran_img;


	float * feature = new float[256];
	int index_gauss = 0;
	int index_feat = 0;
	float feat1 = 0;
	float feat2 = 0;
	float feat3 = 0;
	float feat4 = 0;
	float feat5 = 0;
	float feat6 = 0;
	float feat7 = 0;
	float feat8 = 0;
	for (int i = 3; i < 64; i += 8)
	{
		for (int j = 3; j < 32; j += 8)
		{
			for (int k = i - size_wds / 2; k < i + size_wds / 2; k++)
			{
				for (int l = j - size_wds / 2; l < j + size_wds / 2; l++)
				{
					if (k >= 0 && k < 64 && l >= 0 && l < 32)
					{
						feat1 += gauss_wds[index_gauss] * direct_img_1[k * 32 + l];
						feat2 += gauss_wds[index_gauss] * direct_img_2[k * 32 + l];
						feat3 += gauss_wds[index_gauss] * direct_img_3[k * 32 + l];
						feat4 += gauss_wds[index_gauss] * direct_img_4[k * 32 + l];
						feat5 += gauss_wds[index_gauss] * direct_img_5[k * 32 + l];
						feat6 += gauss_wds[index_gauss] * direct_img_6[k * 32 + l];
						feat7 += gauss_wds[index_gauss] * direct_img_7[k * 32 + l];
						feat8 += gauss_wds[index_gauss] * direct_img_8[k * 32 + l];
					}
					index_gauss++;
				}
			}
			feature[index_feat++] = feat1;
			feature[index_feat++] = feat2;
			feature[index_feat++] = feat3;
			feature[index_feat++] = feat4;
			feature[index_feat++] = feat5;
			feature[index_feat++] = feat6;
			feature[index_feat++] = feat7;
			feature[index_feat++] = feat8;
			feat1 = 0;
			feat2 = 0;
			feat3 = 0;
			feat4 = 0;
			feat5 = 0;
			feat6 = 0;
			feat7 = 0;
			feat8 = 0;
			index_gauss = 0;
		}
	}

	return feature;
}

void FeatureExtract(Mat srcImg, string file_pixel_out, bool useHanning)
{
	if (file_pixel_out == "")
	{
		fprintf(stdout, "Result save path is null!\n");
		return;
	}

	// This is different in VS2017 and in sever187 g++
#ifdef VS2017
	FILE *fp_pixel;
	errno_t err;
	err = fopen_s(&fp_pixel, file_pixel_out.c_str(), "wb+");
	if (err) {
		fprintf(stdout, "Create file %s failed!\n", file_pixel_out.c_str());
		return;
	}
#else
	FILE *fp_pixel = fopen(file_pixel_out.c_str(), "wb+");
	if (!fp_pixel)
	{
		fprintf(stdout, "Create file %s failed!\n", file_pixel_out.c_str());
		return;
	}
#endif

	int windowWidth = 40; //////修改窗长*******set

	vector<Point> midPoints;
	Mat tempSrc = lineNormbyCentroid(srcImg, midPoints);

	int rows = tempSrc.rows;
	int cols = tempSrc.cols;
	int windowHeight = 80;
	int shift = 3;    //修改步长*******

	int index = 0;

	Mat grayImg(windowHeight, windowWidth, CV_8UC1); // 这里存放切出来的帧
	//Mat sobel(windowHeight,windowWidth,CV_32FC1);

	int nSamples = (cols - windowWidth) / 3 + 1; //修改总帧数*********
	int sampPeriod = 1;
	short paramKind = 9;
	short sampSize_pixel = 256 * sizeof(float); //修改特征维数***********

	nSamples = swap32(nSamples);
	sampPeriod = swap32(sampPeriod);
	sampSize_pixel = swap16(sampSize_pixel);
	//sampSize_moment = swap16(sampSize_moment);
	paramKind = swap16(paramKind);

	fwrite(&nSamples, sizeof(int), 1, fp_pixel);
	fwrite(&sampPeriod, sizeof(int), 1, fp_pixel);
	fwrite(&sampSize_pixel, sizeof(short), 1, fp_pixel);
	fwrite(&paramKind, sizeof(short), 1, fp_pixel);
	//**************高斯窗************************//
	int size_wds = 32;
	if (!gauss_win)
	{
		gauss_win = calc_gauss_window(size_wds);
	}
	//_CrtDumpMemoryLeaks();
//***********************************************
	for (int i = 0; i <= cols - windowWidth; i += shift)
	{
		index++;
#ifdef SHOW_IMG
		printf("\nindex:%d\n", index);
#endif
		cv::Rect window = Rect(midPoints[i].x, midPoints[i].y - 40, windowWidth, windowHeight);

		tempSrc(window).copyTo(grayImg);
		//gradient(window).copyTo(sobel);

#ifdef SHOW_IMG
		namedWindow("src1",0);
		imshow("src1",grayImg);//加边后截取的40*80的图
#endif

		if (useHanning)
		{
			HanningFilter(grayImg, grayImg);
#ifdef SHOW_IMG
			//namedWindow("hF",0);
			//imshow("hF",grayImg);
#endif
		}

		float *feature = grad_feature_extract(grayImg, grayImg.cols, grayImg.rows, gauss_win);

		//save feature//
		for (int k = 0; k < 256; k++)
		{
			float tmp = feature[k] * 100;
#ifdef SHOW_IMG
			printf("%.2f\t", tmp);
#endif
			int tmp_int = *(int*)(&tmp);
			tmp_int = swap32(tmp_int);
			fwrite(&tmp_int, sizeof(float), 1, fp_pixel);
		}
#ifdef SHOW_IMG
		waitKey();
#endif
		delete[] feature;
		feature = NULL;
	}

	fclose(fp_pixel);
	delete[]gauss_win;
	gauss_win = NULL;
	//fclose(fp_pixel_txt);
}

void Run(char* file_list_in, char* pixel_file_path_out, bool useHanning)
{

	if (!file_list_in || !pixel_file_path_out)
	{
		fprintf(stdout, "Input path is null!\n");
		return;
	}

	// This is different in VS2017 and in sever187 g++
#ifdef VS2017
	FILE *fp;
	errno_t err;
	err = fopen_s(&fp, file_list_in, "r");
	if (err)
	{
		fprintf(stdout, "Open file %s failed!\n", file_list_in);
		return;
	}
#else
	FILE *fp = fopen(file_list_in, "r");
	if (!fp)
	{
		fprintf(stdout, "Open file %s failed!\n", file_list_in);
		return;
	}
#endif

	string outPath_pixel = pixel_file_path_out;

	char c;

	while (!feof(fp))
	{

		char image_name[1024];
		// This is different in VS2017 and in sever187 g++
#ifdef VS2017
		fscanf_s(fp, "%s", image_name, _countof(image_name));
#else
		fscanf(fp, "%s", image_name);
#endif

		if ((c = fgetc(fp)) == EOF)
		{
			break;
		}

		int len = strlen(image_name);
		if (image_name[len - 1] == '\n' && image_name[len - 2] == '\r')
		{
			image_name[len - 2] = '\0';
		}
		if (image_name[len - 1] == '\n' && image_name[len - 2] != '\r')
		{
			image_name[len - 1] = '\0';
		}

		fprintf(stdout, "Process file %s\n", image_name);
		Mat img = imread(image_name, 0);

		if (img.cols * img.rows == 0)
		{
			fprintf(stdout, "Image read failed!\n", image_name);
			continue;
		}

#ifdef SHOW_IMG
		namedWindow("gray", 0);
		imshow("gray", img);
#endif

		string fileName = image_name;
		string dir;
		string imgName;
		string imgType;
		GetImgInfo(fileName, dir, imgName, imgType);
		string pixel_save_path = outPath_pixel + "/" + imgName + ".htk";


		//*******************二值化******************************
		calcOtsu(img, img.cols, img.rows);

		FeatureExtract(img, pixel_save_path, useHanning);

#ifdef SHOW_IMG
		waitKey();
#endif
	}

	fclose(fp);
}

int main(int argc, char* argv[])
{
	char* list_name = NULL;
	char* pixel_path = NULL;
	//char* moment_path = NULL;
	bool useHanning = true;

	if (argc != 5 && argc != 7)
	{
		fprintf(stdout, "Miss Parameter! .exe -L list -O out_path  [-F useHanning]\n");
		return -1;
	}

	for (int i = 0; i < argc - 1;)
	{
		char* cmd = argv[i];
		string str = cmd;
		if (str == "-L")
		{
			list_name = argv[i + 1];
			i += 2;
			continue;
		}
		if (str == "-O")
		{
			pixel_path = argv[i + 1];
			i += 2;
			continue;
		}
		if (str == "-F")
		{
			int flag = atoi(argv[i + 1]);
			if (flag == 0)
			{
				useHanning = false;
			}
			i += 2;
			continue;
		}
		i++;
	}

	if (!list_name || !pixel_path)
	{
		fprintf(stdout, "Miss Parameter!\n");
		return -1;
	}

	Run(list_name, pixel_path, useHanning);

	return 0;
}
