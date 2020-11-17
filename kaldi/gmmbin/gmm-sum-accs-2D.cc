// gmmbin/gmm-sum-accs-2D.cc

// Copyright 2009-2011  Saarland University;  Microsoft Corporation

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

#include "util/common-utils.h"
#include "gmm/mle-am-diag-gmm.h"
//#include "hmm/transition-model.h"

// 将分散在 JOB 个文件中、由gmm-acc-stats-ali-2D 生成的累计量中 对应相同 trans-id、pdf-id 的累计量合并在一起
int main(int argc, char *argv[]) {
  try {
    typedef kaldi::int32 int32;

    const char *usage =
        "Sum multiple accumulated stats files for GMM training.\n"
        "Usage: gmm-sum-accs-2D [options] <stats-out> <stats-in1> <stats-in2> ...\n"
        "E.g.: gmm-sum-accs-2D 1.acc 1.1.acc 1.2.acc\n";

    bool binary = true;
    kaldi::ParseOptions po(usage); // 使用usage字串初始化一个ParseOptions类的实例po
    po.Register("binary", &binary, "Write output in binary mode"); // 对ParseOptions对象注册命令行选项(Option的结构体有它自己的注册函数)
	po.Read(argc, argv); // 对命令行参数进行解析
	// 检查是否存在有效数量的位置参数
    if (po.NumArgs() < 2) {
      po.PrintUsage();
      exit(1);
    }

	// 读取指定位置的命令行参数，并赋值给相应的选项
    std::string stats_out_filename = po.GetArg(1); // 合并累积量的输出文件名
	kaldi::Vector<double> transition_accs_top_down;
	kaldi::Vector<double> transition_accs_left_right;
    kaldi::AccumAmDiagGmm gmm_accs;

    int num_accs = po.NumArgs() - 1;
    for (int i = 2, max = po.NumArgs(); i <= max; i++) { // 读取所有输入的累积量文件并累加
      std::string stats_in_filename = po.GetArg(i);
      bool binary_read;
      kaldi::Input ki(stats_in_filename, &binary_read);
	  transition_accs_top_down.Read(ki.Stream(), binary_read, true /*add read values*/);
	  transition_accs_left_right.Read(ki.Stream(), binary_read, true /*add read values*/);
      gmm_accs.Read(ki.Stream(), binary_read, true /*add read values*/); // true选项表示累加
    }

	// 输出累积量并给出相关日志信息
    // Write out the accs
    {
      kaldi::Output ko(stats_out_filename, binary);
	  transition_accs_top_down.Write(ko.Stream(), binary);
	  transition_accs_left_right.Write(ko.Stream(), binary);
      gmm_accs.Write(ko.Stream(), binary);
    }
    KALDI_LOG << "Summed " << num_accs << " stats, total count "
              << gmm_accs.TotCount() << ", avg like/frame "
              << (gmm_accs.TotLogLike() / gmm_accs.TotCount());
    KALDI_LOG << "Total count of stats is " << gmm_accs.TotStatsCount();
    KALDI_LOG << "Written stats to " << stats_out_filename;
  } catch(const std::exception &e) {
    std::cerr << e.what() << '\n';
    return -1;
  }
}


