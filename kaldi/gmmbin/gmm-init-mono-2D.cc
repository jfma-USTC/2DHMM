// gmmbin/gmm-init-mono-2D.cc

// Copyright 2009-2011  Microsoft Corporation

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
#include "hmm/hmm-topology-2D.h"
#include "hmm/transition-model-2D.h"
#include "tree/context-dep.h"

namespace kaldi {
	// This function reads a file like:
	// 1 2 3
	// 4 5
	// 6 7 8
	// where each line is a list of integer id's of phones (that should have their pdfs shared).
	/*
	void ReadSharedPhonesList(std::string rxfilename, std::vector<std::vector<int32> > *list_out) {
		list_out->clear(); // list_out为二维向量指针，此处调用了clear方法将listout指向的二维向量清空
		Input input(rxfilename);
		std::istream &is = input.Stream();
		std::string line;
		while (std::getline(is, line)) {
			list_out->push_back(std::vector<int32>()); // 在二维数组最后追加一个空向量
			if (!SplitStringToIntegers(line, " \t\r", true, &(list_out->back()))) // 以\t或\r分割line并赋值给刚追加的空向量
				KALDI_ERR << "Bad line in shared phones list: " << line << " (reading "
				<< PrintableRxfilename(rxfilename) << ")";
			std::sort(list_out->rbegin()->begin(), list_out->rbegin()->end());
			if (!IsSortedAndUniq(*(list_out->rbegin())))
				KALDI_ERR << "Bad line in shared phones list (repeated phone): " << line
				<< " (reading " << PrintableRxfilename(rxfilename) << ")";
		}
	}
	*/

} // end namespace kaldi

int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		using kaldi::int32;

		const char *usage =
			"Initialize monophone GMM.\n"
			"Usage:  gmm-init-mono-2D <topology-in> <dim> <model-out> \n"
			"e.g.: \n"
			" gmm-init-mono-2D topo 39 mono.mdl\n";
		// 声明选项并设置默认值
		bool binary = true;
		std::string train_feats;
		std::string shared_phones_rxfilename;
		BaseFloat perturb_factor = 0.0;
		// 使用usage字串初始化一个ParseOptions类的实例po
		ParseOptions po(usage);
		// 对ParseOptions对象注册命令行选项(Option的结构体有它自己的注册函数)
		// 详情参看：https://shiweipku.gitbooks.io/chinese-doc-of-kaldi/content/parsing_cmd_options.html
		po.Register("binary", &binary, "Write output in binary mode");
		po.Register("train-feats", &train_feats,
			"rspecifier for training features [used to set mean and variance]");
		po.Register("shared-phones", &shared_phones_rxfilename,
			"rxfilename containing, on each line, a list of phones whose pdfs should be shared.");
		po.Register("perturb-factor", &perturb_factor,
			"Perturb the means using this fraction of standard deviation.");
		// 对命令行参数进行解析
		po.Read(argc, argv);
		// 检查是否存在有效数量的位置参数
		if (po.NumArgs() != 3) {
			po.PrintUsage();
			exit(1);
		}

		// The positional arguments get read here (they can only be obtained
		// from ParseOptions as strings).
		// 读取指定位置的命令行参数，并赋值给相应的选项
		// （注意这些参数只能通过ParseOptions类以字符串的形式获取）
		std::string topo_filename = po.GetArg(1);
		int dim = atoi(po.GetArg(2).c_str());  // atoi把字符串转换成整型数
		// 用c_str()方法将string类转化为char *类型，再用atoi将char *类型转化为整形并赋值给dim(特征维度)
		KALDI_ASSERT(dim > 0 && dim < 10000);
		std::string model_filename = po.GetArg(3);
		//std::string tree_filename = po.GetArg(4);

		// 创建dim维的方差、均值向量并设初值
		Vector<BaseFloat> glob_inv_var(dim);
		glob_inv_var.Set(1.0);
		Vector<BaseFloat> glob_mean(dim);
		glob_mean.Set(1.0);

		if (train_feats != "") {
			double count = 0.0;
			Vector<double> var_stats(dim);
			Vector<double> mean_stats(dim);
			// typedef  SequentialTableReader<KaldiObjectHolder<Matrix<double> > >  SequentialDoubleMatrixReader;
			// SequentialDoubleMatrixReader是模板类SequentialTableReader确定类型之后的类，feat_reader是它的一个实例
			// 可以把Table当成一种通用的map或者list
			SequentialDoubleMatrixReader feat_reader(train_feats); // train_feats中存放部分文件的特征向量（前十个文件）
			for (; !feat_reader.Done(); feat_reader.Next()) { // 顺序遍历feat_reader类（其实是一个容器）
				const Matrix<double> &mat = feat_reader.Value();
				// 顺序读取容器中的values，这里subfeats存放的某个value为单个文件中提取出来的
				// 一系列特征向量构成的特征矩阵（切分之后每个frame对应一个特征向量）
				for (int32 i = 0; i < mat.NumRows(); i++) { // 将当前特征矩阵每行写入到均值和方差，并统计行数
					count += 1.0;
					var_stats.AddVec2(1.0, mat.Row(i)); // 对每行平方求和
					mean_stats.AddVec(1.0, mat.Row(i)); // 对每行求和
				}
			}
			if (count == 0) { KALDI_ERR << "no features were seen."; } // count等于列数（特征向量维数）
			var_stats.Scale(1.0 / count); // Scale方法将当前向量中所有元素乘以某个常数，这里起到归一化作用（平方和的均值）
			mean_stats.Scale(1.0 / count); // 所有特征矩阵每行求和后的均值
			var_stats.AddVec2(-1.0, mean_stats); // AddVec2方法执行*this = *this + alpha * rv^2【方差=方差-均值^2】
			if (var_stats.Min() <= 0.0)
				KALDI_ERR << "bad variance";
			var_stats.InvertElements(); // InvertElements方法对所有元素求倒数
			glob_inv_var.CopyFromVec(var_stats); // 将得到的结果复制到全局均值、方差中
			glob_mean.CopyFromVec(mean_stats);
		}

		HmmTopology_2D topo;
		bool binary_in;
		// Kaldi根据开头是否"\0B"来判断是二进制还是文本格式，同时准备好一个流
		Input ki(topo_filename, &binary_in); // topo_filename存放‘$lang/topo’
		topo.Read(ki.Stream(), binary_in); // 从流里读取属于HmmTopology类的对象到topo

		const std::vector<int32> &phones = topo.GetPhones(); // GetPhones方法从topo中返回排序好的音素列表

		std::vector<int32> phone2num_pdf_classes(1 + phones.back()); // back方法返回向量最后一个元素的引用
		// 假设phone中存储着数字1~200，则phone2num_pdf_classes创建了长度为201的向量

		int32 num_pdfs = 0;
		int32 num_pdfs_for_each_phone = 0;
		for (size_t i = 0; i < phones.size(); i++) // 从1~end对phone2num_pdf_classes进行赋值
		{
			num_pdfs_for_each_phone = topo.NumPdfClasses(phones[i]);
			num_pdfs += num_pdfs_for_each_phone;
			phone2num_pdf_classes[phones[i]] = num_pdfs_for_each_phone; // NumPdfClasses方法返回对应于这个音素的pdf数量
		}
		/*
		// 根据每个 phone 音素对应 pdf 数量来创建 ContextDependency (决策树)对象
		// Now the tree [not really a tree at this point]:
		ContextDependency *ctx_dep = NULL;
		if (shared_phones_rxfilename == "") {  // No sharing of phones: standard approach.
			ctx_dep = MonophoneContextDependency(phones, phone2num_pdf_classes);
		}
		else {
			std::vector<std::vector<int32> > shared_phones;
			ReadSharedPhonesList(shared_phones_rxfilename, &shared_phones); // 将sets.int文件读入二维向量shared_phones中
			// ReadSharedPhonesList crashes on error.
			ctx_dep = MonophoneContextDependencyShared(shared_phones, phone2num_pdf_classes);
		}
		// 获取所有 pdfs 数量 = phones * 每个 phone 含有的 pdfclass 数量
		int32 num_pdfs = ctx_dep->NumPdfs();
		*/

		// 根据特征统计出的结果，创建 DiagGmm 初始化模型
		AmDiagGmm am_gmm; // AmDiagGmm可以视为由gmm组成的向量
		DiagGmm gmm;
		gmm.Resize(1, dim); // Resize arrays to this dim. Does not initialize data.
		{  // Initialize the gmm.
			Matrix<BaseFloat> inv_var(1, dim);
			inv_var.Row(0).CopyFromVec(glob_inv_var); // 将全局方差拷贝到方差矩阵的第一行
			Matrix<BaseFloat> mu(1, dim);
			mu.Row(0).CopyFromVec(glob_mean);
			Vector<BaseFloat> weights(1);
			weights.Set(1.0);
			gmm.SetInvVarsAndMeans(inv_var, mu); // 更新均值、方差到DiagGmm类的实例gmm中
			gmm.SetWeights(weights);
			gmm.ComputeGconsts(); // 计算Gconsts并写入gmm
		}

		// 将每个 pdf 都初始化为上述创建的 gmm
		for (int i = 0; i < num_pdfs; i++)
			am_gmm.AddPdf(gmm); // 将gmms都写入到am_gmm内，并更新PDFNUM的数值

		// 添加 perturb_factor 因子
		if (perturb_factor != 0.0) { // 若perturb_factor有外部指定值
			for (int i = 0; i < num_pdfs; i++)
				// Perturbs（扰动）， the component means with a random vector（随机向量） multiplied by the perturb factor.
				am_gmm.GetPdf(i).Perturb(perturb_factor);
		}

		// 将 ContextDependency 与 topo 合并为一个模型文件保存下来
		// Now the transition model:
		TransitionModel_2D trans_model_2D(topo);

		{
			Output ko(model_filename, binary); // 打开model_filename并将文件句柄赋给ko。binary默认为true，可由命令行指定
			trans_model_2D.Write(ko.Stream(), binary); // 写入trans_model
			am_gmm.Write(ko.Stream(), binary); // 写入am_gmm
		}

		/*
		// 将ContextDependency存为决策树文件
		// Now write the tree.
		ctx_dep->Write(Output(tree_filename, binary).Stream(),
					   binary);

		delete ctx_dep;
		*/

		return 0;
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}

