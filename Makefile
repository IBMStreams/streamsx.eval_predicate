# Copyright (C)2020, 2021 International Business Machines Corporation and  
# others. All Rights Reserved.                        
.PHONY: build all distributed clean

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
    OUTPUT_DIR1 = output/com.ibm.streamsx.eval_predicate.EvalPredicateExample/BuildConfig
    OUTPUT_DIR2 = output/com.ibm.streamsx.eval_predicate.FunctionalTests/BuildConfig
    TOOLKIT_PATH = 
endif

SPL_MAIN_COMPOSITE1 = com.ibm.streamsx.eval_predicate::EvalPredicateExample
SPL_MAIN_COMPOSITE2 = com.ibm.streamsx.eval_predicate::FunctionalTests
SPLC_FLAGS = -a 
SPL_CMD_ARGS ?=

build: distributed

all: clean build

distributed:
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE1) --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR1) $(SPL_CMD_ARGS)
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE2) --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR2) $(SPL_CMD_ARGS)

clean:
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE1) --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR1) -C $(SPL_CMD_ARGS)
	$(SPLC) $(SPLC_FLAGS) -M $(SPL_MAIN_COMPOSITE2) --data-dir $(DATA_DIR) --output-dir $(OUTPUT_DIR2) -C $(SPL_CMD_ARGS)

	rm -rf output
