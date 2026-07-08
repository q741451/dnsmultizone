# dnsmultizone Makefile
# 默认 `make` 即可在本机静态编译出可执行文件 dnsmultizone
# 交叉编译示例:
#   make CXX=aarch64-linux-musl-g++ LDFLAGS="-static -s"
# ----------------------------------
NAME        = dnsmultizone
VERSION     = 1.0.0
# ----------------------------------

SRC_DIR     = src
BUILD_DIR   = build
BIN         = $(NAME)

SRCS        = $(wildcard $(SRC_DIR)/*.cpp)
OBJS        = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# 允许通过环境变量/命令行覆盖，便于交叉编译
CXX         ?= g++
CXXFLAGS    ?= -Wall -std=c++11
CXXFLAGS    += -I$(SRC_DIR) -Wno-format-security
CPPFLAGS    ?= -O2
LDFLAGS     ?= -static
LIBS        ?=

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CPPFLAGS) $(OBJS) -o $@ $(LIBS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	$(RM) -r $(BUILD_DIR) $(BIN)
