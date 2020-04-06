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

int main(int argc, char* argv[])
{
	char* data_list = NULL;
	char* block_num_info = NULL;
	int shift_right = 8; //default value 8
	int shift_down = 8; //default value 8

	if (argc != 5)
	{
		fprintf(stdout, "Usage: %s <data-list> <out-block-num> <shift-right> <shift-down>\n", argv[0]);
		fprintf(stdout, " e.g.: %s data/img_list.txt /disk2/jfma/2DHMM/block 8 8\n", argv[0]);
		fprintf(stdout, "    Which means read img from img_list.txt(each line store one img save path), \n");
		fprintf(stdout, "  frame every pic 8 pixels rightward, 8 pixels downward; * rightward perior * \n");
		fprintf(stdout, "  and store the block info to <out-block-num>.\n");
		fprintf(stdout, "<data-list>      --each line is one img save path\n");
		fprintf(stdout, "<out-block-num>  --the file to save block information of every data\n");
		fprintf(stdout, "                   Format each line: bmp_name blocks_rows blocks_cols tol_num\n");
		fprintf(stdout, "<shift-right>    --step size scan rightward, suggest 8 in 2D situation\n");
		fprintf(stdout, "<shift-down>     --step size scan downward, suggest 8 in 2D situation\n");
		fprintf(stdout, "                 --if in 2D-mod, filter both horizontally and vertically;\n");
		return -1;
	}

	data_list = argv[1];
	block_num_info = argv[2];
	shift_right = atoi(argv[3]);
	shift_down = atoi(argv[4]);

	if (!data_list || !shift_right || !shift_down) {
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

	while (std::getline(file_list_txt, image_name)) {

		fprintf(stdout, "Processing file %s\n", image_name.c_str());
		Mat img = imread(image_name, 0);
		if (img.cols * img.rows == 0) {
			fprintf(stdout, "Image %s read failed!\n", image_name.c_str());
			continue;
		}

		string dir;
		string imgName;
		string imgType;
		GetImgInfo(image_name, dir, imgName, imgType);

		// -------- 对二值化图像进行尺度归一（保持长宽不变高度变为‘Reszie_Rows’pixels）-------- //
		int Reszie_Rows = 80;	//--------修改归一化后图像的高度--------//
		Mat ResizedImg(Reszie_Rows, Reszie_Rows * img.cols / img.rows, CV_8UC1);
		cv::resize(img, ResizedImg, ResizedImg.size());

		int windowCols = 40;	//--------修改窗宽--------//
		int windowRows = 40;	//--------修改窗高--------//

		cv::copyMakeBorder(ResizedImg, ResizedImg, windowRows / 2, windowRows / 2, windowCols / 2, windowCols / 2, BORDER_CONSTANT, cv::Scalar(255));

		int cols = ResizedImg.cols; //归一化之后图像的列数（宽）
		int rows = ResizedImg.rows; //归一化之后图像的行数（高）
		int Sample_Num_Cols = (cols - windowCols) / shift_right + 1; //水平方向上应该被切出来的帧数（有几列帧）
		int Sample_Num_Rows = (rows - windowRows) / shift_down + 1; //竖直方向上应该被切出来的帧数（有几行帧）	
		int nSamples = Sample_Num_Cols * Sample_Num_Rows; //总帧数

		block_num_info_ofstream << imgName << " " << Sample_Num_Rows << " " << Sample_Num_Cols << " " << nSamples << endl;
		
		img.release();
		ResizedImg.release();
	}
	file_list_txt.close();
	block_num_info_ofstream.close();

	return 0;
}
