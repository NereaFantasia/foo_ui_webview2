#!/usr/bin/env node
/**
 * postbuild.js - Node.js 版本，跨平台兼容
 * 修复 VitePress 生成的绝对路径为相对路径，支持 SPA 部署
 */

const fs = require('fs');
const path = require('path');

const DIST_DIR = 'dist';

if (!fs.existsSync(DIST_DIR)) {
  console.error(`错误: ${DIST_DIR} 目录不存在，请先运行 'npm run build'`);
  process.exit(1);
}

console.log(`正在处理 ${DIST_DIR} 文件以支持 SPA 路由...`);

/**
 * 递归处理目录中的所有 HTML 文件
 */
function processDirectory(dir) {
  const files = fs.readdirSync(dir);
  
  for (const file of files) {
    const filePath = path.join(dir, file);
    const stat = fs.statSync(filePath);
    
    if (stat.isDirectory()) {
      processDirectory(filePath);
    } else if (file.endsWith('.html')) {
      processHtmlFile(filePath);
    }
  }
}

/**
 * 处理单个 HTML 文件
 */
function processHtmlFile(filePath) {
  // 计算文件相对于 dist 的深度
  const relPath = path.relative(DIST_DIR, filePath);
  const depth = relPath.split(path.sep).length - 1;
  
  // 生成相对路径前缀
  const prefix = depth === 0 ? './' : '../'.repeat(depth);
  
  let content = fs.readFileSync(filePath, 'utf8');
  
  // 修复 href="/path" → href="./path" 或 "../path"
  content = content.replace(/href="\/([^"]*)"/g, (match, p1) => {
    if (p1 === '') return `href="${prefix}index.html"`; // href="/" → href="./index.html"
    return `href="${prefix}${p1}"`;
  });
  
  // 修复 src="/path" → src="./path" 或 "../path"
  content = content.replace(/src="\/([^"]*)"/g, (match, p1) => {
    return `src="${prefix}${p1}"`;
  });
  
  // 注意：删除脚本标签只用于 file:// 直接打开
  // HTTP 服务（如 CloudBase）不需要删除，保留所有交互功能
  
  fs.writeFileSync(filePath, content, 'utf8');
}

try {
  processDirectory(DIST_DIR);
  console.log('? 完成!');
  console.log('提示: 搜索功能需要 HTTP 服务器 (npm run preview) 或部署到云平台。');
} catch (error) {
  console.error('? 处理失败:', error.message);
  process.exit(1);
}
