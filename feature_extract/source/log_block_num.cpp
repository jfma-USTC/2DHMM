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

//#define SHOW_DETAIL
//#define VS2017


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

void Run(char* file_list_in, char* out_file)
{
	if (!file_list_in || !out_file)
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
	FILE *fp = fopen(file_list_in,"r");
	if (!fp)
	{
		fprintf(stdout, "Open file %s failed!\n",file_list_in);
		return;
	}
#endif

#ifdef VS2017
	FILE *fp_out;
	errno_t err2;
	err2 = fopen_s(&fp_out, out_file, "wb+");
	if (err2){
		fprintf(stdout, "Create file %s failed!\n", out_file);
		return;
	}
#else
	FILE *fp_out = fopen(out_file,"wb+");
	if (!fp_out)
	{
		fprintf(stdout, "Create file %s failed!\n",out_file);
		return;
	}
#endif

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

		fprintf(stdout, "Processing file %s :", image_name);
		Mat img = imread(image_name, 0);

		if (img.cols * img.rows == 0)
		{
			fprintf(stdout, "Image %s read failed!\n", image_name);
			continue;
		}

		string fileName = image_name;
		string dir;
		string imgName;
		string imgType;
		GetImgInfo(fileName, dir, imgName, imgType);
		
		int rows = 60; //归一化之后图像的行数（高）
		int cols = rows * img.cols / img.rows; //归一化之后图像的列数（宽）
		int windowCols = 20;	//--------修改窗宽--------//
		int windowRows = 20;	//--------修改窗高--------//
		int shift_right = 5;	//--------修改左右滑窗步长--------//
		int shift_down = 5;	//--------修改上下滑窗步长--------//
		int Sample_Num_Cols = (cols - windowCols) / shift_right; //帧数的宽度（有几列帧）
		int Sample_Num_Rows = (rows - windowRows) / shift_down; //帧数的高度（有几行帧）
		if ((cols - windowCols) <= 0)
			Sample_Num_Cols = 1;
		else if (((float)(cols - windowCols) / (float)shift_right - Sample_Num_Cols) < 0.001) //当刚好(cols - windowCols) / shift_right为整数时
			Sample_Num_Cols ++ ;
		else
			Sample_Num_Cols += 2;
		if ((rows - windowRows) <= 0)
			Sample_Num_Rows = 1;
		else if (((float)(rows - windowRows) / (float)shift_down - Sample_Num_Rows) < 0.001) //当刚好(rows - windowCols) / shift_down为整数时
			Sample_Num_Rows ++ ;
		else
			Sample_Num_Rows += 2;
		int nSamples = Sample_Num_Cols * Sample_Num_Rows; //总帧数
		fprintf(fp_out, "%s %d %d %d\n", imgName.c_str(),Sample_Num_Rows,Sample_Num_Cols,nSamples);
		fprintf(stdout, "num_rows:%d num_cols:%d num_tol:%d\n", Sample_Num_Rows, Sample_Num_Cols, nSamples);
	}
	fclose(fp);
	fclose(fp_out);
}


int main(int argc, char* argv[])
{
	char* list_name = NULL;
	char* out_file = NULL;

	if (argc != 5)
	{
		fprintf(stdout, "Miss Parameter! \
			.exe -L list -O out_file\n");
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
			out_file = argv[i + 1];
			i += 2;
			continue;
		}
		i++;
	}

	if (!list_name || !out_file)
	{
		fprintf(stdout, "Miss Parameter!\n");
		return -1;
	}

	Run(list_name, out_file);

	return 0;
}
