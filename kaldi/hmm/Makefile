all:


include ../kaldi.mk

TESTFILES = hmm-topology-test hmm-utils-test transition-model-test posterior-test

OBJFILES = hmm-topology-2D.o transition-model-2D.o tree-accu-2D.o \
        hmm-topology.o transition-model.o hmm-utils.o tree-accu.o \
        posterior.o hmm-test-utils.o

LIBNAME = kaldi-hmm
ADDLIBS = ../tree/kaldi-tree.a ../util/kaldi-util.a ../matrix/kaldi-matrix.a \
          ../base/kaldi-base.a 

include ../makefiles/default_rules.mk

