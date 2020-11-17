// gmmbin/gmm-est.cc

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
//#include "tree/context-dep.h"
#include "hmm/transition-model-2D.h"
#include "gmm/mle-am-diag-gmm.h"

// 主要分两部分，一部分更新 TransitionModel，一部分更新GMM
int main(int argc, char *argv[]) {
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;

		const char *usage =
			"Do Maximum Likelihood re-estimation of GMM-based acoustic model\n"
			"Usage:  gmm-est-2D [options] <model-in> <stats-in> <model-out>\n"
			"e.g.: gmm-est 1.mdl 1.acc 2.mdl\n";

		bool binary_write = true;
		MleTransitionUpdateConfig tcfg;
		MleDiagGmmOptions gmm_opts;
		int32 mixup = 0;
		int32 mixdown = 0;
		BaseFloat perturb_factor = 0.01;
		BaseFloat power = 0.2;
		BaseFloat min_count = 20.0;
		std::string update_flags_str = "mvwt";
		std::string occs_out_filename;

		ParseOptions po(usage); // 使用usage字串初始化一个ParseOptions类的实例po
		// 对ParseOptions对象注册命令行选项(Option的结构体有它自己的注册函数)
		po.Register("binary", &binary_write, "Write output in binary mode");
		po.Register("mix-up", &mixup, "Increase number of mixture components to "
			"this overall target."); // mix-up选项指定当前需要达到的高斯数
		po.Register("min-count", &min_count,
			"Minimum per-Gaussian count enforced while mixing up and down.");
		po.Register("mix-down", &mixdown, "If nonzero, merge mixture components to this "
			"target.");
		po.Register("power", &power, "If mixing up, power to allocate Gaussians to"
			" states.");
		po.Register("update-flags", &update_flags_str, "Which GMM parameters to "
			"update: subset of mvwt.");
		po.Register("perturb-factor", &perturb_factor, "While mixing up, perturb "
			"means by standard deviation times this factor.");
		po.Register("write-occs", &occs_out_filename, "File to write pdf "
			"occupation counts to.");
		tcfg.Register(&po);
		gmm_opts.Register(&po);

		po.Read(argc, argv); // 对命令行参数进行解析
		// 检查是否存在有效数量的位置参数
		if (po.NumArgs() != 3) {
			po.PrintUsage();
			exit(1);
		}

		kaldi::GmmFlagsType update_flags =
			StringToGmmFlags(update_flags_str); // update_flags_str存放'mvwt'的子串，指定哪些GMM参数需要更新

		// 读取指定位置的命令行参数，并赋值给相应的选项
		std::string model_in_filename = po.GetArg(1),
			stats_filename = po.GetArg(2),
			model_out_filename = po.GetArg(3);

		// 从上一个模型（.mdl）中读取TM和GMMs的信息到trans_model和am_gmm
		AmDiagGmm am_gmm;
		TransitionModel_2D trans_model;
		{
			bool binary_read;
			Input ki(model_in_filename, &binary_read);
			trans_model.Read(ki.Stream(), binary_read);
			am_gmm.Read(ki.Stream(), binary_read);
		}

		// 从累积量文件中读取
		// 【1、所有文件所有frame中出现的trans_id计数值到transition_accs】
		// 【2、每个DiagGmm更新所需的统计量到gmm_accs】
		Vector<double> transition_accs_top_down;
		Vector<double> transition_accs_left_right;
		AccumAmDiagGmm gmm_accs;
		{
			bool binary;
			Input ki(stats_filename, &binary);
			transition_accs_top_down.Read(ki.Stream(), binary);
			transition_accs_left_right.Read(ki.Stream(), binary);
			gmm_accs.Read(ki.Stream(), binary, true);  // true == add; doesn't matter here.
		}

		if (update_flags & kGmmTransitions) {  // Update transition model.
			BaseFloat objf_impr, count;
			// MleUpdate根据trans_id的统计量来更新trans_id的log_prob_，记录trans_id的总个数（count）（约等于frame数，因为frame到frame才有状态转移），以及
			// objf_impr = counts(tid) * (Log(new_probs(tid)) - Log(old_probs(tid))) 为每段转移弧出现次数与他们转移概率的对数差乘积之和
			trans_model.MleUpdate_TopDown(transition_accs_top_down, tcfg, &objf_impr, &count);
			KALDI_LOG << "Transition model update: Overall " << (objf_impr / count)
				<< " log-like improvement per frame over " << (count)
				<< " frames. In TopDown direction.";
			trans_model.MleUpdate_LeftRight(transition_accs_left_right, tcfg, &objf_impr, &count);
			KALDI_LOG << "Transition model update: Overall " << (objf_impr / count)
				<< " log-like improvement per frame over " << (count)
				<< " frames. In LeftRight direction.";
		}

		{  // Update GMMs.
			BaseFloat objf_impr, count;
			BaseFloat tot_like = gmm_accs.TotLogLike(),
				tot_t = gmm_accs.TotCount();
			MleAmDiagGmmUpdate(gmm_opts, gmm_accs, update_flags, &am_gmm,
				&objf_impr, &count);
			KALDI_LOG << "GMM update: Overall " << (objf_impr / count)
				<< " objective function improvement per frame over "
				<< count << " frames";
			KALDI_LOG << "GMM update: Overall avg like per frame = "
				<< (tot_like / tot_t) << " over " << tot_t << " frames.";
		}

		if (mixup != 0 || mixdown != 0 || !occs_out_filename.empty()) {
			// get pdf occupation counts
			Vector<BaseFloat> pdf_occs;
			pdf_occs.Resize(gmm_accs.NumAccs());
			for (int i = 0; i < gmm_accs.NumAccs(); i++)
				pdf_occs(i) = gmm_accs.GetAcc(i).occupancy().Sum(); // p(m|Oj)对j、m求和，其中Oj为由这个GMM生成的观测向量（帧），m表示这个GMM中第m个分量

			if (mixdown != 0)
				am_gmm.MergeByCount(pdf_occs, mixdown, power, min_count);

			if (mixup != 0)
				am_gmm.SplitByCount(pdf_occs, mixup, perturb_factor,
					power, min_count);

			if (!occs_out_filename.empty()) {
				bool binary = false;
				WriteKaldiObject(pdf_occs, occs_out_filename, binary);
			}
		}

		{
			Output ko(model_out_filename, binary_write);
			trans_model.Write(ko.Stream(), binary_write);
			am_gmm.Write(ko.Stream(), binary_write);
		}

		KALDI_LOG << "Written model to " << model_out_filename;
		return 0;
	}
	catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return -1;
	}
}


