# Copyright (C)2020, 2024 International Business Machines Corporation and  
# others. All Rights Reserved.                        
.PHONY: build all distributed clean

# Please point this to your correct eval_predicate toolkit location.
STREAMS_EVAL_PREDICATE_TOOLKIT ?= $(PWD)/../../com.ibm.streamsx.eval_predicate

ifeq ($(STREAMS_STUDIO_BUILDING), 1)
    $(info Building from Streams Studio, use env vars set by studio)
    SPLC = $(STREAMS_STUDIO_SC_PATH)
    DATA_DIR = $(STREAMS_STUDIO_DATA_DIRECTORY)
    OUTPUT_DIR = $(STREAMS_STUDIO_OUTPUT_DIRECTORY)
    TOOLKIT_PATH = $(STREAMS_STUDIO_SPL_PATH)
else
    $(info build use env settings)
    ifndef STREAMS_INSTALL
        $(error require streams environment STREAMS_INSTALL)
    endif
    SPLC = $(STREAMS_INSTALL)/bin/sc
    DATA_DIR = data
    OUTPUT_DIR = output/com.ibm.streamsx.eval_predicate.test.EvalPredicateExample/BuildConfig
    TOOLKIT_PATH = $(STREAMS_EVAL_PREDICATE_TOOLKIT)
endif

SPL_MAIN_COMPOSITE = com.ibm.streamsx.eval_predicate.test::EvalPredicateExample
SPLC_FLAGS = -a 
SPL_CMD_ARGS ?=

build: distributed

all: clean build

distributed:
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE) -t ${TOOLKIT_PATH} --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR) $(SPL_CMD_ARGS)

clean:
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE) -t ${TOOLKIT_PATH} --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR) -C $(SPL_CMD_ARGS)

	rm -rf output
