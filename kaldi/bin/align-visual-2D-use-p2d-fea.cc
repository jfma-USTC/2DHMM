// bin/align-equal.cc

// Copyright 2009-2013  Microsoft Corporation
//                      Johns Hopkins University (Author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "hmm/transition-model-2D.h"
#include <opencv2/opencv.hpp>


namespace kaldi {
	void GetImgInfo(std::string srcImgPath, std::string &dirPath, std::string &imgName, std::string &imgType)
	{   // e.g.  srcImgPath = "/disk2/myfile/01.htk"     ----------------->
	    //       dirParh = "/disk2/myfile"; imgName = "01"; imgType = "htk"
		int nPos1 = srcImgPath.find_last_of("\\");
		int nPos2 = srcImgPath.find_last_of("/");
		int nPos = nPos1;

		if (nPos1 < nPos2)
			nPos = nPos2;

		dirPath = srcImgPath.substr(0, nPos);
		std::string name = srcImgPath.substr(nPos + 1, srcImgPath.length() - 1);
		nPos = name.find_last_of(".");
		imgName = name.substr(0, nPos);
		imgType = name.substr(nPos + 1, name.length() - 1);
	}

	std::vector<std::string> mysplit(const std::string& str, const std::string& delim) {
		std::vector<std::string> res;
		if ("" == str) return res;
		//先将要切割的字符串从string类型转换为char*类型  
		char * strs = new char[str.length() + 1];  
		strcpy(strs, str.c_str());

		char * d = new char[delim.length() + 1];
		strcpy(d, delim.c_str());

		char *p = strtok(strs, d);
		while (p) {
			std::string s = p; //分割得到的字符串转换为string类型  
			res.push_back(s); //存入结果数组  
			p = strtok(NULL, d);
		}

		return res;
	} // end namespace kaldi

	bool sortByM1(cv::Rect m, cv::Rect n) {
		return m.x < n.x;
	}

	int calcLineHeight(cv::Mat imgOri, std::vector<cv::Point> &points) {
		cv::Mat binImg = imgOri.clone();
		cv::threshold(imgOri, binImg, 0, 255, CV_THRESH_OTSU);
		binImg = 255 - binImg;
		cv::Mat tempImg = binImg.clone();
		std::vector<std::vector<cv::Point> > contour;
		findContours(tempImg, contour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		std::vector<cv::Rect> rectVec_rank;
		rectVec_rank.clear();
		for (int k = 0; k < contour.size(); k++) {
			cv::Rect currRect = boundingRect(contour[k]);
			if (currRect.width > 10 || currRect.height > 10)
				rectVec_rank.push_back(currRect);
		}

		std::sort(rectVec_rank.begin(), rectVec_rank.end(), sortByM1);

		std::vector<cv::Rect> rectVec;
		rectVec.clear();
		for (int k = 0; k < rectVec_rank.size(); k++) {
			cv::Rect currRect = rectVec_rank[k];
			if (currRect.width > 10 || currRect.height > 10) {
				if (rectVec.empty())
					rectVec.push_back(currRect);
				else {
					cv::Rect lastRect = rectVec[rectVec.size() - 1];
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
						rectVec.push_back(cv::Rect(xFuseStart, yFuseStart, xFuseEnd - xFuseStart, yFuseEnd - yFuseStart));
						//}
						//else
						//{
						//	rectVec.push_back(currRect);
						//}
					}
					else {
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
				points.push_back(cv::Point(j, yPre + (j - xPre)*slope));
			for (int j = rectVec[i].x; j < rectVec[i].x + rectVec[i].width; j++)
				points.push_back(cv::Point(j, yCore));

			xPre = rectVec[i].x + rectVec[i].width;
			yPre = yCore;
		}
		for (int i = xPre; i < imgOri.cols; i++)
			points.push_back(cv::Point(i, yPre));

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

	cv::Mat lineNormbyCentroid(cv::Mat imgOri, std::vector<cv::Point> &points)
	{
		int lineHeight = calcLineHeight(imgOri, points);
		float ratio = 60.0 / lineHeight;
		int normHeight = imgOri.rows*ratio;
		int normWidth = imgOri.cols*ratio;
		cv::Mat normImg(normHeight, normWidth, CV_8UC1);
		cv::resize(imgOri, normImg, normImg.size());

		int topPad = 0;
		int bottomPad = 0;
		int leftPad = 20;
		int rightPad = 20;

		cv::Mat normPadImg(normHeight + topPad + bottomPad, normWidth + leftPad + rightPad, CV_8UC1);
		cv::copyMakeBorder(normImg, normPadImg, topPad, bottomPad, leftPad, rightPad, cv::BORDER_CONSTANT, cv::Scalar(255));

		return normPadImg;
	}
}

int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;
		const char *usage = "Write equally spaced alignments of utterances "
			"(to get training started)\n"
			"Usage:  align-visual-2D [options] <model-in> <bmp-path> <alignments-rspecifier> <output-bmp-path>\n"
			"e.g.: \n"
			" align-visual-2D --shift-right=8 --shift-down=8 final.mdl all_bmp_path.txt all.ali ali_map_dir\n";

		ParseOptions po(usage);
		
		int32 shift_right = -1;
		int32 shift_down = -1;

		po.Register("shift-right", &shift_right,
			"pixels shift while extract features rightward");
		po.Register("shift-down", &shift_down,
			"pixels shift while extract features downward");

		po.Read(argc, argv);

		if (po.NumArgs() != 4) {
			po.PrintUsage();
			exit(1);
		}
		
		std::string model_in_filename = po.GetArg(1);    // .mdl主要为了获取trans_state对应的hmm_state
		std::string bmp_read_path = po.GetArg(2);        // 读取每个sample的存放地址（file_name bmp_save_path）
		std::string alignment_rspecifier = po.GetArg(3); // 读取每个sample对应的alignment vector
		std::string bmp_save_path = po.GetArg(4);        // 处理之后的每张图片存放的文件夹名称

		TransitionModel_2D trans_model;
		{
			bool binary; // binary的值通过ki实例化的语句来判定，二进制为1，文本格式为0
			Input ki(model_in_filename, &binary); // Kaldi根据开头是否"\0B"来判断是二进制还是文本格式，同时准备好一个流ki
			trans_model.Read(ki.Stream(), binary);// 从.mdl流里读取属于TransitionModel类的对象到trans_model
		}

		std::ifstream bmp_path_stream;
		bmp_path_stream.open(bmp_read_path, std::ios::in);
		if (!bmp_path_stream) {
			std::cout << "can not open " << bmp_read_path << std::endl;
			return -1;
		}
		RandomAccessInt32VectorReader alignment_reader(alignment_rspecifier);
		
		std::string this_bmp_path;

		std::vector< std::vector<int> > RGB_LIST =
		{
			{0,0,255},{0,255,127},{0,255,255},{124,252,0},{127,255,212},
			{100,149,237},{208,32,144},{255,0,255},{255,0,0},{255,105,180},
			{188,143,143},{205,92,92},{255,255,0},{255,215,0},{238,221,130},
			{255,218,185},{255,127,0},{155,48,255},{255,181,197},{0,205,102}
		};

		int32 done = 0, no_ali = 0, other_error = 0;
		while (std::getline(bmp_path_stream, this_bmp_path)) {
			std::vector<std::string> path_pair = mysplit(this_bmp_path, " ");
			KALDI_ASSERT(path_pair.size() == 2 && "bmp path is not in pair (file_name bmp_save_path) format!");
			std::string& file_name = path_pair[0];
			std::string& bmp_path = path_pair[1];
			if (alignment_reader.HasKey(file_name)) {
				const std::vector<int32> &ali_list = alignment_reader.Value(file_name);
				std::string dir;
				std::string imgName;
				std::string imgType;
				GetImgInfo(bmp_path, dir, imgName, imgType);
				std::string ali_bmp_path;
				if (bmp_save_path.find_last_of("/") == bmp_save_path.size() - 1) {
					ali_bmp_path = bmp_save_path + imgName + "_ali." + imgType;
				}
				else {
					ali_bmp_path = bmp_save_path + "/" + imgName + "_ali." + imgType;
				}
				cv::Mat img = cv::imread(bmp_path, 0);
				if (img.cols * img.rows == 0) {
					KALDI_WARN << "Image " << bmp_path << " read failed!";
					other_error++;
					continue;
				}
				cv::Mat img_3ch;
				if (img.channels() == 1) {
					img_3ch = cv::Mat::zeros(img.rows, img.cols, CV_8UC3);
					std::vector<cv::Mat> channels;
					for (size_t i = 0; i < 3; i++)
						channels.push_back(img);
					cv::merge(channels, img_3ch);
				}
				else if (img.channels() == 3) {
					img_3ch = img;
				}
				else {
					KALDI_WARN << "img " << bmp_path << " has channels: " << img.channels() << " , unable to handle.";
					other_error++;
					continue;
				}
				std::vector<int> this_rgb;
				std::vector<cv::Point> midPoints;
				cv::Mat LineNormedImg = lineNormbyCentroid(img, midPoints);
				int cols = LineNormedImg.cols,
					frameWidth = 40,  // 水平切帧的帧宽
					frameHeight = 80, // 水平切帧的帧高
					blockHeight = 40; // 竖直切帧的帧高

				int Sample_Num_Cols = (cols - frameWidth) / shift_right + 1; //水平方向上应该被切出来的帧数（有几列帧）
				int Sample_Num_Rows = (frameHeight - blockHeight) / shift_down + 1; //竖直方向上应该被切出来的帧数（有几行帧）

				int Sample_Num_Tol = Sample_Num_Rows * Sample_Num_Cols; //水平方向上应该被切出来的帧数（有几列帧）
				if (ali_list.size() != Sample_Num_Tol) {
					KALDI_WARN << "img " << bmp_path << " should have " << Sample_Num_Tol << 
						" blocks, however get " << ali_list.size() << "alignments. Please cheak [shift_down] & [shift_right] parameters";
					other_error++;
					continue;
				}

				cv::Size resize_size(Sample_Num_Cols*shift_right, Sample_Num_Rows*shift_down);
				cv::resize(img_3ch, img_3ch, resize_size);
				cv::Mat color_map = cv::Mat::zeros(Sample_Num_Rows*shift_down, Sample_Num_Cols*shift_right, CV_8UC3);
				for (size_t i = 0; i < Sample_Num_Rows; i++) {
					for (size_t j = 0; j < Sample_Num_Cols; j++) {
						int32 ali_index = i * Sample_Num_Cols + j;
						int32 ali = trans_model.TransitionStateToHmmState(ali_list[ali_index]);
						this_rgb = RGB_LIST[ali%RGB_LIST.size()];
						cv::Mat roi(color_map, cv::Rect(j*shift_right, i*shift_down, shift_right, shift_down));
						roi = cv::Scalar(this_rgb[0], this_rgb[1], this_rgb[2]);
					}
				}
				cv::Mat dst = cv::Mat::zeros(img_3ch.rows, img_3ch.cols, CV_8UC3);
				cv::addWeighted(img_3ch, 0.5, color_map, 0.5, 1.5, dst);
				cv::imwrite(ali_bmp_path, dst);
				img.release();
				img_3ch.release();
				color_map.release();
				dst.release();
				done++;
			}
			else {
				KALDI_WARN << "AlignVisual2D: no alignment info for utterance " << file_name;
				no_ali++;
			}
		}

		if (done != 0 && no_ali == 0 && other_error == 0) {
			KALDI_LOG << "Success: done " << done << " utterances.";
		}
		else {
			KALDI_WARN << "Computed " << done << " visual-alignments; " << no_ali
				<< " samples have no alignment found, " << other_error
				<< " had other errors.";
		}
		if (done != 0) return 0;
		else return 1;
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}


