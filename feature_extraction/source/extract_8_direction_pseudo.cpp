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
#include <fstream>

using namespace std;
using namespace cv;

//#define VS2017
//#define SHOW_IMG

float * gauss_win = NULL;

#define swap16(x)  ( (((x)<<8) & 0xff00) | (((x)>>8) & 0x0ff) )
#define swap32(x)  ( (((x)<<24) & 0xff000000) | (((x)<<8) & 0x0ff0000) |  (((x)>>8) & 0x0ff00) | (((x)>>24) & 0x0ff) )

bool sortByM1(Rect m, Rect n) {
	return m.x < n.x;
}

int calcLineHeight(Mat imgOri, vector<Point> &points) {
	Mat binImg = imgOri.clone();
	cv::threshold(imgOri, binImg, 0, 255, CV_THRESH_OTSU);
	binImg = 255 - binImg;
	Mat tempImg = binImg.clone();
	vector<vector<Point> > contour;
	findContours(tempImg, contour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

	vector<Rect> rectVec_rank;
	rectVec_rank.clear();
	for (int k = 0; k < contour.size(); k++) {
		Rect currRect = boundingRect(contour[k]);
		if (currRect.width > 10 || currRect.height > 10)
			rectVec_rank.push_back(currRect);
	}

	std::sort(rectVec_rank.begin(), rectVec_rank.end(), sortByM1);

	vector<Rect> rectVec;
	rectVec.clear();
	for (int k = 0; k < rectVec_rank.size(); k++) {
		Rect currRect = rectVec_rank[k];
		if (currRect.width > 10 || currRect.height > 10){
			if (rectVec.empty())
				rectVec.push_back(currRect);
			else {
				Rect lastRect = rectVec[rectVec.size() - 1];
				int x1_start = lastRect.x;
				int x1_end = lastRect.x + lastRect.width;
				int x2_start = currRect.x;
				int x2_end = currRect.x + currRect.width;

				int startMax = x1_start > x2_start ? x1_start : x2_start;
				int endMin = x1_end < x2_end ? x1_end : x2_end;

				if (startMax - endMin < 5) {
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
				else{
					rectVec.push_back(currRect);
				}
			}
		}
	}

	points.clear();
	int xPre = 0;
	int yPre = 0;
	if (rectVec.empty())
		yPre = imgOri.rows / 2;
	else
		yPre = rectVec[0].y + rectVec[0].height / 2;

	for (int i = 0; i < rectVec.size(); i++) {
		int xCore = 0;
		int yCore = 0;
		int foregroundPixelCnt = 0;
		for (int m = rectVec[i].y; m < rectVec[i].y + rectVec[i].height; m++)
			for (int n = rectVec[i].x; n < rectVec[i].x + rectVec[i].width; n++)
				if (binImg.at<uchar>(m, n) == 255) {
					foregroundPixelCnt++;
					xCore += n;
					yCore += m;
				}
		if (foregroundPixelCnt <= 0) {
			foregroundPixelCnt = 1;
			printf("foregroundPixelCnt <= 0 \n");
		}
		xCore /= foregroundPixelCnt;
		yCore /= foregroundPixelCnt;

		float slope = (float)(yCore - yPre) / (float)(rectVec[i].x - xPre);
		for (int j = xPre; j < rectVec[i].x; j++)
			points.push_back(Point(j, yPre + (j - xPre)*slope));
		for (int j = rectVec[i].x; j < rectVec[i].x + rectVec[i].width; j++)
			points.push_back(Point(j, yCore));

		xPre = rectVec[i].x + rectVec[i].width;
		yPre = yCore;
	}
	for (int i = xPre; i < imgOri.cols; i++)
		points.push_back(Point(i, yPre));

	int sumArea = 0;
	int sumWidth = 0;
	for (int i = 0; i < rectVec.size(); i++) {
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
	namedWindow("Otsu_after_norm", 0);
	imshow("Otsu_after_norm", normImg);
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

void HanningFilter_1D(Mat srcImg, Mat dstImg) {
	int rows = srcImg.rows;
	int cols = srcImg.cols;
	//use int rather than size_t for Mat.rows maybe -1
	Mat LineNormedImg(rows, cols, CV_8UC1);
	for (int j = 0; j < cols; j++) {
		float w = 0.5 * (1 - cos(2 * CV_PI * (j / float(cols))));
		for (int i = 0; i < rows; i++)
			LineNormedImg.data[i * cols + j] = 255 - uchar((255 - srcImg.data[i * cols + j]) * w);
	}
	LineNormedImg.copyTo(dstImg);
}

void HanningFilter_2D(Mat srcImg, Mat dstImg) {
	int rows = srcImg.rows;
	int cols = srcImg.cols;
	//use int rather than size_t for Mat.rows maybe -1
	Mat LineNormedImg(rows, cols, CV_8UC1);
	for (int j = 0; j < cols; j++) {
		float w_j = 0.5 * (1 - cos(2 * CV_PI * (j / float(cols))));
		for (int i = 0; i < rows; i++) {
			float w_i = 0.5 * (1 - cos(2 * CV_PI * (i / float(cols))));
			LineNormedImg.at<uchar>(i, j) = 255 - uchar((255 - srcImg.at<uchar>(i, j)) * w_j *w_i);
		}
	}
	LineNormedImg.copyTo(dstImg);
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
		for (int j = -N / 2; j < N / 2; j++)
			gauss_mask[(i + N / 2)*N + (j + N / 2)] = exp(-(i * i + j * j) / (2 * sigma * sigma)) / (2 * 3.1415926 * sigma * sigma);
	return gauss_mask;
}

float * grad_feature_extract_1D(Mat img)
{
	int gauss_win_size = 32;
	assert(img.rows == 64 && img.cols == 32);
	int rows = 64, cols = 32, tol = rows * cols;

	unsigned char* aran_img = new unsigned char[rows * cols];
	for (int i = 0; i < rows; i++)
		for (int j = 0; j < cols; j++)
			aran_img[i * cols + j] = img.ptr(i)[j];

	float* direct_img_1 = new float[tol];
	float* direct_img_2 = new float[tol];
	float* direct_img_3 = new float[tol];
	float* direct_img_4 = new float[tol];
	float* direct_img_5 = new float[tol];
	float* direct_img_6 = new float[tol];
	float* direct_img_7 = new float[tol];
	float* direct_img_8 = new float[tol];
	for (int i = 0; i < tol; i++)
		direct_img_1[i] = direct_img_2[i] = direct_img_3[i] = direct_img_4[i] = direct_img_5[i] = direct_img_6[i] = direct_img_7[i] = direct_img_8[i] = 0;

	float dfx = 0;
	float dfy = 0;
	float abs_dfx = 0;
	float abs_dfy = 0;
	float a1;
	float a2;
	for (int i = 1; i < rows - 1; i++)
		for (int j = 1; j < cols - 1; j++) {
			dfx = (aran_img[(i + 1) * cols + j - 1] + 2 * aran_img[(i + 1) * cols + j] + aran_img[(i + 1) * cols + j + 1] - aran_img[(i - 1) * cols + j - 1] - 2 * aran_img[(i - 1) * cols + j] - aran_img[(i - 1) * cols + j + 1]) / 8;
			dfy = (aran_img[(i - 1) * cols + j + 1] + 2 * aran_img[(i)* cols + j + 1] + aran_img[(i + 1) * cols + j + 1] - aran_img[(i - 1) * cols + j - 1] - 2 * aran_img[(i)* cols + j - 1] - aran_img[(i + 1) * cols + j - 1]) / 8;

			abs_dfx = fabs(dfx);
			abs_dfy = fabs(dfy);
			if (abs_dfx < 0.0001 && abs_dfy < 0.0001) {
				a1 = 0;
				a2 = 0;
			}
			else {
				a1 = fabs(abs_dfx - abs_dfy); // / sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
				float min_df = (abs_dfx < abs_dfy) ? abs_dfx : abs_dfy;
				a2 = sqrt(2.0)*min_df; // /sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
			}

			if (dfx >= 0 && abs_dfy <= abs_dfx)
				direct_img_7[i * cols + j] += a1;
			else if (dfx < 0 && abs_dfy <= abs_dfx)
				direct_img_3[i * cols + j] += a1;
			else if (dfy >= 0 && abs_dfy > abs_dfx)
				direct_img_5[i * cols + j] += a1;
			else if (dfy < 0 && abs_dfy > abs_dfx)
				direct_img_1[i * cols + j] += a1;

			if (dfx >= 0 && dfy >= 0)
				direct_img_6[i * cols + j] += a2;
			else if (dfx >= 0 && dfy < 0)
				direct_img_8[i * cols + j] += a2;
			else if (dfx < 0 && dfy < 0)
				direct_img_2[i * cols + j] += a2;
			else if (dfx < 0 && dfy >= 0)
				direct_img_4[i * cols + j] += a2;
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
	for (int i = 3; i < rows; i += 8)
	{
		for (int j = 3; j < cols; j += 8)
		{
			for (int k = i - gauss_win_size / 2; k < i + gauss_win_size / 2; k++)
			{
				for (int l = j - gauss_win_size / 2; l < j + gauss_win_size / 2; l++)
				{
					if (k >= 0 && k < rows && l >= 0 && l < cols)
					{
						feat1 += gauss_win[index_gauss] * direct_img_1[k * cols + l];
						feat2 += gauss_win[index_gauss] * direct_img_2[k * cols + l];
						feat3 += gauss_win[index_gauss] * direct_img_3[k * cols + l];
						feat4 += gauss_win[index_gauss] * direct_img_4[k * cols + l];
						feat5 += gauss_win[index_gauss] * direct_img_5[k * cols + l];
						feat6 += gauss_win[index_gauss] * direct_img_6[k * cols + l];
						feat7 += gauss_win[index_gauss] * direct_img_7[k * cols + l];
						feat8 += gauss_win[index_gauss] * direct_img_8[k * cols + l];
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
	delete[] direct_img_1; // 释放内存
	delete[] direct_img_2;
	delete[] direct_img_3;
	delete[] direct_img_4;
	delete[] direct_img_5;
	delete[] direct_img_6;
	delete[] direct_img_7;
	delete[] direct_img_8;

	return feature;
}

float * grad_feature_extract_2D(Mat img)
{
	int gauss_win_size = 16;
	assert(img.rows == 32 && img.cols == 32);
	int rows = 32, cols = 32, tol = rows * cols;

	unsigned char* aran_img = new unsigned char[rows * cols];
	for (int i = 0; i < rows; i++)
		for (int j = 0; j < cols; j++)
			aran_img[i * cols + j] = img.ptr(i)[j];

	float* direct_img_1 = new float[tol];
	float* direct_img_2 = new float[tol];
	float* direct_img_3 = new float[tol];
	float* direct_img_4 = new float[tol];
	float* direct_img_5 = new float[tol];
	float* direct_img_6 = new float[tol];
	float* direct_img_7 = new float[tol];
	float* direct_img_8 = new float[tol];
	for (int i = 0; i < tol; i++)
		direct_img_1[i] = direct_img_2[i] = direct_img_3[i] = direct_img_4[i] = direct_img_5[i] = direct_img_6[i] = direct_img_7[i] = direct_img_8[i] = 0;

	float dfx = 0;
	float dfy = 0;
	float abs_dfx = 0;
	float abs_dfy = 0;
	float a1;
	float a2;
	for (int i = 1; i < rows - 1; i++)
		for (int j = 1; j < cols - 1; j++) {
			dfx = (aran_img[(i + 1) * cols + j - 1] + 2 * aran_img[(i + 1) * cols + j] + aran_img[(i + 1) * cols + j + 1] - aran_img[(i - 1) * cols + j - 1] - 2 * aran_img[(i - 1) * cols + j] - aran_img[(i - 1) * cols + j + 1]) / 8;
			dfy = (aran_img[(i - 1) * cols + j + 1] + 2 * aran_img[(i) * cols + j + 1] + aran_img[(i + 1) * cols + j + 1] - aran_img[(i - 1) * cols + j - 1] - 2 * aran_img[(i) * cols + j - 1] - aran_img[(i + 1) * cols + j - 1]) / 8;

			abs_dfx = fabs(dfx);
			abs_dfy = fabs(dfy);
			if (abs_dfx < 0.0001 && abs_dfy < 0.0001) {
				a1 = 0;
				a2 = 0;
			}
			else {
				a1 = fabs(abs_dfx - abs_dfy); // / sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
				float min_df = (abs_dfx < abs_dfy) ? abs_dfx : abs_dfy;
				a2 = sqrt(2.0)*min_df; // /sqrt(abs_dfx*abs_dfx + abs_dfy*abs_dfy);
			}

			if (dfx >= 0 && abs_dfy <= abs_dfx)
				direct_img_7[i * cols + j] += a1;
			else if (dfx < 0 && abs_dfy <= abs_dfx)
				direct_img_3[i * cols + j] += a1;
			else if (dfy >= 0 && abs_dfy > abs_dfx)
				direct_img_5[i * cols + j] += a1;
			else if (dfy < 0 && abs_dfy > abs_dfx)
				direct_img_1[i * cols + j] += a1;

			if (dfx >= 0 && dfy >= 0)
				direct_img_6[i * cols + j] += a2;
			else if (dfx >= 0 && dfy < 0)
				direct_img_8[i * cols + j] += a2;
			else if (dfx < 0 && dfy < 0)
				direct_img_2[i * cols + j] += a2;
			else if (dfx < 0 && dfy >= 0)
				direct_img_4[i * cols + j] += a2;
		}

	delete[] aran_img;


	float * feature = new float[128];
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
	for (int i = 3; i < rows; i += 8)
	{
		for (int j = 3; j < cols; j += 8)
		{
			for (int k = i - gauss_win_size / 2; k < i + gauss_win_size / 2; k++)
			{
				for (int l = j - gauss_win_size / 2; l < j + gauss_win_size / 2; l++)
				{
					if (k >= 0 && k < rows && l >= 0 && l < cols)
					{
						feat1 += gauss_win[index_gauss] * direct_img_1[k * cols + l];
						feat2 += gauss_win[index_gauss] * direct_img_2[k * cols + l];
						feat3 += gauss_win[index_gauss] * direct_img_3[k * cols + l];
						feat4 += gauss_win[index_gauss] * direct_img_4[k * cols + l];
						feat5 += gauss_win[index_gauss] * direct_img_5[k * cols + l];
						feat6 += gauss_win[index_gauss] * direct_img_6[k * cols + l];
						feat7 += gauss_win[index_gauss] * direct_img_7[k * cols + l];
						feat8 += gauss_win[index_gauss] * direct_img_8[k * cols + l];
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
	delete[] direct_img_1; // 释放内存
	delete[] direct_img_2;
	delete[] direct_img_3;
	delete[] direct_img_4;
	delete[] direct_img_5;
	delete[] direct_img_6;
	delete[] direct_img_7;
	delete[] direct_img_8;

	return feature;
}

void FeatureExtract(Mat srcImg, string file_pixel_out, int shift_right, int shift_down, bool useHanning)
{
	if (file_pixel_out == "") {
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

	vector<Point> midPoints;
	Mat LineNormedImg = lineNormbyCentroid(srcImg, midPoints);
	int rows = LineNormedImg.rows,
		cols = LineNormedImg.cols,
		frameWidth = 40,  // 水平切帧的帧宽
		frameHeight = 80, // 水平切帧的帧高
		blockWidth = 40,  // 竖直切帧的帧宽
		blockHeight = 40, // 竖直切帧的帧高
		frameWidth_resized = 32,  // 水平切帧resize后的帧宽
		frameHeight_resized = 64, // 水平切帧resize后的帧高
		blockWidth_resized = 32,  // 竖直切帧resize后的帧宽
		blockHeight_resized = 32; // 竖直切帧resize后的帧高

#ifdef SHOW_IMG
	namedWindow("LineNormed img", 0);
	imshow("LineNormed img", LineNormedImg);
	printf("Pic after LineNorm rows: %d, cols: %d\n", LineNormedImg.rows, LineNormedImg.cols);
#endif

	Mat frame(frameHeight, frameWidth, CV_8UC1); // 这里存放水平切出来的帧
	Mat block(blockHeight, blockWidth, CV_8UC1); // 这里存放每一水平帧中切出来的竖直帧
	Mat frame_resized(frameHeight_resized, frameWidth_resized, CV_8UC1); // 水平帧大小调整为32*64
	Mat block_resized(blockHeight_resized, blockWidth_resized, CV_8UC1); // 竖直帧大小调整为32*32

	int nSamples = (shift_down == -1) ? ((cols - frameWidth) / shift_right + 1) : (((cols - frameWidth) / shift_right + 1)*((frameHeight - blockHeight) / shift_down + 1));
	int sampPeriod = 1;
	short paramKind = 9;
	short sampSize_pixel = (shift_down == -1) ? (256 * sizeof(float)) : (128 * sizeof(float)); //存放特征维数

	nSamples = swap32(nSamples);
	sampPeriod = swap32(sampPeriod);
	sampSize_pixel = swap16(sampSize_pixel);
	paramKind = swap16(paramKind);

	fwrite(&nSamples, sizeof(int), 1, fp_pixel);
	fwrite(&sampPeriod, sizeof(int), 1, fp_pixel);
	fwrite(&sampSize_pixel, sizeof(short), 1, fp_pixel);
	fwrite(&paramKind, sizeof(short), 1, fp_pixel);

	int size_wds = (shift_down == -1) ? 32 : 16;
	if (!gauss_win)
		gauss_win = calc_gauss_window(size_wds);

	int frame_index = 0;
	int block_index = 0;
	for (int i = 0; i <= cols - frameWidth; i += shift_right)
	{
		frame_index++;
		cv::Rect frame_window = Rect(midPoints[i].x, midPoints[i].y - 40, frameWidth, frameHeight);
		LineNormedImg(frame_window).copyTo(frame);
		resize(frame, frame_resized, Size(frameWidth_resized, frameHeight_resized));
#ifdef SHOW_IMG
		printf("\nframe_index:%d\n", frame_index);
		namedWindow("frame", 0);
		imshow("frame", frame);//加边后截取的40*80的图
		printf("This frame rows: %d, cols: %d\n", frame.rows, frame.cols);
#endif

		float *feature;
		if (shift_down == -1) {
			if (useHanning)
				HanningFilter_1D(frame_resized, frame_resized);
#ifdef SHOW_IMG
			namedWindow("resized_frame_64*32_with_hanning", 0);
			imshow("resized_frame_64*32_with_hanning", frame_resized);
#endif
			feature = grad_feature_extract_1D(frame_resized);
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
			delete[] feature;
			feature = NULL;
#ifdef SHOW_IMG
			waitKey();
#endif
		}
		else {
			for (int j = 0; j <= frameHeight - blockHeight; j += shift_down) {
				block_index++;
				cv::Rect block_window = Rect(0, j, blockWidth, blockHeight);
				frame(block_window).copyTo(block);
				resize(block, block_resized, Size(blockWidth_resized, blockHeight_resized));

				if (useHanning)
					HanningFilter_2D(block_resized, block_resized);
#ifdef SHOW_IMG
				printf("\nblock_index:%d\n", block_index);
				namedWindow("resized_block_32*32_with_hanning", 0);
				imshow("resized_block_32*32_with_hanning", block_resized);
#endif
				feature = grad_feature_extract_2D(block_resized);

				//save feature//
				for (int k = 0; k < 128; k++)
				{
					float tmp = feature[k] * 100;
#ifdef SHOW_IMG
					printf("%.2f\t", tmp);
#endif
					int tmp_int = *(int*)(&tmp);
					tmp_int = swap32(tmp_int);
					fwrite(&tmp_int, sizeof(float), 1, fp_pixel);
				}
				delete[] feature;
				feature = NULL;
#ifdef SHOW_IMG
				waitKey();
#endif
			}
		}
	}

	fclose(fp_pixel);
	delete[] gauss_win;
	gauss_win = NULL;
}

int main(int argc, char* argv[])
{
	char* data_list = NULL;
	char* htk_path = NULL;
	int shift_right = 5; //default value 5
	int shift_down = 5; //default value 5
	bool useHanning = true;

	if (argc != 5 && argc != 6)
	{
		fprintf(stdout, "Usage: %s <data-list> <out-dir> <shift-right> <shift-down> <useHanning(bool)==1>\n", argv[0]);
		fprintf(stdout, " e.g.: %s data/img_list.txt data/htk_dir 3 5 1\n", argv[0]);
		fprintf(stdout, "    Which means read img from img_list.txt(each line store one img save path), \n");
		fprintf(stdout, "  frame the pic rightward every 3 pixels, re-frame each frame downward every 5 pixels, \n");
		fprintf(stdout, "  and use hanning filter within each frame, output feature extracted from them to [htk_dir].\n");
		fprintf(stdout, "<data-list>      --each line is one img save path\n");
		fprintf(stdout, "<out-dir>        --the diretory to save htk files of every data\n");
		fprintf(stdout, "<shift-right>    --step size scan rightward\n");
		fprintf(stdout, "<shift-down>     --step size scan downward, set -1 means only scan rightward (1D-mod)\n");
		fprintf(stdout, "<useHanning>     --set 0 means don't use hanning filter; default 1;\n");
		fprintf(stdout, "                 --if in 1D-mod, only filter horizontally;\n");
		fprintf(stdout, "                 --if in 2D-mod, filter both horizontally and vertically;\n");
		return -1;
	}

	data_list = argv[1];
	htk_path = argv[2];
	shift_right = atoi(argv[3]);
	shift_down = atoi(argv[4]);
	if ((argc == 6) && (atoi(argv[5]) == 0))
		useHanning = false;

	if (!data_list || !htk_path || !shift_right || !shift_down) {
		fprintf(stdout, "Parameter error! Note that 0 pixel shift is not allowed.\n");
		return -1;
	}

	ifstream file_list_txt;
	file_list_txt.open(data_list, ios::in);
	if (!file_list_txt) {
		fprintf(stdout, "can not open %s!\n", data_list);
		return -1;
	}

	string image_name;
	string outPath_pixel = htk_path;

	while (std::getline(file_list_txt, image_name)) {

		fprintf(stdout, "Processing file %s\n", image_name.c_str());
		Mat img = imread(image_name, 0);
		if (img.cols * img.rows == 0) {
			fprintf(stdout, "Image %s read failed!\n", image_name.c_str());
			continue;
		}

#ifdef SHOW_IMG
		//namedWindow("raw_img, gray", 0);
		//imshow("raw_img, gray", img);
		printf("Current pic: %s. Raw pic rows: %d, cols: %d\n", image_name, img.rows, img.cols);
#endif

		//*******************二值化******************************
		threshold(img, img, 0, 255, CV_THRESH_OTSU);

#ifdef SHOW_IMG
		namedWindow("Otsu_img, binary", 0);
		imshow("Otsu_img, binary", img);
#endif

		string dir;
		string imgName;
		string imgType;
		GetImgInfo(image_name, dir, imgName, imgType);
		string pixel_save_path = outPath_pixel + "/" + imgName + ".htk";
		FeatureExtract(img, pixel_save_path, shift_right, shift_down, useHanning);

#ifdef SHOW_IMG
		waitKey();
#endif
	}
	file_list_txt.close();

	return 0;
}
