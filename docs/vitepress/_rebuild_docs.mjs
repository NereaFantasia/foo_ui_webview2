/**
 * Rebuild VitePress markdown source files from dist/assets/*.js
 * 
 * Usage: node _rebuild_docs.mjs
 * 
 * Handles two VitePress output patterns:
 * 1. Simple pages: s[0]||(s[0]=[t(`...HTML...`,N)])
 * 2. Complex pages with Vue components: t[N]||(t[N]=e('...HTML...',N))
 *    These have multiple static chunks interspersed with dynamic component calls
 */

import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'fs';
import { join, dirname } from 'path';

const DIST_ASSETS = join(import.meta.dirname, 'dist', 'assets');
const OUTPUT_ROOT = import.meta.dirname;

// Get all non-lean JS files that correspond to .md pages
const assetFiles = readdirSync(DIST_ASSETS)
  .filter(f => f.endsWith('.js') && !f.endsWith('.lean.js') && f.includes('.md.'));

console.log(`Found ${assetFiles.length} asset files to process`);

function extractFromAsset(filename) {
  const content = readFileSync(join(DIST_ASSETS, filename), 'utf-8');
  
  // Extract frontmatter from JSON.parse('...')
  const jsonMatch = content.match(/const \w+=JSON\.parse\('(.+?)'\)/);
  let frontmatter = {};
  if (jsonMatch) {
    try {
      frontmatter = JSON.parse(jsonMatch[1].replace(/\\'/g, "'"));
    } catch(e) {
      console.warn(`  Warning: Could not parse frontmatter JSON for ${filename}`);
    }
  }
  
  // Strategy: Find all HTML content chunks in the file
  // VitePress uses patterns like:
  //   functionName(`<html>...`,N)  - backtick template
  //   functionName('<html>...',N)  - single-quote string
  // where functionName is a single letter (a, e, t, etc.)
  // The key identifier is that the content starts with '<h' or '<' HTML tags
  
  const chunks = [];
  
  // Pattern A: Find backtick templates containing HTML
  // Look for: [funcName(`<...`,N)] or someVar=funcName(`<...`,N)
  // The content may contain backticks (in code blocks), so we can't use simple regex
  // Instead, find the start pattern and then look for the end pattern: `,N)]` or `,N))]`
  const backtickStartPattern = /[\[=(]\w\(`/g;
  let match;
  while ((match = backtickStartPattern.exec(content)) !== null) {
    const chunkStart = match.index + match[0].length;
    // Check if it contains HTML (starts with < or contains HTML tags)
    const preview = content.substring(chunkStart, chunkStart + 50);
    if (!preview.includes('<')) continue;
    
    // Find the end: look for backtick followed by ,N) pattern
    let endPos = -1;
    let searchPos = chunkStart;
    while (searchPos < content.length) {
      const btIdx = content.indexOf('`', searchPos);
      if (btIdx < 0) break;
      // Check if followed by ,N) pattern
      const after = content.substring(btIdx + 1, btIdx + 15);
      if (/^,\d+\)/.test(after)) {
        endPos = btIdx;
        break;
      }
      searchPos = btIdx + 1;
    }
    
    if (endPos > chunkStart && (endPos - chunkStart) > 20) {
      const chunk = content.substring(chunkStart, endPos);
      chunks.push({ index: match.index, content: chunk, type: 'backtick' });
    }
  }
  
  // Pattern B: Find single-quote strings containing HTML
  // Look for: someVar=funcName('<...>...',N)
  // Need to handle escaped quotes inside
  {
    const singleQuoteStarts = [];
    // Find all occurrences of =a(' or =e(' or =t(' or [a(' or (a(' etc. followed by <
    const sqPattern = /[\[=(](\w)\('/g;
    while ((match = sqPattern.exec(content)) !== null) {
      const afterQuote = content.substring(match.index + match[0].length, match.index + match[0].length + 5);
      if (afterQuote.startsWith('<')) {
        singleQuoteStarts.push(match.index + match[0].length);
      }
    }
    
    for (const startPos of singleQuoteStarts) {
      // Parse until we find an unescaped closing single quote
      let pos = startPos;
      let escaped = false;
      let endPos = -1;
      while (pos < content.length) {
        const ch = content[pos];
        if (escaped) {
          escaped = false;
          pos++;
          continue;
        }
        if (ch === '\\') {
          escaped = true;
          pos++;
          continue;
        }
        if (ch === "'") {
          // Check if followed by ,N) or ))
          const after = content.substring(pos + 1, pos + 15);
          if (/^,\d+\)/.test(after) || /^\)/.test(after)) {
            endPos = pos;
            break;
          }
          // Not the end, continue
        }
        pos++;
      }
      
      if (endPos > startPos && (endPos - startPos) > 20) {
        let chunk = content.substring(startPos, endPos);
        // Unescape
        chunk = chunk.replace(/\\'/g, "'");
        chunk = chunk.replace(/\\"/g, '"');
        chunk = chunk.replace(/\\n/g, '\n');
        chunk = chunk.replace(/\\\\/g, '\\');
        chunks.push({ index: startPos, content: chunk, type: 'singlequote' });
      }
    }
  }
  
  // Sort chunks by position and deduplicate overlapping chunks
  chunks.sort((a, b) => a.index - b.index);
  // Remove duplicates (same position or overlapping)
  const deduped = [];
  for (const chunk of chunks) {
    if (deduped.length === 0) {
      deduped.push(chunk);
    } else {
      const last = deduped[deduped.length - 1];
      // Skip if this chunk starts within the previous chunk's range
      if (chunk.index > last.index + last.content.length) {
        deduped.push(chunk);
      } else if (chunk.content.length > last.content.length) {
        // Replace with longer chunk if overlapping
        deduped[deduped.length - 1] = chunk;
      }
    }
  }
  const html = deduped.map(c => c.content).join('\n');
  
  return { frontmatter, html, relativePath: frontmatter.relativePath || '' };
}

function htmlToMarkdown(html) {
  if (!html) return '';
  
  let md = html;
  
  // Decode HTML entities first
  md = md.replace(/&#39;/g, "'")
    .replace(/&quot;/g, '"')
    .replace(/&lt;/g, '<')
    .replace(/&gt;/g, '>')
    .replace(/&amp;/g, '&')
    .replace(/&nbsp;/g, ' ');
  
  // Remove VitePress header anchors
  md = md.replace(/<a class="header-anchor"[^>]*>​<\/a>/g, '');
  
  // Remove line-numbers-wrapper divs entirely
  md = md.replace(/<div class="line-numbers-wrapper"[^>]*>[\s\S]*?<\/div>/g, '');
  
  // Remove copy buttons
  md = md.replace(/<button[^>]*class="copy"[^>]*>.*?<\/button>/g, '');
  md = md.replace(/<button[^>]*title="Copy Code"[^>]*>.*?<\/button>/g, '');
  
  // Convert code blocks - VitePress format
  md = md.replace(/<div class="language-([\w-]+)[^"]*"[^>]*>(?:<button[^>]*>.*?<\/button>)?(?:<span class="lang">[^<]*<\/span>)?<pre[^>]*><code>([\s\S]*?)<\/code><\/pre>(?:<div[^>]*>[\s\S]*?<\/div>)?<\/div>/g, (match, lang, code) => {
    let cleanCode = code;
    // Split by line spans - each <span class="line"> is one line
    // Replace the line boundary pattern with newlines
    cleanCode = cleanCode.replace(/<\/span><span class="line">/g, '\n');
    // Remove the opening and closing line span
    cleanCode = cleanCode.replace(/^<span class="line">/, '');
    cleanCode = cleanCode.replace(/<\/span>$/, '');
    // Remove all remaining span tags (syntax highlighting)
    cleanCode = cleanCode.replace(/<span[^>]*>/g, '');
    cleanCode = cleanCode.replace(/<\/span>/g, '');
    // Decode entities in code
    cleanCode = cleanCode
      .replace(/&lt;/g, '<')
      .replace(/&gt;/g, '>')
      .replace(/&amp;/g, '&')
      .replace(/&quot;/g, '"')
      .replace(/&#39;/g, "'");
    // Remove any remaining HTML tags
    cleanCode = cleanCode.replace(/<[^>]+>/g, '');
    // Clean up
    cleanCode = cleanCode.replace(/^\n+/, '').replace(/\n+$/, '');
    
    return `\n\n\`\`\`${lang}\n${cleanCode}\n\`\`\`\n\n`;
  });
  
  // Convert custom blocks (tip, warning, danger, info) BEFORE paragraphs
  md = md.replace(/<div class="(\w+) custom-block">([\s\S]*?)<\/div>/g, (match, type, content) => {
    const titleMatch = content.match(/<p class="custom-block-title">(.*?)<\/p>/);
    const body = content.replace(/<p class="custom-block-title">.*?<\/p>/, '').replace(/<\/?p>/g, '').trim();
    const title = titleMatch ? titleMatch[1] : type.toUpperCase();
    return `\n\n::: ${type} ${title}\n${body}\n:::\n\n`;
  });
  
  // Convert tables
  md = md.replace(/<table[^>]*>([\s\S]*?)<\/table>/g, (match, tableContent) => {
    const rows = [];
    const headerMatch = tableContent.match(/<thead>([\s\S]*?)<\/thead>/);
    const bodyMatch = tableContent.match(/<tbody>([\s\S]*?)<\/tbody>/);
    
    if (headerMatch) {
      const cells = [];
      headerMatch[1].replace(/<th[^>]*>([\s\S]*?)<\/th>/g, (m, cell) => {
        cells.push(cell.replace(/<[^>]+>/g, '').trim());
      });
      if (cells.length > 0) {
        rows.push('| ' + cells.join(' | ') + ' |');
        rows.push('| ' + cells.map(() => '---').join(' | ') + ' |');
      }
    }
    
    if (bodyMatch) {
      const trMatches = bodyMatch[1].match(/<tr>([\s\S]*?)<\/tr>/g) || [];
      for (const tr of trMatches) {
        const cells = [];
        tr.replace(/<td[^>]*>([\s\S]*?)<\/td>/g, (m, cell) => {
          cells.push(cell.replace(/<[^>]+>/g, '').trim());
        });
        if (cells.length > 0) {
          rows.push('| ' + cells.join(' | ') + ' |');
        }
      }
    }
    
    return '\n\n' + rows.join('\n') + '\n\n';
  });
  
  // Convert blockquotes
  md = md.replace(/<blockquote>([\s\S]*?)<\/blockquote>/g, (match, content) => {
    const text = content.replace(/<\/?p>/g, '').trim();
    const lines = text.split('\n');
    return '\n\n' + lines.map(l => `> ${l.trim()}`).join('\n') + '\n\n';
  });
  
  // Convert headers - ensure newlines before and after
  md = md.replace(/<h1[^>]*>(.*?)<\/h1>/g, '\n\n# $1\n\n');
  md = md.replace(/<h2[^>]*>(.*?)<\/h2>/g, '\n\n## $1\n\n');
  md = md.replace(/<h3[^>]*>(.*?)<\/h3>/g, '\n\n### $1\n\n');
  md = md.replace(/<h4[^>]*>(.*?)<\/h4>/g, '\n\n#### $1\n\n');
  md = md.replace(/<h5[^>]*>(.*?)<\/h5>/g, '\n\n##### $1\n\n');
  
  // Convert inline code (must be before other inline conversions)
  md = md.replace(/<code>(.*?)<\/code>/g, '`$1`');
  
  // Convert bold
  md = md.replace(/<strong>(.*?)<\/strong>/g, '**$1**');
  
  // Convert italic/em
  md = md.replace(/<em>(.*?)<\/em>/g, '*$1*');
  
  // Convert links
  md = md.replace(/<a[^>]*href="([^"]*)"[^>]*>(.*?)<\/a>/g, '[$2]($1)');
  
  // Convert unordered lists
  md = md.replace(/<ul>([\s\S]*?)<\/ul>/g, (match, content) => {
    let result = '\n';
    content.replace(/<li>([\s\S]*?)<\/li>/g, (m, item) => {
      result += `- ${item.replace(/<[^>]+>/g, '').trim()}\n`;
    });
    return result + '\n';
  });
  
  // Convert ordered lists
  md = md.replace(/<ol[^>]*>([\s\S]*?)<\/ol>/g, (match, content) => {
    let result = '\n';
    let counter = 0;
    content.replace(/<li>([\s\S]*?)<\/li>/g, (m, item) => {
      counter++;
      result += `${counter}. ${item.replace(/<[^>]+>/g, '').trim()}\n`;
    });
    return result + '\n';
  });
  
  // Convert paragraphs
  md = md.replace(/<p>([\s\S]*?)<\/p>/g, '\n\n$1\n\n');
  
  // Convert <br> tags
  md = md.replace(/<br\s*\/?>/g, '\n');
  
  // Remove remaining HTML tags (but preserve custom elements like <fb-xxx>)
  // First, wrap custom elements in backticks to preserve them
  md = md.replace(/<(fb-[\w-]+)([^>]*)>/g, '`<$1$2>`');
  md = md.replace(/<\/(fb-[\w-]+)>/g, '`</$1>`');
  // Now remove remaining HTML tags
  md = md.replace(/<\/?[^>]+>/g, '');
  
  // Clean up whitespace
  md = md.replace(/\n{3,}/g, '\n\n');
  md = md.trim();
  
  return md;
}

function buildFrontmatter(fm) {
  if (!fm || !fm.frontmatter) return '';
  const data = fm.frontmatter;
  if (!data || Object.keys(data).length === 0) return '';
  
  // For home layout pages
  if (data.layout === 'home') {
    let yaml = '---\n';
    yaml += 'layout: home\n';
    if (data.hero) {
      yaml += 'hero:\n';
      yaml += `  name: "${data.hero.name}"\n`;
      yaml += `  text: "${data.hero.text}"\n`;
      yaml += `  tagline: "${data.hero.tagline}"\n`;
      if (data.hero.actions) {
        yaml += '  actions:\n';
        for (const action of data.hero.actions) {
          yaml += `    - theme: ${action.theme}\n`;
          yaml += `      text: "${action.text}"\n`;
          yaml += `      link: ${action.link}\n`;
        }
      }
    }
    if (data.features) {
      yaml += 'features:\n';
      for (const feat of data.features) {
        yaml += `  - icon:\n`;
        yaml += `      src: ${feat.icon.src}\n`;
        yaml += `    title: "${feat.title}"\n`;
        yaml += `    details: "${feat.details}"\n`;
      }
    }
    yaml += '---\n';
    return yaml;
  }
  
  return '';
}

// Process each asset file
let successCount = 0;
let emptyCount = 0;

for (const file of assetFiles) {
  const { frontmatter, html, relativePath } = extractFromAsset(file);
  
  if (!relativePath) {
    console.warn(`  Skipping ${file}: no relativePath found`);
    continue;
  }
  
  const outputPath = join(OUTPUT_ROOT, relativePath);
  const outputDir = dirname(outputPath);
  
  // Ensure directory exists
  if (!existsSync(outputDir)) {
    mkdirSync(outputDir, { recursive: true });
  }
  
  // Build the markdown content
  let mdContent = '';
  
  // Add frontmatter if needed
  const fm = buildFrontmatter({ frontmatter: frontmatter.frontmatter });
  if (fm) {
    mdContent = fm + '\n';
  }
  
  // Convert HTML to markdown
  const markdown = htmlToMarkdown(html);
  mdContent += markdown;
  
  if (!markdown && !fm) {
    emptyCount++;
    console.warn(`  ⚠ ${relativePath} (empty content)`);
  } else {
    successCount++;
    console.log(`  ✓ ${relativePath} (${markdown.length} chars)`);
  }
  
  // Ensure file ends with newline
  if (!mdContent.endsWith('\n')) {
    mdContent += '\n';
  }
  
  writeFileSync(outputPath, mdContent, 'utf-8');
}

console.log(`\nDone! ${successCount} files rebuilt, ${emptyCount} empty.`);
