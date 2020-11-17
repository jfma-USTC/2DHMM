#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "hmm/transition-model.h"
#include "util/text-utils.h"

//#define SHOW_DETAIL

int main(int argc, char *argv[]) {
	using namespace kaldi;
	try {
		using namespace kaldi;
		typedef kaldi::int32 int32;
		const char *usage =
			"Initialize tree from 1.mdl; Note only TE-based tree could be rebuild\n"
			"Usage:  bulid-tree-from-model [options] <model-in> <tree-out-txt>\n"
			"e.g.: \n"
			" bulid-tree-from-model 1.mdl tree.txt\n";
		
		ParseOptions po(usage);
		po.Read(argc, argv);

		if (po.NumArgs() != 2) {
			po.PrintUsage();
			exit(1);
		}

		std::string
			model_in_filename = po.GetArg(1),
			tree_out_filename = po.GetArg(2);

		// 从上一个模型（.mdl）中读取TM的信息到trans_model
		TransitionModel trans_model;
		{
			bool binary_read;
			Input ki(model_in_filename, &binary_read);
			trans_model.Read(ki.Stream(), binary_read);
		}
		HmmTopology topo = trans_model.GetTopo();

		const std::vector<int32> &phones = topo.GetPhones(); // 注意，这里的phones都是按升序排序好的，e.g. 1 2 3 4 5 ... 3000
		KALDI_ASSERT(!phones.empty());
		int32 max_phone_id = *std::max_element(phones.begin(), phones.end());
		std::vector<int32> num_pdf_classes(1 + max_phone_id, -1);
		for (size_t i = 0; i < phones.size(); i++)
			num_pdf_classes[phones[i]] = topo.NumPdfClasses(phones[i]);

		Output ko(tree_out_filename, false); // 以accs_wxfilename为文件名注册输出文件句柄ko，格式为binary（默认为true，可以由命令行指定）

		ko.Stream() << "ContextDependency 1 0 ToPdf TE 0 " << max_phone_id+1 << " ( \n";

		for (size_t phone = 0; phone <= max_phone_id; phone++) {
			if (num_pdf_classes[phone] == -1) {
				ko.Stream() << "NULL \n";
			}
			else {
				ko.Stream() << "TE -1 " << num_pdf_classes[phone] << " (";
				for (size_t j = 0; j < num_pdf_classes[phone]; j++) {
					int32 hmm_state = j;
					int32 trans_state = trans_model.PairToState(phone, hmm_state);
					if (trans_state == -1) KALDI_ERR << "Cannot map pair(" << phone << " " << hmm_state << ") to trans_state";
					int32 pdf_id = trans_model.TransitionStateToForwardPdf(trans_state);
					// TE -1 5 ( CE 6 CE 7 CE 8 CE 9 CE 10 ) 
					ko.Stream() << " CE "<< pdf_id;
				}
				ko.Stream() << " ) \n";
			}
		}

		ko.Stream() << ") \nEndContextDependency ";
	}
	catch (const std::exception &e) {
		std::cerr << e.what();
		return -1;
	}
}

