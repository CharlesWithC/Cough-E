
CC 			=	ccache g++
APP 		= 	cough-e
BUILD_DIR 	= 	build
.DEFAULT_GOAL = all

CFLAGS_MARKER = $(BUILD_DIR)/.last_cflags
ifneq ($(shell cat $(CFLAGS_MARKER) 2>/dev/null),$(CFLAGS))
.PHONY: $(CFLAGS_MARKER)
endif

SRC_DIRS 	= 	Src kiss_fftr input_data FxP/imu FxP/audio
SRC 		= 	main.cpp $(foreach c, $(SRC_DIRS), $(wildcard $c/*.c*))

OBJS          = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(filter %.cpp, $(SRC)))
OBJS         += $(patsubst %.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SRC)))

INC_DIRS 	= 	Inc FxP universal/include/sw libposit/include ./ kiss_fftr precision_analysis range_analysis
INC 		= 	$(foreach d, $(INC_DIRS), -I$d)

TWIDDLE_GENERATOR = kiss_fftr/gen_twiddles_fixed.py
TWIDDLE_Q15 = kiss_fftr/twiddles_win08_fs8000_q15.h
TWIDDLE_Q31 = kiss_fftr/twiddles_win08_fs8000_q31.h

MATH        =    -lm
CFLAGS      +=   -Wall -O3 -g

COMP_STRING = 	" |++++++++++++++++++++++++++++++++|\n"
COMP_STRING +=  "|            Cough-E             |\n"
COMP_STRING += 	"|      Compilation successful!   |\n"
COMP_STRING += 	"|++++++++++++++++++++++++++++++++|\n"
COMP_STRING += 	"|              ( (               |\n"
COMP_STRING += 	"|              ) )               |\n"
COMP_STRING += 	"|           ........             |\n"
COMP_STRING += 	"|           |      |]            |\n"
COMP_STRING += 	"|           \      /             |\n"
COMP_STRING += 	"|            '----'              |\n"
COMP_STRING += 	"|++++++++++++++++++++++++++++++++|\n"

# @echo "Compilation terminated successfully! ☕\n"
# @echo "Running with command:"

all: $(TWIDDLE_Q15) $(TWIDDLE_Q31) $(BUILD_DIR)/$(APP)

$(CFLAGS_MARKER):
	@mkdir -p $(dir $@)
	@echo "$(CFLAGS)" > $@

$(BUILD_DIR)/$(APP): $(OBJS)
	$(CC) -o $@ $(OBJS) $(MATH) $(CFLAGS)
	@echo -e $(COMP_STRING)

$(BUILD_DIR)/%.o: %.cpp $(CFLAGS_MARKER)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(BUILD_DIR)/%.o: %.c $(CFLAGS_MARKER)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

run: all
	@echo $(EXEC_STRING)
	@./$(BUILD_DIR)/$(APP)

$(TWIDDLE_Q15) $(TWIDDLE_Q31): $(TWIDDLE_GENERATOR)
	python3 $(TWIDDLE_GENERATOR) --formats q15 q31 --out-dir kiss_fftr

submodule:
	git submodule update --init
	git submodule add -f https://github.com/stillwater-sc/universal universal
	git submodule add -f https://github.com/CharlesWithC/libposit libposit
	cd universal && git sparse-checkout init --no-cone && git sparse-checkout set sparse-checkout set /include/sw
	cd libposit && git sparse-checkout init --no-cone && git sparse-checkout set sparse-checkout set /include

clean:
	rm -rf $(BUILD_DIR)
