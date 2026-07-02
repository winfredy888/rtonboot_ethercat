#!/bin/bash

# 目标目录
TARGET_DIR="/mnt/hgfs/todesktop/ref_kernel/cmp_stmmmac/3588"

# 创建目标目录（如果不存在）
mkdir -p "$TARGET_DIR"

# 查找所有 .o 文件
find . -type f -name "*.o" | while read -r obj_file; do
    # 提取 .o 文件的路径（不包括扩展名）
    base_name=$(basename "$obj_file" .o)
    dir_name=$(dirname "$obj_file")

    # 查找对应的 .c 文件
    c_file=$(find "$dir_name" -type f -name "$base_name.c")

    # 如果找到对应的 .c 文件，则拷贝到目标目录
    if [ -f "$c_file" ]; then
        cp "$c_file" "$TARGET_DIR"
        echo "Copied: $c_file -> $TARGET_DIR"
    else
        echo "No corresponding .c file found for: $obj_file"
    fi
done

echo "Copy completed!"
