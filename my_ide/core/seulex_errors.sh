#!/usr/bin/env bash
# test_seuyacc_errors.sh - 使用 seuyacc 测试错误恢复

set -euo pipefail

root_dir=$(cd "$(dirname "$0")" && pwd)
build_dir="$root_dir/build_seuyacc_error_test"
rm -rf "$build_dir"
mkdir -p "$build_dir"

echo "=== 使用 SeuYacc 测试错误恢复 ==="
echo ""

# 复制文件
cp "$root_dir/examples/minic.y" "$build_dir/"
cp "$root_dir/examples/minic.l" "$build_dir/"

# echo "步骤 1: 使用 seuyacc 生成 parser..."
cd "$build_dir"
"$root_dir/seuyacc" --definitions minic.y

echo "Running lex..."
"$root_dir/seulex" "$build_dir/minic.l"
cp "$build_dir/lex.yy.c" "$build_dir/minic_lex.yy.c"
rm -rf "$build_dir/lex.yy.c"


echo ""
echo "步骤 3: 编译 parser..."
if c++ -I. minic.tab.c minic_lex.yy.c -o minic_parser 2>/dev/null; then
    echo "✓ 使用 C++ 编译成功"
elif cc -I. minic.tab.c minic_lex.yy.c -o minic_parser 2>/dev/null; then
    echo "✓ 使用 C 编译成功"
else
    echo "✗ 编译失败"
    exit 1
fi

echo ""
echo "步骤 4: 测试错误文件..."

# 查找所有 error_*.c 文件
error_files=("$root_dir"/examples/error_*.c)

if [ -e "${error_files[0]}" ]; then
    for error_file in "${error_files[@]}"; do
        filename=$(basename "$error_file")
        name="${filename%.c}"
        json_file="$build_dir/${name}.json"
        
        echo ""
        echo "测试: $filename"
        echo "---"
        log_file="$build_dir/${name}.log"

        # gstdbuf -oL -eL ./minic_parser < "$error_file" 2>&1 | \
        #     tee "$log_file" | \
        #     sed -n '/{/,/^}/p' > "$json_file" || true


        ./minic_parser < "$error_file" 2>&1 | \
            tee "$log_file" | \
            sed -n '/{/,/^}/p' > "$json_file" || true

        echo "✓ 诊断信息已生成: $json_file"
        
        # 显示 JSON 内容
        if [ -f "$json_file" ] && [ -s "$json_file" ]; then
            cat "$json_file"
        else
            echo "{\"errors\": [], \"errorCount\": 0}"
        fi
    done
else
    echo "⚠ 没有找到 error_*.c 测试文件"
fi

echo ""
echo "=== 测试完成 ==="
echo "构建目录: $build_dir"
