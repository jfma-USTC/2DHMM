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
#include <fstream>

using namespace std;
using namespace cv;

//#define SHOW_DETAIL
//#define VS2017
#define GAUSS_WIN_SIZE 16
#define FEAT_DIM 128

float * gauss_win = NULL;

#define swap16(x)  ( (((x)<<8) & 0xff00) | (((x)>>8) & 0x0ff) )
#define swap32(x)  ( (((x)<<24) & 0xff000000) | (((x)<<8) & 0x0ff0000) |  (((x)>>8) & 0x0ff00) | (((x)>>24) & 0x0ff) )

void HanningFilter_2D(Mat &srcImg, Mat &dstImg)
{
	int rows = srcImg.rows;
	int cols = srcImg.cols;

	Mat ResizedImg(rows, cols, CV_8UC1);

	for (int j = 0; j < cols; j++) {
		float w_j = 0.5 * (1 - cos(2 * CV_PI * (j / float(cols))));
		for (int i = 0; i < rows; i++) {
			float w_i = 0.5 * (1 - cos(2 * CV_PI * (i / float(cols))));
			ResizedImg.at<uchar>(i, j) = 255 - int((255 - srcImg.at<uchar>(i, j)) * w_j *w_i);
		}
	}
	ResizedImg.copyTo(dstImg);
}

void GetImgInfo(string srcImgPath, string &dirPath, string &imgName, string &imgType)
{
	int nPos1 = srcImgPath.find_last_of("\\");
	int nPos2 = srcImgPath.find_last_of("/");
	int nPos = nPos1;

	if (nPos1 < nPos2)
		nPos = nPos2;

	dirPath = srcImgPath.substr(0, nPos);
	string name = srcImgPath.substr(nPos + 1, srcImgPath.length() - 1);
	nPos = name.find_last_of(".");
	imgName = name.substr(0, nPos);
	imgType = name.substr(nPos + 1, name.length() - 1);
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

float * grad_feature_extract_2D(Mat img)
{
	int gauss_win_size = GAUSS_WIN_SIZE;
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

void FeatureExtract(Mat srcImg, string img_name, string file_pixel_out, fstream &file_block_num, int shift_right, int shift_down, bool useHanning)
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
	// -------- 对二值化图像进行尺度归一（保持长宽不变高度变为‘Reszie_Rows’pixels）-------- //
	int Reszie_Rows = 80;	//--------修改归一化后图像的高度--------//
	Mat ResizedImg(Reszie_Rows, Reszie_Rows * srcImg.cols / srcImg.rows, CV_8UC1);
	cv::resize(srcImg, ResizedImg, ResizedImg.size());

	int windowCols = 40;	//--------修改窗宽--------//
	int windowRows = 40;	//--------修改窗高--------//
	int windowCols_resized = 32;	//--------修改窗宽--------//
	int windowRows_resized = 32;	//--------修改窗高--------//

	cv::copyMakeBorder(ResizedImg, ResizedImg, windowRows / 2, windowRows / 2, windowCols / 2, windowCols / 2, BORDER_CONSTANT, cv::Scalar(255));
	
	int cols = ResizedImg.cols; //归一化之后图像的列数（宽）
	int rows = ResizedImg.rows; //归一化之后图像的行数（高）
	int Sample_Num_Cols = (cols - windowCols) / shift_right + 1; //水平方向上应该被切出来的帧数（有几列帧）
	int Sample_Num_Rows = (rows - windowRows) / shift_down + 1; //竖直方向上应该被切出来的帧数（有几行帧）	
	int nSamples = Sample_Num_Cols * Sample_Num_Rows; //总帧数
	int sampPeriod = 1;
	short paramKind = 9;
	short sampSize_pixel = FEAT_DIM * sizeof(float); //修改特征维数--------****

	file_block_num << img_name << " " << Sample_Num_Rows << " " << Sample_Num_Cols << " " << nSamples << endl;

	nSamples = swap32(nSamples);
	sampPeriod = swap32(sampPeriod);
	paramKind = swap16(paramKind);
	sampSize_pixel = swap16(sampSize_pixel);
	//sampSize_moment = swap16(sampSize_moment);
	fwrite(&nSamples, sizeof(int), 1, fp_pixel);
	fwrite(&sampPeriod, sizeof(int), 1, fp_pixel);
	fwrite(&sampSize_pixel, sizeof(short), 1, fp_pixel);
	fwrite(&paramKind, sizeof(short), 1, fp_pixel);

	if (!gauss_win)
		gauss_win = calc_gauss_window(GAUSS_WIN_SIZE);

	Mat block(windowRows, windowCols, CV_8UC1); // 用来存储每一个block的临时矩阵
	Mat resized_block(windowRows_resized, windowCols_resized, CV_8UC1); // 用来存储每一个block的临时矩阵
	
#ifdef SHOW_DETAIL
	int index = 0;
	printf("img_after_resize: width: %d  height: %d\n", ResizedImg.cols, ResizedImg.rows);
	printf("Sample_Num_Cols : %d , Sample_Num_Rows : %d\n", Sample_Num_Cols, Sample_Num_Rows);
	namedWindow("after_resize", 0);
	imshow("after_resize", ResizedImg);
#endif
	for (int j = 0; j < Sample_Num_Rows; j++)
	{
		for (int i = 0; i < Sample_Num_Cols; i++)
		{
			// 定义 Rect(int _x,int _y,int _width,int _height);
			cv::Rect window(i*shift_right, j*shift_down, windowCols, windowRows);
			// 从ResizedImg中切割window指定的矩形图像，并赋值给block,超出范围时用纯白像素填充（255）
			//block = ImageCropPadding(ResizedImg, window);
			ResizedImg(window).copyTo(block);
			cv::resize(block, resized_block, resized_block.size());

#ifdef SHOW_DETAIL
			printf("index:%d\n", index);
			printf("img_after_resize: width: %d  height: %d\n", ResizedImg.cols, ResizedImg.rows);
			printf("shfit_r: %d ,shift_d: %d \n", i*shift_right, j*shift_down);
#endif
			float *feature = NULL;
			if (useHanning)
				HanningFilter_2D(resized_block, resized_block);
			feature = grad_feature_extract_2D(resized_block);
#ifdef SHOW_DETAIL
			namedWindow("block_after_hanning", 0);
			imshow("block_after_hanning", resized_block);
#endif
			
			//save feature//
			for (int k = 0; k < FEAT_DIM; k++)
			{
				float tmp = feature[k] * 100;
#ifdef SHOW_DETAIL
				printf("%.2f\t", tmp);
#endif
				int tmp_int = *(int*)(&tmp);
				tmp_int = swap32(tmp_int);
				fwrite(&tmp_int, sizeof(float), 1, fp_pixel);
			}
#ifdef SHOW_DETAIL
			waitKey();
			index++;
			printf("\n");
#endif
			delete[] feature;
			feature = NULL;
		}
	}
	block.release();
	resized_block.release();
	ResizedImg.release();
	fclose(fp_pixel);
	delete[] gauss_win;
	gauss_win = NULL;
}

int main(int argc, char* argv[])
{
	char* data_list = NULL;
	char* htk_path = NULL;
	char* block_num_info = NULL;
	int shift_right = 8; //default value 8
	int shift_down = 8; //default value 8
	bool useHanning = true;

	if (argc != 6 && argc != 7)
	{
		fprintf(stdout, "Usage: %s <data-list> <out-dir> <out-block-num> <shift-right> <shift-down> <useHanning(bool)==1>\n", argv[0]);
		fprintf(stdout, " e.g.: %s data/img_list.txt data/htk_dir /disk2/jfma/2DHMM/block 8 8 1\n", argv[0]);
		fprintf(stdout, "    Which means read img from img_list.txt(each line store one img save path), \n");
		fprintf(stdout, "  frame every pic 8 pixels rightward, 8 pixels downward; * rightward perior * \n");
		fprintf(stdout, "  and use hanning filter within each frame, output feature extracted from them to [htk_dir].\n");
		fprintf(stdout, "<data-list>      --each line is one img save path\n");
		fprintf(stdout, "<out-dir>        --the diretory to save htk files of every data\n");
		fprintf(stdout, "<out-block-num>  --the file to save block information of every data\n");
		fprintf(stdout, "                   Format each line: bmp_name blocks_rows blocks_cols tol_num\n");
		fprintf(stdout, "<shift-right>    --step size scan rightward, suggest 8 in 2D situation\n");
		fprintf(stdout, "<shift-down>     --step size scan downward, suggest 8 in 2D situation\n");
		fprintf(stdout, "<useHanning>     --set 0 means don't use hanning filter; default 1;\n");
		fprintf(stdout, "                 --if in 2D-mod, filter both horizontally and vertically;\n");
		return -1;
	}

	data_list = argv[1];
	htk_path = argv[2];
	block_num_info = argv[3];
	shift_right = atoi(argv[4]);
	shift_down = atoi(argv[5]);
	if ((argc == 6) && (atoi(argv[6]) == 0))
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

	fstream block_num_info_ofstream;
	block_num_info_ofstream.open(block_num_info, ios::out);
	if (!block_num_info_ofstream) {
		fprintf(stdout, "can not open %s!\n", block_num_info);
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

#ifdef SHOW_DETAIL
		printf("Current pic: %s. Raw pic rows: %d, cols: %d\n", image_name, img.rows, img.cols);
#endif
		// turn the pic to binary in Otsu way
		threshold(img, img, 0, 255, CV_THRESH_OTSU);

#ifdef SHOW_DETAIL
		namedWindow("Otsu_img, binary", 0);
		imshow("Otsu_img, binary", img);
#endif

		string dir;
		string imgName;
		string imgType;
		GetImgInfo(image_name, dir, imgName, imgType);
		string pixel_save_path = outPath_pixel + "/" + imgName + ".htk";
		FeatureExtract(img, imgName, pixel_save_path, block_num_info_ofstream, shift_right, shift_down, useHanning);

#ifdef SHOW_DETAIL
		waitKey();
#endif
	}
	file_list_txt.close();
	block_num_info_ofstream.close();

	return 0;
}
