#!/bin/bash
# postbuild.sh - Linux/Docker 版本，修复相对路径支持 SPA 部署

set -e

DIST_DIR="dist"
if [ ! -d "$DIST_DIR" ]; then
    echo "错误: dist 目录不存在，请先运行 'npm run build'"
    exit 1
fi

echo "正在处理 $DIST_DIR 文件以支持 SPA 路由..."

# 查找所有 HTML 文件并修复绝对路径为相对路径
find "$DIST_DIR" -name "*.html" -type f | while read file; do
    # 计算文件深度
    depth=$(echo "$file" | tr -cd '/' | wc -c)
    depth=$((depth - 1))  # 去掉 dist 本身的深度
    
    # 生成相对路径前缀
    if [ $depth -eq 0 ]; then
        prefix="./"
    else
        prefix=$(printf '../%.0s' $(seq 1 $depth))
    fi
    
    # 备份原文件
    cp "$file" "$file.bak"
    
    # 修复绝对路径
    sed -i "s|href=\"/|href=\"${prefix}|g" "$file"
    sed -i "s|src=\"/|src=\"${prefix}|g" "$file"
    
    # 移除 ES modules (file:// 下被 CORS 阻止)
    sed -i '/<script type="module"[^>]*><\/script>/d' "$file"
    
    # 移除 modulepreload
    sed -i '/<link rel="modulepreload"[^>]*\/?>/d' "$file"
    
    # 移除字体 preload (file:// 下 CORS 问题)
    sed -i '/<link rel="preload" href="[^"]*\.woff2"[^>]*\/?>/d' "$file"
    
    rm "$file.bak"
done

echo "完成!"
echo "提示: 搜索功能需要 HTTP 服务器 (npm run preview) 或部署到云平台。"
