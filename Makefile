# 编译器和选项
CC = gcc
CFLAGS = -Wall -std=c11 -I./src
LDFLAGS = -lSDL3 -lSDL3_image

# 源文件列表 (在这里添加所有新创建的 .c 文件)
SRCS = ./src/main.c \
       ./src/renderer.c \
       ./src/player.c \
       ./src/camera.c \
       ./src/tile.c \
       ./src/map.c

# 输出文件
TARGET = ./out/sdl_game.exe

# 默认指令
all: build

# 编译指令
build:
	@if not exist "out" mkdir out
	$(CC) $(SRCS) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

# 运行指令
run:
	$(TARGET)

# 清理指令
clean:
	del /q out\*.exe

.PHONY: all build run clean