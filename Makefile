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
CPPFLAGS    ?= -Os
LDFLAGS     ?= -static
LIBS        ?=

# 静态链接体积优化，赋空值即可关闭：make SIZE_CXXFLAGS= SIZE_LDFLAGS=
#   本项目不使用异常，也没有向下转型（两处向上转型用 static_pointer_cast
#   即可），因此可以关掉异常与 RTTI；配合按函数/数据分节和链接期回收，
#   未被引用的代码不会进入结果。另见 src/CxxRuntime.cpp。
SIZE_CXXFLAGS ?= -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables \
                 -ffunction-sections -fdata-sections
SIZE_LDFLAGS  ?= -Wl,--gc-sections -Wl,--build-id=none

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CPPFLAGS) $(OBJS) -o $@ $(LIBS) $(LDFLAGS) $(SIZE_LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(SIZE_CXXFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	$(RM) -r $(BUILD_DIR) $(BIN)
