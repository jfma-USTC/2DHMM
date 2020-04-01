// gmmbin/gmm-acc-stats-ali.cc

// Copyright 2009-2012  Microsoft Corporation  Johns Hopkins University (Author: Daniel Povey)

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
#include "gmm/am-diag-gmm.h"
#include "hmm/transition-model-2D.h"
#include "gmm/mle-am-diag-gmm.h"

int main(int argc, char *argv[]) {
	using namespace kaldi;
	typedef kaldi::int32 int32;
	try {
		const char *usage =
			"Accumulate stats for GMM training.\n"
			"Usage:  gmm-acc-stats-ali-2D [options] <model-in> <feature-rspecifier> "
			"<block-rspecifier> <alignments-rspecifier> <stats-out>\n"
			"e.g.:\n gmm-acc-stats-ali 1.mdl scp:train.scp ark:block ark:1.ali 1.acc\n";

		ParseOptions po(usage); // 使用usage字串初始化一个ParseOptions类的实例po
		bool binary = true;
		po.Register("binary", &binary, "Write output in binary mode");// 对ParseOptions对象注册命令行选项(Option的结构体有它自己的注册函数)
		po.Read(argc, argv); // 对命令行参数进行解析
		// 检查是否存在有效数量的位置参数
		if (po.NumArgs() != 5) {
			po.PrintUsage();
			exit(1);
		}
		// 读取指定位置的命令行参数，并赋值给相应的选项
		std::string model_filename = po.GetArg(1),
			feature_rspecifier = po.GetArg(2),
			block_rspecifier = po.GetArg(3),
			alignments_rspecifier = po.GetArg(4),
			accs_wxfilename = po.GetArg(5);

		AmDiagGmm am_gmm;
		TransitionModel_2D trans_model;
		{
			bool binary; // binary的值通过ki实例化的语句来判定，二进制为1，文本格式为0
			Input ki(model_filename, &binary); // Kaldi根据开头是否"\0B"来判断是二进制还是文本格式，同时准备好一个流ki
			trans_model.Read(ki.Stream(), binary);// 从.mdl流里读取属于TransitionModel类的对象到trans_model
			am_gmm.Read(ki.Stream(), binary);// 从.mdl流里读取属于AmDiagGmm类的对象到am_gmm(共有NUMPDFS个DiagGmms组成AmDiagGmm)
		}

		Vector<double> transition_accs_top_down; // to save weighted counts of top2down trans_id
		Vector<double> transition_accs_left_right; // to save weighted counts of left2right trans_id
		trans_model.InitStats_TopDown(&transition_accs_top_down); // 将transition_accs_top_down的长度初始化为总竖直转移弧数+1
		trans_model.InitStats_LeftRight(&transition_accs_left_right); // 将transition_accs_top_down的长度初始化为总竖直转移弧数+1
		AccumAmDiagGmm gmm_accs; // AccumAmDiagGmm类中的私有变量为vector<AccumDiagGmm*>，AccumDiagGmm类存放了一个DiagGmm参数更新所需的信息
		gmm_accs.Init(am_gmm, kGmmAll); // 使用am_gmm来初始化每个GMM的累积器gmm_accs，kGmmAll（0x00F）指更新均值、方差、权重、trans

		double tot_like = 0.0;
		kaldi::int64 tot_t = 0;

		SequentialBaseFloatMatrixReader feature_reader(feature_rspecifier);
		RandomAccessInt32VectorReader alignments_reader(alignments_rspecifier);
		RandomAccessInt32VectorReader block_info_reader(block_rspecifier);

		int32 num_done = 0, num_err = 0;
		for (; !feature_reader.Done(); feature_reader.Next()) { // 处理每个子任务中的所有训练样本
			std::string key = feature_reader.Key(); // 存放样本名称
			if (!alignments_reader.HasKey(key)) {
				KALDI_WARN << "No alignment for utterance " << key;
				num_err++;
				continue;
			}
			else if(!block_info_reader.HasKey(key)) {
				KALDI_WARN << "No block information for utterance " << key;
				num_err++;
				continue;
			}
			else {
				const Matrix<BaseFloat> &mat = feature_reader.Value(); // mat存储当前feature_reader指向的Value（当前样本的feats）
				const std::vector<int32> &alignment = alignments_reader.Value(key); // alignment存储alignments_reader由key指定的对齐信息
				const std::vector<int32> &block_info = block_info_reader.Value(key); // blcok_info存储block_info_reader由key指定的帧数信息
				if (alignment.size() != mat.NumRows()) { // 通过检测feats帧数和对齐数是否相等来判断数据是否有效
					KALDI_WARN << "Alignments has wrong size " << (alignment.size())
						<< " vs. " << (mat.NumRows());
					num_err++;
					continue;
				}
				// 如果某个sample的block-information不是三个【row_num col_num tol_num】，则错误数+1，跳过这个sample
				if (block_info.size() != 3) {
					KALDI_WARN << "Block information size supposed to be 3, but get "
						<< block_info.size() << " from " << key << " instead.";
					num_err++;
					continue;
				}
				size_t block_row_num = static_cast<size_t>(block_info[0]),
					block_col_num = static_cast<size_t>(block_info[1]),
					block_tol_num = static_cast<size_t>(block_info[2]);
				if (block_row_num*block_col_num != block_tol_num) { // 检测block_info的正确性
					KALDI_WARN << "Block rows number * cols number not equal to total number, sample name:" << key
						<< "rows:" << block_row_num << " cols:" << block_col_num << " total:" << block_tol_num;
					num_err++;
					continue;
				}
				if (alignment.size() != block_tol_num) { // 检测block_info的正确性
					KALDI_WARN << "Block total number has error " << block_tol_num
						<< " vs. Alignment size " << alignment.size();
					num_err++;
					continue;
				}
				num_done++;
				BaseFloat tot_like_this_file = 0.0;

				// 对于当前文件中的每一帧遍历，更新pdf_id对应的特征数目计数值、trans_id计数值、文件生成概率
				for (size_t i = 0; i < alignment.size(); i++) {
					int32 this_state = alignment[i],
						down_side_state = (i + block_col_num) >= block_tol_num ? 0 : alignment[i + block_col_num],
						right_side_state = ((i + 1) % block_col_num) == 0 ? 0 : alignment[i + 1];
					int32 tid_top_down = trans_model.StatePairToTransitionId_TopDown(this_state, down_side_state),  // transition identifier.
						tid_left_right = trans_model.StatePairToTransitionId_LeftRight(this_state, right_side_state);  // transition identifier.
					if (tid_top_down == -1) {
						KALDI_WARN << "Alignment error, TopDown transition from trans_state " << this_state
							<< " to " << down_side_state << " is not allowed!";
						num_err++;
						continue;
					}
					if (tid_left_right == -1) {
						KALDI_WARN << "Alignment error, LeftRight transition from trans_state " << this_state
							<< " to " << right_side_state << " is not allowed!";
						num_err++;
						continue;
					}
					int32 pdf_id = trans_model.TransitionIdToPdf_TopDown(tid_top_down); // 通过TM的id2pdf_id_映射关系找到对应的pdf_id
					trans_model.Accumulate_TopDown(1.0, tid_top_down, &transition_accs_top_down); // 在transition_accs中对应的弧（trans_id）位置计数值加一
					trans_model.Accumulate_LeftRight(1.0, tid_left_right, &transition_accs_left_right); // 在transition_accs中对应的弧（trans_id）位置计数值加一
				   // am_gmm存放.mdl中GMMs的相关参数、mat存放当前文件的特征矩阵（每行为一个特征向量）、pdf_id是由对齐序列给出的当前帧对应的GMM
					tot_like_this_file += gmm_accs.AccumulateForGmm(am_gmm, mat.Row(i), // 累加每一帧由对应的GMM产生的对数概率
						pdf_id, 1.0);
				}
				tot_like += tot_like_this_file; // tot_like_this_file存放当前文件（feature vectors）由对应的对齐序列（GMM sequence）产生的对数概率之和
				tot_t += alignment.size(); // tot_t累计所有文件的总帧数
				if (num_done % 50 == 0) { // 每处理50个文件输出一次log信息，包括第50*n个文件的平均每帧生成概率（依赖于当前的对齐信息）
					KALDI_LOG << "Processed " << num_done << " utterances; for utterance "
						<< key << " avg. like is "
						<< (tot_like_this_file / alignment.size())
						<< " over " << alignment.size() << " frames.";
				}
			}
		}
		KALDI_LOG << "Done " << num_done << " files, " << num_err
			<< " with errors.";

		// 所有文件每一帧的生成概率对数和（tot_like）/所有文件总帧数（tot_t）=平均每帧生成概率（avg like per frame）
		KALDI_LOG << "Overall avg like per frame (Gaussian only) = "
			<< (tot_like / tot_t) << " over " << tot_t << " frames.";

		{
			Output ko(accs_wxfilename, binary); // 以accs_wxfilename为文件名注册输出文件句柄ko，格式为binary（默认为true，可以由命令行指定）
			transition_accs_top_down.Write(ko.Stream(), binary); // 将所有弧在所有文件所有frame中出现的计数值transition_accs（double型向量）写入accs_wxfilename
			transition_accs_left_right.Write(ko.Stream(), binary); // 将所有弧在所有文件所有frame中出现的计数值transition_accs（double型向量）写入accs_wxfilename
			gmm_accs.Write(ko.Stream(), binary); // 将每个DiagGmm更新所需的参数gmm_accs写入accs_wxfilename
		}
		KALDI_LOG << "Written accs.";
		if (num_done != 0)
			return 0;
		else
			return 1;
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}


