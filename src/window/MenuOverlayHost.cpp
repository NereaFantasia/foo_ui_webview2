#include "pch.h"
#include "window/MenuOverlayHost.h"
#include "window/WindowManager.h"
#include "core/WebViewContext.h"
#include "api/BridgeCore.h"
#include "webview/WebViewHost.h"

// ============================================
// 自绘菜单覆盖面宿主实现（Self-Drawn Menu：渲染器 + 子菜单 + 键盘导航 WAI-ARIA APG）
// ============================================

namespace {
    std::wstring BuildMenuOverlayHtml() {
        return LR"MENUHTML(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style id="fb-default">
  html,body{margin:0;padding:0;width:100%;height:100%;overflow:hidden;background:transparent;font:13px "Segoe UI",sans-serif;}
  #block{position:fixed;inset:0;}
  .fb-menu{position:fixed;min-width:170px;max-height:100vh;overflow:auto;background:#2b2b2b;color:#eaeaea;border:1px solid #555;border-radius:6px;padding:4px 0;box-shadow:0 6px 20px rgba(0,0,0,.45);display:none;z-index:10;}
  .fb-item{display:flex;align-items:center;padding:6px 30px 6px 26px;cursor:default;white-space:nowrap;position:relative;}
  .fb-item:hover,.fb-item.active{background:#3d6fd0;color:#fff;}
  .fb-item.disabled{color:#777;}
  .fb-item.disabled:hover,.fb-item.disabled.active{background:transparent;color:#777;}
  .fb-item.checked::before{content:"\2713";position:absolute;left:8px;}
  .fb-arrow{position:absolute;right:8px;font-size:10px;opacity:.85;}
  /* Inline monochrome icon column (普通项；仅当本层有项带 iconSvg 时出现，保证文字左缘对齐)。*/
  .fb-item-ico{flex:0 0 16px;width:16px;height:16px;margin-right:8px;display:inline-flex;align-items:center;justify-content:center;}
  .fb-item-ico svg{width:16px;height:16px;display:block;fill:currentColor;}
  .fb-menu.has-icons .fb-item.nrm{padding-left:8px;}
  /* 勾选项的 ✓ 落在图标列（left:8px）；若该项自带图标则隐藏其图标，避免 ✓ 与图标重叠。*/
  .fb-menu.has-icons .fb-item.nrm.checked .fb-item-ico{visibility:hidden;}
  .fb-sep{height:1px;background:#555;margin:4px 0;}
  #probe{position:fixed;left:8px;top:8px;padding:4px 8px;font-size:11px;color:#fff;background:rgba(0,0,0,.5);border-radius:5px;pointer-events:none;opacity:.5;}
  /* ContentSized：去全覆盖拦截层；根菜单入场动画（参 QQNT，opacity+transform）。子菜单纯 CSS 在固定窗内右展，窗口全程不 resize。*/
  body.content-sized #block{display:none;}
  body.content-sized #probe{display:none;}
  body.content-sized #menu{opacity:0;transform:translateY(4px) scale(.985);transition:opacity .13s ease-out,transform .13s ease-out;}
  body.content-sized #menu.in{opacity:1;transform:none;}
  /* 退场动画（opt-in，镜像入场）：收到 menu:__hide 时 #menu 去 .in 加 .out 播淡出；前端可经 css 覆盖 #menu.out。*/
  body.content-sized #menu.out{opacity:0;transform:translateY(4px) scale(.985);transition:opacity .13s ease-out,transform .13s ease-out;}
  /* 富菜单项（仅 webview 自绘）：nowplaying 歌曲卡 / rating 星级 / slider 音量滑块。*/
  .fb-np{display:flex;align-items:center;gap:10px;padding:8px 12px;cursor:default;}
  .fb-np-cover{width:40px;height:40px;flex:0 0 auto;border-radius:4px;background:#444 center/cover no-repeat;}
  .fb-np-text{min-width:0;overflow:hidden;}
  .fb-np-title{font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:240px;}
  .fb-np-sub{margin-top:2px;opacity:.65;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:240px;}
  .fb-rating,.fb-slider{display:flex;align-items:center;gap:8px;}
  .fb-rating-label,.fb-slider-label{opacity:.85;}
  .fb-stars{margin-left:auto;display:inline-flex;gap:2px;}
  .fb-star{color:#666;cursor:pointer;font-size:15px;line-height:1;}
  .fb-star.on{color:#ffcc44;}
  .fb-slider-track{position:relative;flex:1 1 auto;min-width:96px;height:4px;margin:0 2px;background:#555;border-radius:2px;cursor:pointer;touch-action:none;}
  .fb-slider-fill{position:absolute;left:0;top:0;height:100%;background:#3d6fd0;border-radius:2px;}
  .fb-slider-thumb{position:absolute;top:50%;width:10px;height:10px;margin-left:-5px;border-radius:50%;background:#fff;transform:translateY(-50%);box-shadow:0 0 2px rgba(0,0,0,.5);}
  .fb-slider-val{flex:0 0 auto;min-width:26px;text-align:right;opacity:.8;}
  /* segmented 分段单选（仅 webview 自绘）：一行互斥选项；选中段用强调色高亮。各段可放 svg 图标或文字。*/
  .fb-seg{display:flex;align-items:center;gap:10px;}
  .fb-seg-label{opacity:.85;}
  .fb-seg-group{margin-left:auto;display:inline-flex;gap:2px;}
  .fb-seg-btn{display:inline-flex;align-items:center;justify-content:center;padding:2px 8px;cursor:pointer;border-radius:4px;color:#cfcfcf;}
  .fb-seg-btn svg{width:16px;height:16px;display:block;fill:currentColor;}
  .fb-seg-btn.on{background:#3d6fd0;color:#fff;}
  .fb-seg-btn.disabled{color:#666;cursor:default;}
  /* 富项 hover/active 用克制的灰底（覆盖上面 .fb-item 蓝底），保留控件辨识度。*/
  .fb-np:hover,.fb-np.active,.fb-rating:hover,.fb-rating.active,.fb-slider:hover,.fb-slider.active,.fb-seg:hover,.fb-seg.active{background:#3a3a3a;color:#eaeaea;}
</style>
<!-- 前端样式接管（S-CSS）：注入序 默认(fb-default) → 前端(fb-user) → 受保护结构层(fb-protected)。
     fb-user 由 render() 写入 st.css；cssReplace=true 时禁用 fb-default，仅留 fb-user+fb-protected。 -->
<style id="fb-user"></style>
<style id="fb-protected">
  /* 受保护结构层（measure 安全，STYLING_TAKEOVER_DESIGN §12.3 D-C）：!important 回填盒模型/定位/溢出，保根尺寸测量稳定，不接受前端覆盖。*/
  html,body{margin:0!important;padding:0!important;width:100%!important;height:100%!important;overflow:hidden!important;box-sizing:border-box!important;}
  .fb-menu{box-sizing:border-box!important;position:fixed!important;overflow:auto!important;}
  /* 仅强制【根 #menu】的 display（仅根参与窗口测量）；子菜单 .fb-menu 仍用内联 display 定位，不纳入强制，避免子菜单闪现。*/
  #menu:not(.fb-hidden){display:block!important;}
  #menu.fb-hidden{display:none!important;}
  /* 内部基础设施（debug 探针 / 全覆盖拦截层）非前端接管目标：内容尺寸窗下恒隐，防 replace 模式禁用 fb-default 后泄漏。仅 content-sized 作用域，不影响 menu.show 全屏拦截层。*/
  body.content-sized #block{display:none!important;}
  body.content-sized #probe{display:none!important;}
</style></head>
<body>
  <div id="block"></div>
  <div id="menu" class="fb-menu fb-hidden" part="root"></div>
  <div id="probe">self-drawn-menu</div>
  <script>
    var cur = null;
    var layers = [];   // index=depth: {el, rows:[{el,item,navigable,hasSub}], active}

    function dismiss(reason){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__dismiss",{reason:reason}); }catch(e){} }
    function select(id){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__select",{menuId:(cur&&cur.menuId)||"",itemId:id}); }catch(e){} }
    function valueChange(id, value){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__valueChanged",{menuId:(cur&&cur.menuId)||"",itemId:id,value:value}); }catch(e){} }
    function clampInt(v,lo,hi){ v=Math.round((+v)||0); if(v<lo)v=lo; if(v>hi)v=hi; return v; }
    function numOr(v,d){ return (typeof v==="number"&&!isNaN(v))?v:d; }
    function attrEsc(s){ return (""+s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }
    function isNormalKind(it){ return !!it && it.type!=="separator" && it.type!=="nowplaying" && it.type!=="rating" && it.type!=="slider" && it.type!=="segmented"; }   // 仅普通/子菜单项参与图标列（富项含 segmented 不参与，镜像 C++ TrayItemKindRendersIcon）

)MENUHTML" LR"MENUHTML(    function setActive(depth, idx){
      var L=layers[depth]; if(!L) return;
      if(L.active>=0 && L.rows[L.active]) L.rows[L.active].el.classList.remove("active");
      L.active=idx;
      if(idx>=0 && L.rows[idx]){ var e=L.rows[idx].el; e.classList.add("active"); try{ e.scrollIntoView({block:"nearest"}); }catch(x){} }
    }
    function firstNav(depth){ var L=layers[depth]; if(!L) return -1; for(var i=0;i<L.rows.length;i++) if(L.rows[i].navigable) return i; return -1; }
    function moveActive(depth, dir){
      var L=layers[depth]; if(!L||!L.rows.length) return;
      var n=L.rows.length, i=(L.active<0?(dir>0?-1:0):L.active);
      for(var k=0;k<n;k++){ i=(i+dir+n)%n; if(L.rows[i].navigable){ setActive(depth,i); return; } }
    }
    function closeLayersFrom(depth){
      // 固定窗设计：子菜单收起仅移除其 DOM；窗口不 resize（无闪），无需重测/缩窗。
      for(var i=layers.length-1;i>=depth;i--){ if(i>0 && layers[i] && layers[i].el && layers[i].el.parentNode) layers[i].el.parentNode.removeChild(layers[i].el); }
      if(layers.length>depth) layers.length=depth;
    }

    // ---- 富菜单项构建器（仅根层出现；均 push 一个 row 供键盘导航/active 高亮）----
    function buildNowPlaying(menuEl, L, it, en, depth){
      var d=document.createElement("div");
      d.className="fb-item fb-np"+(!en?" disabled":"");
      d.setAttribute("part","item");
      var cv=document.createElement("div"); cv.className="fb-np-cover"; cv.setAttribute("part","nowplaying-cover");
      if(it.cover){ var cvv=""+it.cover, u=null;
        if(/^data:image\/[a-z0-9.+-]+;base64,[A-Za-z0-9+\/=]+$/i.test(cvv) || /^https?:\/\/[^\s"')]+$/i.test(cvv)){ u=cvv; }   // data: 或 http(s) 白名单（排除引号/括号/空白，杜绝 url() 注入/坏图）
        else if(/^[A-Za-z0-9+\/=]+$/.test(cvv)){ u="data:image/jpeg;base64,"+cvv; }   // 裸 base64 兼容
        if(u){ cv.style.backgroundImage='url("'+u+'")'; } }
      d.appendChild(cv);
      var tx=document.createElement("div"); tx.className="fb-np-text";
      var t=document.createElement("div"); t.className="fb-np-title"; t.setAttribute("part","nowplaying-title"); t.textContent=(it.title||it.label||""); tx.appendChild(t);
      if(it.subtitle){ var sb=document.createElement("div"); sb.className="fb-np-sub"; sb.setAttribute("part","nowplaying-subtitle"); sb.textContent=it.subtitle; tx.appendChild(sb); }
      d.appendChild(tx);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"nowplaying"};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ closeLayersFrom(depth+1); setActive(depth, ridx); });
      d.addEventListener("click", function(){ if(row.navigable) select(it.id); });   // 点击=普通项：回传 {id} + 关闭
    }

    function buildRating(menuEl, L, it, en, depth){
      var d=document.createElement("div");
      d.className="fb-item fb-rating"+(!en?" disabled":"");
      d.setAttribute("part","item");
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-rating-label"; lb.textContent=it.label; d.appendChild(lb); }
      var box=document.createElement("span"); box.className="fb-stars"; box.setAttribute("part","rating-stars");
      var ctl={v:clampInt(it.value,0,5)};
      var stars=[];
      function paint(n){ for(var k=0;k<5;k++) stars[k].classList.toggle("on", k<n); }
      for(var k=0;k<5;k++){ (function(k){
        var s=document.createElement("span"); s.className="fb-star"; s.setAttribute("part","rating-star"); s.textContent="\u2605";
        s.addEventListener("mouseenter", function(){ if(en) paint(k+1); });                                  // hover 预览
        s.addEventListener("click", function(ev){ ev.stopPropagation(); if(!en) return; var nv=((k+1)===ctl.v)?0:(k+1); ctl.v=nv; paint(nv); valueChange(it.id, nv); });   // 点同星=清 0
        box.appendChild(s); stars.push(s);
      })(k); }
      box.addEventListener("mouseleave", function(){ paint(ctl.v); });                                       // 移出还原实际值
      d.appendChild(box);
      paint(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"rating",
        adjust:function(delta){ if(!en) return; var nv=clampInt(ctl.v+delta,0,5); if(nv!==ctl.v){ ctl.v=nv; paint(nv); valueChange(it.id, nv); } }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ closeLayersFrom(depth+1); setActive(depth, ridx); });
    }

    function buildSlider(menuEl, L, it, en, depth){
      var mn=numOr(it.min,0), mx=numOr(it.max,100); if(mx<=mn) mx=mn+1;
      var ctl={v:clampInt(numOr(it.value,mn),mn,mx)};
      var d=document.createElement("div");
      d.className="fb-item fb-slider"+(!en?" disabled":"");
      d.setAttribute("part","item");
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-slider-label"; lb.textContent=it.label; d.appendChild(lb); }
      var track=document.createElement("span"); track.className="fb-slider-track"; track.setAttribute("part","slider-track");
      var fill=document.createElement("span"); fill.className="fb-slider-fill"; track.appendChild(fill);
      var thumb=document.createElement("span"); thumb.className="fb-slider-thumb"; track.appendChild(thumb);
      d.appendChild(track);
      var out=document.createElement("span"); out.className="fb-slider-val"; out.setAttribute("part","slider-value"); d.appendChild(out);
      function paint(v){ var p=(v-mn)/(mx-mn); if(p<0)p=0; if(p>1)p=1; fill.style.width=(p*100)+"%"; thumb.style.left=(p*100)+"%"; out.textContent=""+v; }
      var lastSent=0, dragging=false;
      function emit(v, force){ var now=Date.now(); if(force||(now-lastSent)>=50){ lastSent=now; valueChange(it.id, v); } }   // 拖动 throttle 50ms；松手强发终值
      function valFromX(cx){ var r=track.getBoundingClientRect(); var p=(cx-r.left)/((r.width)||1); if(p<0)p=0; if(p>1)p=1; return Math.round(mn+p*(mx-mn)); }
      function setFromEvent(e, force){ if(!en) return; var nv=valFromX(e.clientX); if(nv!==ctl.v){ ctl.v=nv; paint(nv); emit(nv, force); } else if(force){ emit(nv, true); } }
      track.addEventListener("pointerdown", function(e){ if(!en) return; e.stopPropagation(); dragging=true; try{ track.setPointerCapture(e.pointerId); }catch(x){} setFromEvent(e, false); });
      track.addEventListener("pointermove", function(e){ if(dragging) setFromEvent(e, false); });
      track.addEventListener("pointerup", function(e){ if(dragging){ dragging=false; try{ track.releasePointerCapture(e.pointerId); }catch(x){} setFromEvent(e, true); } });
      track.addEventListener("pointercancel", function(){ if(dragging){ dragging=false; emit(ctl.v, true); } });
      track.addEventListener("click", function(ev){ ev.stopPropagation(); });   // 阻止冒泡到 row（不应选中/关闭）
      paint(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var step=Math.max(1, Math.round((mx-mn)/20));
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"slider",
        adjust:function(delta){ if(!en) return; var nv=clampInt(ctl.v+delta*step,mn,mx); if(nv!==ctl.v){ ctl.v=nv; paint(nv); valueChange(it.id, nv); } }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ closeLayersFrom(depth+1); setActive(depth, ridx); });
    }

)MENUHTML" LR"MENUHTML(    // segmented 分段单选：读 it.segments（互斥选项），it.value=选中段索引。点启用段→paint+valueChange（走现有 value 通道，不关菜单）；
    // Left/Right 经 row.adjust 移到上/下一个【启用】段并 valueChange（clamp 边界）。index→业务语义由前端映射。
    function buildSegmented(menuEl, L, it, en, depth){
      var d=document.createElement("div"); d.className="fb-item fb-seg"+(!en?" disabled":""); d.setAttribute("part","item");
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-seg-label"; lb.setAttribute("part","segmented-label"); lb.textContent=it.label; d.appendChild(lb); }
      var grp=document.createElement("span"); grp.className="fb-seg-group"; grp.setAttribute("part","segmented-group");
      var segs=(it.segments||[]), btns=[]; var ctl={v:(typeof it.value==="number")?it.value:-1};
      function paint(sel){ for(var k=0;k<btns.length;k++) btns[k].classList.toggle("on", k===sel); }
      function pick(idx){ var s=segs[idx]; if(!en||!s||s.enabled===false) return; ctl.v=idx; paint(idx); valueChange(it.id, idx); }
      for(var i=0;i<segs.length;i++){ (function(c, idx){
        var b=document.createElement("span"); b.className="fb-seg-btn"+((c&&c.enabled===false)?" disabled":""); b.setAttribute("part","segmented-option"); b.title=(c&&c.label)||"";
        if(c&&c.iconSvg&&c.iconSvg.content){ b.innerHTML='<svg viewBox="'+attrEsc((c.iconSvg.viewBox)||"0 0 24 24")+'" width="16" height="16" aria-hidden="true">'+c.iconSvg.content+'</svg>'; }
        else { b.textContent=(c&&c.label)||""; }
        b.addEventListener("click", function(ev){ ev.stopPropagation(); pick(idx); });
        grp.appendChild(b); btns.push(b);
      })(segs[i], i); }
      paint(ctl.v); d.appendChild(grp); menuEl.appendChild(d);
      var ridx=L.rows.length;
      function nextEnabled(from, dir){ var n=segs.length; for(var k=1;k<=n;k++){ var i2=from+dir*k; if(i2<0||i2>=n) return -1; if(segs[i2]&&segs[i2].enabled!==false) return i2; } return -1; }
      L.rows.push({el:d, item:it, navigable:en, hasSub:false, kind:"segmented",
        adjust:function(delta){ if(!en) return; var base=(ctl.v<0?(delta>0?-1:segs.length):ctl.v); var nx=nextEnabled(base, delta>0?1:-1); if(nx>=0) pick(nx); }});
      d.addEventListener("mouseenter", function(){ closeLayersFrom(depth+1); setActive(depth, ridx); });
    }

    function buildLayer(menuEl, items, depth){
      var L={el:menuEl, rows:[], active:-1};
      layers[depth]=L; if(layers.length>depth+1) layers.length=depth+1;
      var layerHasIcon=false;
      for(var hi=0;hi<items.length;hi++){ var hit=items[hi]; if(isNormalKind(hit)&&hit.iconSvg&&hit.iconSvg.content){ layerHasIcon=true; break; } }
      if(layerHasIcon) menuEl.classList.add("has-icons");
      for(var i=0;i<items.length;i++){
        var it=items[i];
        if(it && it.type==="separator"){ var s=document.createElement("div"); s.className="fb-sep"; s.setAttribute("part","separator"); menuEl.appendChild(s); continue; }
        if(it && it.type==="nowplaying"){ buildNowPlaying(menuEl, L, it, !(it.enabled===false), depth); continue; }
        if(it && it.type==="rating"){ buildRating(menuEl, L, it, !(it.enabled===false), depth); continue; }
        if(it && it.type==="slider"){ buildSlider(menuEl, L, it, !(it.enabled===false), depth); continue; }
        if(it && it.type==="segmented"){ buildSegmented(menuEl, L, it, !(it.enabled===false), depth); continue; }
        var hasSub=!!(it && it.submenu && it.submenu.length);
        var en=!(it && it.enabled===false);
        var d=document.createElement("div");
        d.className="fb-item nrm"+(!en?" disabled":"")+(it&&it.checked?" checked":"")+(hasSub?" has-sub":"");
        d.setAttribute("part","item");
        if(layerHasIcon){
          var ic=document.createElement("span"); ic.className="fb-item-ico"; ic.setAttribute("part","item-icon");
          var iv=it&&it.iconSvg;
          if(iv&&iv.content){ ic.innerHTML='<svg viewBox="'+attrEsc(iv.viewBox||"0 0 24 24")+'" width="16" height="16" aria-hidden="true">'+iv.content+'</svg>'; }
          d.appendChild(ic);
        }
        var lab=document.createElement("span"); lab.setAttribute("part","item-label"); lab.textContent=(it&&it.label)||""; d.appendChild(lab);
        if(hasSub){ var ar=document.createElement("span"); ar.className="fb-arrow"; ar.setAttribute("part","submenu-arrow"); ar.textContent="\u25B6"; d.appendChild(ar); }
        menuEl.appendChild(d);
        var row={el:d, item:it, navigable:(en&&(hasSub||(it&&it.id))), hasSub:hasSub};
        var idx=L.rows.length; L.rows.push(row);
        (function(r, ridx){
          r.el.addEventListener("mouseenter", function(){
            if(r.navigable && r.hasSub && layers[depth+1] && layers[depth+1].parentRowIdx===ridx){ setActive(depth, ridx); return; }   // 幂等：本项子菜单已开则不重开（bug2 防抖）
            closeLayersFrom(depth+1);
            setActive(depth, ridx);
            if(r.navigable && r.hasSub) openSub(depth, ridx);
          });
          r.el.addEventListener("click", function(){
            if(!r.navigable) return;
            if(r.hasSub){ closeLayersFrom(depth+1); openSub(depth, ridx); setActive(depth+1, firstNav(depth+1)); }
            else select(r.item.id);
          });
        })(row, idx);
      }
      return L;
    }

)MENUHTML" LR"MENUHTML(    function openSub(parentDepth, rowIdx){
      var L=layers[parentDepth]; if(!L || !L.rows[rowIdx]) return;
      var content = !!(cur && cur.windowModel==="contentSized");
      if(content && parentDepth>=1) return;   // 内容窗仅 1 层子菜单；2+ 层不展开（深层）
      var parentEl=L.rows[rowIdx].el; var items=L.rows[rowIdx].item.submenu;
      var sub=document.createElement("div"); sub.className="fb-menu"; sub.setAttribute("part","submenu");
      document.body.appendChild(sub);
      buildLayer(sub, items, parentDepth+1);
      layers[parentDepth+1].parentRowIdx = rowIdx;   // 子菜单归属父项（hover 幂等，bug2）
      sub.style.display="block";
      var pr=parentEl.getBoundingClientRect(), sw=sub.offsetWidth, sh=sub.offsetHeight;
      if(content){
        // 固定窗设计：窗口已含子菜单预留，子菜单纯 CSS 在窗内右展，【不 resize 窗口】→ 无 Chromium 闪烁。
        // 垂直：默认对齐父项 top；底超【窗口可视高】则上移（不高于窗口顶），避免被裁。
        var subTop=pr.top;
        if(subTop + sh > window.innerHeight){ subTop=Math.max(0, window.innerHeight - sh); }
        sub.style.left=(pr.right-2)+"px";   // 右展：根右侧（窗口预留区内）
        sub.style.top=subTop+"px";
      } else {
        var x=pr.right-2; if(x+sw>window.innerWidth) x=Math.max(0, pr.left-sw+2);
        var y=pr.top-4; if(y+sh>window.innerHeight) y=Math.max(0, window.innerHeight-sh);
        sub.style.left=x+"px"; sub.style.top=y+"px";
      }
    }

    function placeRoot(menuEl, x, y){
      var aw=window.innerWidth, ah=window.innerHeight, mw=menuEl.offsetWidth, mh=menuEl.offsetHeight;
      if(x+mw>aw) x=Math.max(0, aw-mw);
      if(y+mh>ah) y=Math.max(0, ah-mh);
      menuEl.style.left=x+"px"; menuEl.style.top=y+"px";
    }

    function render(st){
      cur=st;
      closeLayersFrom(0); layers.length=0;
      var root=document.getElementById("menu"); root.innerHTML=""; root.classList.remove("in"); root.classList.remove("out");
      var content = !!(st && st.windowModel==="contentSized");
      document.body.classList.toggle("content-sized", content);
      if(!st || !st.visible || !st.items || !st.items.length){ root.classList.toggle("fb-hidden", true); return; }   // 根显隐改 class 切换：受保护层据 .fb-hidden 强制 display（子菜单仍用内联 display）
      buildLayer(root, st.items, 0);
      root.classList.toggle("fb-hidden", false);
      // 前端样式接管（S-CSS）：每次 render 覆盖 fb-user 防串味；cssReplace=true 时禁用默认样式（仅留 fb-user+fb-protected）。
      // 务必在 measureAndReport / placeRoot 之前应用，保证测量基于最终样式。
      var userStyle=document.getElementById("fb-user");
      if(userStyle) userStyle.textContent=(st.css||"");
      var defStyle=document.getElementById("fb-default");
      if(defStyle) defStyle.disabled=!!st.cssReplace;
      if(content){
        // 内容窗：菜单贴客户区原点；窗口尺寸/定位由 C++ OnContentMeasured 据测量回报完成。
        root.style.left="0px"; root.style.top="0px";
        setActive(0, firstNav(0));
        measureAndReport();
      } else {
        var dpr=window.devicePixelRatio||1;   // anchor 为物理像素，转 CSS 像素再定位（高 DPI 修正）
        placeRoot(root, (st.anchorX||0)/dpr, (st.anchorY||0)/dpr);
        setActive(0, firstNav(0));   // WAI-ARIA：菜单打开焦点置首项
      }
    }

    // ContentSized 固定窗：只测【根菜单】尺寸，回报物理像素 → C++ 定「根 + 预留」固定窗；子菜单不再触发 resize。
    // rAF 防抖：合并同帧多次调用 + 等 layout 稳定后再测，避免快速连发的过期回报竞态。
    var _measureRaf=0;
    function measureAndReport(){
      if(!cur || cur.windowModel!=="contentSized") return;
      if(_measureRaf){ try{ cancelAnimationFrame(_measureRaf); }catch(e){} }
      _measureRaf=requestAnimationFrame(function(){
        _measureRaf=0;
        if(!cur || cur.windowModel!=="contentSized") return;
        // 固定窗设计：只测【根菜单】尺寸；C++ 据此定「根+预留」固定窗口，子菜单不再触发 resize。
        var root=document.getElementById("menu"); if(!root) return;
        var dpr=window.devicePixelRatio||1, r=root.getBoundingClientRect();
        var w=Math.ceil(Math.max(root.scrollWidth, Math.ceil(r.width))*dpr);
        var h=Math.ceil(Math.max(root.scrollHeight, Math.ceil(r.height))*dpr);
        if(w<=0||h<=0) return;
        // 仅当根菜单【有子菜单】时窗口才需右侧预留区（C++ 据此定宽 ×2）；无子菜单则窗口=精确根宽，
        // 避免空预留区在亚克力/云母（窗口级 DWM backdrop 作用于整窗）下露出空白半透明面板。
        var hasSub=false; if(layers[0]&&layers[0].rows){ for(var si=0;si<layers[0].rows.length;si++){ if(layers[0].rows[si].hasSub){ hasSub=true; break; } } }
        try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__ready",{w:w,h:h,hasSub:hasSub}); }catch(e){}
      });
    }

    function onPlaced(ev){
      // 固定窗设计：窗口不 resize，子菜单无需隐藏淡入；仅首次定位触发入场动画。
      if(ev && ev.first){ var root=document.getElementById("menu"); requestAnimationFrame(function(){ root.classList.add("in"); }); }
    }

    function onKey(e){
      if(!layers.length) return;
      var depth=layers.length-1, L=layers[depth];
      switch(e.key){
        case "ArrowDown": moveActive(depth,1); e.preventDefault(); break;
        case "ArrowUp":   moveActive(depth,-1); e.preventDefault(); break;
        case "ArrowRight": { var r=L.rows[L.active]; if(r&&r.adjust){ r.adjust(1); e.preventDefault(); break; } if(r&&r.navigable&&r.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); } e.preventDefault(); break; }
        case "ArrowLeft": { var rl=L.rows[L.active]; if(rl&&rl.adjust){ rl.adjust(-1); e.preventDefault(); break; } if(depth>0){ closeLayersFrom(depth); } e.preventDefault(); break; }
        case "Enter": case " ": { var r2=L.rows[L.active]; if(r2&&r2.navigable){ if(r2.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); } else if(r2.kind==="rating"||r2.kind==="slider"||r2.kind==="segmented"){ /* 值控件：Enter 不选中/不关闭（segmented 用 Left/Right 切段） */ } else select(r2.item.id); } e.preventDefault(); break; }
        case "Escape": if(depth>0){ closeLayersFrom(depth); } else { dismiss("escape"); } e.preventDefault(); break;
        default: break;
      }
    }

    function pull(){ try{ if(window.fb2k&&fb2k.invoke){ var p=fb2k.invoke("menu.__getMenuState"); if(p&&p.then) p.then(render); } }catch(e){} }
    function inMenu(t){ return t && t.closest && t.closest(".fb-menu"); }
    window.addEventListener("mousedown", function(e){ if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("contextmenu", function(e){ e.preventDefault(); if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("keydown", onKey, true);
    if(window.fb2k && fb2k.on){ fb2k.on("menu:show", pull); fb2k.on("menu:__placed", onPlaced); fb2k.on("menu:__hide", function(){ var root=document.getElementById("menu"); if(root){ root.classList.remove("in"); root.classList.add("out"); } }); }
    pull();
  </script>
</body></html>)MENUHTML";
    }
}

// ---------- MenuOverlayWindow ----------

void MenuOverlayWindow::OnWebViewReady() {
    PopupWindow::OnWebViewReady();
    if (!readyHtml_.empty()) {
        NavigateToString(readyHtml_);
    }
}

// ---------- MenuOverlayHost ----------

MenuOverlayHost& MenuOverlayHost::GetInstance() {
    static MenuOverlayHost instance;
    return instance;
}

bool MenuOverlayHost::EnsureCreated() {
    PFC_ASSERT(core_api::is_main_thread());
    if (overlay_ && overlayHwnd_ && IsWindow(overlayHwnd_)) {
        return true;
    }

    overlay_ = std::make_unique<MenuOverlayWindow>();
    overlay_->SetReadyHtml(BuildMenuOverlayHtml());

    PopupWindow::CreateParams params;
    params.url = "";
    params.width = 16;
    params.height = 16;
    params.x = 0;
    params.y = 0;
    params.title = "MenuOverlay";
    params.resizable = false;
    params.frame = false;
    params.transparent = true;
    params.alwaysOnTop = true;
    params.showInTaskbar = false;
    params.beforeClose = false;

    params.hasBehavior = true;
    params.behavior = {
        {"noActivate", false},
        {"owner", "none"},
        {"showInTaskbar", false},
        {"showInAltTab", false},
        {"keepVisibleOnShowDesktop", true}
    };
    params.hasBackdropPolicy = true;
    // 自绘菜单系统级亚克力（DWM backdrop）。overlay 已 transparent=true，默认不透明的
    // .fb-menu 背景（#2b2b2b）仍完全遮住亚克力 → 默认视觉零回归；前端把 .fb-menu 背景
    // 改半透明后才透出亚克力。active/inactive 均用 acrylic：菜单弹出期间宿主窗口可能不持
    // 前台激活（noActivate 行为 / 任务栏交互），inactive 同样保亚克力，避免失焦瞬间掉成纯色。
    // 仅改 backdrop 材质，未触碰 transparent 标志与窗口几何 / 定位 / 不-resize 逻辑
    // （ADR-018 浮任务栏不压暗 / ADR-019 防闪烁固定窗）。注意：此 overlay 由 tray
    // （ContentSized 小窗）与 menu.show（FullscreenOverlay 全屏透明窗）共用，backdrop 在此
    // 一次性设定；全屏透明区在 acrylic 下的观感建议真机验证。
    params.backdropPolicy = {
        {"activeEffect", "acrylic"},
        {"inactiveEffect", "acrylic"},
        {"darkMode", true}
    };

    HWND mainHwnd = core_api::get_main_window();
    HWND hwnd = overlay_->Create(params, kOverlayWindowId, mainHwnd);
    if (!hwnd) {
        overlay_.reset();
        overlayHwnd_ = nullptr;
        console::print("[MenuOverlayHost] EnsureCreated failed: Create returned null");
        return false;
    }
    overlayHwnd_ = hwnd;

    overlay_->SetSuppressLifecycleBroadcast(true);
    overlay_->SetMenuDismissCallback([](const std::string& reason) {
        MenuOverlayHost::GetInstance().Hide(reason);
    });

    ShowWindow(overlayHwnd_, SW_HIDE);
    console::print("[MenuOverlayHost] overlay created and hidden (pooled)");
    return true;
}

std::string MenuOverlayHost::Show(const json& items, int screenX, int screenY,
                                  SelectSink onSelect, DismissSink onDismiss,
                                  const MenuShowOptions& opts, ValueSink onValue) {
    PFC_ASSERT(core_api::is_main_thread());
    if (!EnsureCreated()) {
        return std::string();
    }

    // 交错策略 (R3)：已有菜单在显示【或处于退场动画期】时，先强制立即收尾旧菜单，再写入新 sink，
    // 杜绝新 sink 被误用于旧菜单的 dismiss。退场期 visible_&&closing_ 时，Hide("replaced") 会被
    // if(closing_) return 挡掉，旧菜单不会被收回 → 池化单窗会出现旧菜单淡出同时显示新菜单 (R1)。
    // 故直接 ForceHideImmediate 强制同步收尾（绕过 animate 判定），旧 sink/ownerMode 分发 dismiss。
    if (visible_ || closing_) {
        ForceHideImmediate();
    }
    selectSink_ = std::move(onSelect);
    dismissSink_ = std::move(onDismiss);
    valueSink_ = std::move(onValue);
    ownerMode_ = static_cast<bool>(selectSink_) || static_cast<bool>(dismissSink_)
              || static_cast<bool>(valueSink_);
    windowModel_ = opts.windowModel;  // 窗口几何模型（与 ownerMode_ 正交）
    currentCss_ = opts.css;            // 前端样式接管（S-CSS）：本次 show 的前端 CSS（每次 show 覆盖即复位）
    currentCssReplace_ = opts.cssReplace;  // replace 模式开关（默认 false = override 叠加）
    currentCloseAnimationMs_ = opts.closeAnimationMs;  // 退场动画时长（0=立即隐藏不动画，零回归）

    POINT pt{ screenX, screenY };
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT work{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    RECT full = work;
    if (mon && GetMonitorInfo(mon, &mi)) {
        work = mi.rcWork;
        full = mi.rcMonitor;
    }

    currentItems_ = items.is_array() ? items : json::array();

    if (windowModel_ == MenuWindowModel::ContentSized) {
        // ContentSized：先以临时尺寸在光标附近显示（渲染器 opacity:0 测量），
        // 收到 menu.__ready 后由 OnContentMeasured 定位到内容尺寸（rcMonitor 边界，可压任务栏）。
        awaitingMeasure_ = true;
        pendingAnchor_ = pt;
        anchorClientX_ = 0;   // 内容窗：菜单贴客户区原点，渲染器 placeRoot 退化
        anchorClientY_ = 0;
        const int workW = work.right - work.left;
        const int workH = work.bottom - work.top;
        const int tmpW = workW < 480 ? workW : 480;
        const int tmpH = workH < 720 ? workH : 720;
        // 临时窗放屏幕外测量（off-screen + opacity:0 双保险），由 OnContentMeasured 再移到光标处，
        // 彻底消除“先在上方固定高度出现、测量后跳到光标”的可见跳变（用户反馈 bug1）。
        SetWindowPos(overlayHwnd_, HWND_TOPMOST, -32000, -32000, tmpW, tmpH, SWP_NOACTIVATE);
    } else {
        // FullscreenOverlay（现状）：铺满工作区。
        awaitingMeasure_ = false;
        const int w = work.right - work.left;
        const int h = work.bottom - work.top;
        anchorClientX_ = screenX - work.left;
        anchorClientY_ = screenY - work.top;
        SetWindowPos(overlayHwnd_, HWND_TOPMOST, work.left, work.top, w, h, SWP_NOACTIVATE);
    }

    ShowWindow(overlayHwnd_, SW_SHOW);
    SetForegroundWindow(overlayHwnd_);
    SetWindowPos(overlayHwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // 运行时按本次 config 应用 DWM 系统背景：池化 overlay 被 tray（ContentSized）与
    // menu.show（FullscreenOverlay）共用，逐 show 覆盖。须在 ShowWindow 之后调，避开
    // “隐藏窗口设 backdrop 不初始化合成”的坑（EnsureCreated 的 acrylic 仅作创建默认）。
    // menu.show 走默认 opts（backdrop=acrylic）= 与原创建期行为等价；失败仅忽略。
    if (overlay_) {
        std::string bderr;
        overlay_->UpdateBackdropPolicy(
            json{ {"activeEffect", opts.backdrop}, {"inactiveEffect", opts.backdrop}, {"darkMode", opts.backdropDarkMode} },
            bderr);
    }

    if (overlay_->IsWebViewReady()) {
        if (auto* wv = overlay_->GetWebView()) {
            RECT rc{};
            GetClientRect(overlayHwnd_, &rc);
            wv->SetVisible(true);
            wv->Resize(rc);
            if (auto* ctrl = wv->GetController()) {
                ctrl->NotifyParentWindowPositionChanged();
                ctrl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);  // 让 WebView 内容获键盘焦点（菜单可即时键盘导航）
            }
        }
    }

    currentMenuId_ = "menu-" + std::to_string(++showSeq_);
    visible_ = true;
    showTick_ = GetTickCount64();
    ArmWatchdog();
    if (windowModel_ == MenuWindowModel::ContentSized) {
        ArmMeasureTimer();  // 测量回报兜底
    }

    if (overlay_->IsWebViewReady() && overlay_->GetBridge()) {
        overlay_->GetBridge()->EmitEvent("menu:show", { {"menuId", currentMenuId_} });
    }

    console::printf("[MenuOverlayHost] Show id=%s items=%d model=%s anchorClient=(%d,%d)",
                    currentMenuId_.c_str(), (int)currentItems_.size(),
                    windowModel_ == MenuWindowModel::ContentSized ? "content" : "fullscreen",
                    anchorClientX_, anchorClientY_);
    return currentMenuId_;
}

void MenuOverlayHost::Hide(const std::string& reason) {
    if (reason == "blur" && visible_ && (GetTickCount64() - showTick_) < 250) {
        return;
    }
    if (!visible_) {
        return;
    }
    if (closing_) {
        // 已在退场动画期：幂等吞掉重入 / 多源 dismiss（blur/outside/timeout 等），
        // 由 close-timer → FinalizeHide 收尾 (R1：防淡出期重复触发 / 重入)。
        return;
    }

    // 退场动画判定（opt-in）：仅当配置了时长，且 reason 属于"用户主动关闭"白名单时才播退场；
    // replaced / timeout / measure_timeout 不在白名单，天然走立即（R1：池化单窗不能旧菜单淡出
    // 同时显示新菜单；超时/兜底必须立即收尾）。窗口已失效也直接走立即。
    const bool windowOk = overlayHwnd_ && IsWindow(overlayHwnd_);
    const bool animate = windowOk && currentCloseAnimationMs_ > 0 && !closing_
        && (reason == "outside" || reason == "escape" || reason == "select" || reason == "blur");

    if (animate) {
        // 进入 Closing：visible_ 仍 true；发内部事件 menu:__hide 通知渲染器播退场；ArmCloseTimer
        // 延迟到 close-timer 收尾。此刻【不】SW_HIDE、【不】分发 dismiss（留到 FinalizeHide）。
        // 注意：select 的 selectSink_ 已在 OnSelect 里先于 Hide("select") 派发（动作不被动画延迟，R7）。
        closing_ = true;
        pendingCloseReason_ = reason;
        if (overlay_ && overlay_->IsWebViewReady() && overlay_->GetBridge()) {
            overlay_->GetBridge()->EmitEvent("menu:__hide", { {"menuId", currentMenuId_} });  // 内部事件（overlay 私有 bridge→渲染器）；:__ 前缀=codegen 不进公共 events.ts
        }
        ArmCloseTimer(currentCloseAnimationMs_);
        console::printf("[MenuOverlayHost] Hide(animate) id=%s reason=%s ms=%d",
                        currentMenuId_.c_str(), reason.c_str(), currentCloseAnimationMs_);
        return;
    }

    // 立即路径（现状同步隐藏，零回归）：未开动画 / replaced / timeout / measure_timeout / 窗口失效等。
    FinalizeHide(reason);
}

void MenuOverlayHost::FinalizeHide(const std::string& reason) {
    // 两段式 Hide 的同步收尾（立即路径 / close-timer 收尾共用）。
    // 先捕获并复位状态（务必在 ShowWindow 之前，R2）：ShowWindow(SW_HIDE) 会同步触发
    // WM_KILLFOCUS → dismiss 回调 → 重入 Hide。若状态后置清理，重入会先清掉 ownerMode_，
    // 外层继续时误判为非 owner 而泄漏 menu:dismiss。先置 visible_=false 且 closing_=false 让重入早退。
    visible_ = false;
    closing_ = false;
    awaitingMeasure_ = false;
    pendingCloseReason_.clear();
    const std::string closedId = currentMenuId_;
    currentMenuId_.clear();
    currentItems_ = json::array();
    currentCss_.clear();              // 前端样式接管（S-CSS）：本次 show 结束复位，防泄漏到下次
    currentCssReplace_ = false;

    // owner-mode：dismiss 改走 sink（tray 默认无 sink = 原生菜单关闭不发事件），
    // 不发公共 menu:dismiss；menu.* 普通模式维持现状。
    // sink 生命周期：本次 show 结束即释放（避免泄漏到下次 menu.show）。
    const bool wasOwnerMode = ownerMode_;
    DismissSink dismissSink = dismissSink_;
    selectSink_ = nullptr;
    dismissSink_ = nullptr;
    valueSink_ = nullptr;
    ownerMode_ = false;

    KillWatchdog();
    KillMeasureTimer();
    KillCloseTimer();
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        ShowWindow(overlayHwnd_, SW_HIDE);
    }

    if (wasOwnerMode) {
        if (dismissSink) {
            try { dismissSink(reason); } catch (...) {}
        }
    } else {
        BridgeCore::GetInstance().EmitEvent("menu:dismiss", {
            {"menuId", closedId},
            {"reason", reason}
        });
    }
    console::printf("[MenuOverlayHost] FinalizeHide id=%s reason=%s owner=%d",
                    closedId.c_str(), reason.c_str(), (int)wasOwnerMode);
}

void MenuOverlayHost::ForceHideImmediate() {
    // Show 取消挂起关闭复用 (R3)：绕过 animate 判定，直接同步收尾。退场期 visible_&&closing_ 时，
    // 新 Hide("replaced") 会被 if(closing_) return 挡掉，故 Show 不能依赖 Hide("replaced")。
    // 用 "replaced" 收尾（与历史 Show 调 Hide("replaced") 行为一致；tray owner-mode 无 dismiss sink，
    // reason 仅记录用；menu.show 因 closeAnimationMs=0 永不进 Closing，此处恒等于历史立即替换路径）。
    if (!visible_ && !closing_) {
        return;
    }
    FinalizeHide("replaced");
}

void MenuOverlayHost::OnSelect(const std::string& itemId) {
    if (!visible_) {
        return;
    }
    const std::string menuId = currentMenuId_;
    if (ownerMode_) {
        // owner-mode：选中改走 sink（→ tray:menuItemClicked），不发公共 menu:select
        if (selectSink_) {
            try { selectSink_(itemId); } catch (...) {}
        }
    } else {
        BridgeCore::GetInstance().EmitEvent("menu:select", {
            {"menuId", menuId},
            {"itemId", itemId}
        });
    }
    console::printf("[MenuOverlayHost] Select id=%s item=%s owner=%d",
                    menuId.c_str(), itemId.c_str(), (int)ownerMode_);
    Hide("select");
}

void MenuOverlayHost::OnValueChanged(const std::string& itemId, int value) {
    // 富控件（rating/slider）值变更：经 valueSink_ 回报，【不关闭菜单】。
    // 无 sink（menu.* 普通模式）= 静默忽略，不发任何公共事件。
    if (!visible_) {
        return;
    }
    if (valueSink_) {
        try { valueSink_(itemId, value); } catch (...) {}
    }
    console::printf("[MenuOverlayHost] ValueChanged item=%s value=%d owner=%d",
                    itemId.c_str(), value, (int)ownerMode_);
}

void MenuOverlayHost::OnContentMeasured(int wPhysical, int hPhysical, int originXPhysical, bool hasSubmenu) {
    PFC_ASSERT(core_api::is_main_thread());
    // 固定窗设计（根治 resize 闪烁）：窗口一次定为「根菜单 + 子菜单预留」尺寸，全程不 resize；
    // 子菜单纯 CSS 在窗内右展（窗口不动 → 无 Chromium surface 重合成闪烁）。仅首次（根测量）生效。
    if (!visible_ || windowModel_ != MenuWindowModel::ContentSized || !awaitingMeasure_) {
        return;
    }
    awaitingMeasure_ = false;
    KillMeasureTimer();
    (void)originXPhysical;  // 固定窗设计下窗口不动，不再用 originX 锚定根
    const int rootWPhysical = wPhysical;
    const int rootHPhysical = hPhysical;
    if (rootWPhysical <= 0 || rootHPhysical <= 0) {
        return;
    }

    HMONITOR mon = MonitorFromPoint(pendingAnchor_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    RECT full{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    RECT work = full;
    if (mon && GetMonitorInfo(mon, &mi)) {
        full = mi.rcMonitor;   // rcMonitor 边界 → 可压任务栏
        work = mi.rcWork;
    }

    // 窗口宽：仅当根菜单【有子菜单】时才 ×2 预留右侧展开区；无子菜单则=精确根宽，
    // 避免空预留区在亚克力/云母（窗口级 DWM backdrop 作用于整窗）下露出空白半透明面板（用户反馈 bug）。
    // 高 = 根高（子菜单通常 ≤ 根高，CSS 在窗内右展、底超则上移）；不在高度上预留（否则托盘屏底时窗口下半超屏 → 整窗上移 → 把根菜单顶高）。
    int w = hasSubmenu ? rootWPhysical * 2 : rootWPhysical;
    int h = rootHPhysical;
    const int maxH = work.bottom - work.top;
    if (h > maxH) h = maxH;
    const int maxW = work.right - work.left;
    if (w > maxW) w = maxW;

    // 定位：根底贴光标（上展贴托盘），根左=光标 x。窗口高=根高 → 窗口底=光标 y（不超屏，无垂直偏移）。
    int x = pendingAnchor_.x;
    int y = pendingAnchor_.y - h;
    if (x + w > full.right) x = full.right - w;    // 右预留超屏 → 左移整窗
    if (x < full.left) x = full.left;
    if (y < full.top) y = full.top;

    SetWindowPos(overlayHwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    if (overlay_->IsWebViewReady()) {
        if (auto* wv = overlay_->GetWebView()) {
            RECT rc{};
            GetClientRect(overlayHwnd_, &rc);
            wv->SetVisible(true);
            wv->Resize(rc);
            if (auto* ctrl = wv->GetController()) {
                ctrl->NotifyParentWindowPositionChanged();
            }
        }
    }
    if (overlay_->IsWebViewReady() && overlay_->GetBridge()) {
        overlay_->GetBridge()->EmitEvent("menu:__placed", { {"menuId", currentMenuId_}, {"first", true} });  // 内部事件（overlay 私有 bridge→渲染器）；:__ 前缀=codegen 不进公共 events.ts
    }
    console::printf("[MenuOverlayHost] ContentMeasured(A) id=%s win=(%d,%d) pos=(%d,%d)",
                    currentMenuId_.c_str(), w, h, x, y);
}

json MenuOverlayHost::GetMenuStateJson() const {
    return {
        {"visible", visible_},
        {"menuId", currentMenuId_},
        {"items", currentItems_},
        {"anchorX", anchorClientX_},
        {"anchorY", anchorClientY_},
        // 窗口几何模型：渲染器据此分支（contentSized = 去 #block + placeRoot 退化 + 测量回报）。
        {"windowModel", windowModel_ == MenuWindowModel::ContentSized ? "contentSized" : "fullscreen"},
        // 前端样式接管（S-CSS）：css = 前端注入样式字符串（写入 <style id="fb-user">）；
        // cssReplace = true 时 replace 模式（禁用默认样式，仅留 fb-user+fb-protected），false = override 叠加。
        {"css", currentCss_},
        {"cssReplace", currentCssReplace_}
    };
}

void MenuOverlayHost::Shutdown() {
    KillWatchdog();
    KillMeasureTimer();
    KillCloseTimer();
    visible_ = false;
    closing_ = false;
    awaitingMeasure_ = false;
    pendingCloseReason_.clear();
    currentMenuId_.clear();
    currentItems_ = json::array();
    if (overlay_) {
        overlay_->Destroy();
        overlay_.reset();
    }
    overlayHwnd_ = nullptr;
}

void MenuOverlayHost::ArmWatchdog() {
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        SetTimer(overlayHwnd_, kWatchdogTimerId, kWatchdogTimeoutMs, &MenuOverlayHost::WatchdogTimerProc);
    }
}

void MenuOverlayHost::KillWatchdog() {
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        KillTimer(overlayHwnd_, kWatchdogTimerId);
    }
}

void CALLBACK MenuOverlayHost::WatchdogTimerProc(HWND /*hwnd*/, UINT /*msg*/, UINT_PTR idEvent, DWORD /*elapsed*/) {
    if (idEvent != kWatchdogTimerId) {
        return;
    }
    MenuOverlayHost::GetInstance().Hide("timeout");
}

void MenuOverlayHost::ArmMeasureTimer() {
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        SetTimer(overlayHwnd_, kMeasureTimerId, kMeasureTimeoutMs, &MenuOverlayHost::MeasureTimerProc);
    }
}

void MenuOverlayHost::KillMeasureTimer() {
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        KillTimer(overlayHwnd_, kMeasureTimerId);
    }
}

void CALLBACK MenuOverlayHost::MeasureTimerProc(HWND /*hwnd*/, UINT /*msg*/, UINT_PTR idEvent, DWORD /*elapsed*/) {
    if (idEvent != kMeasureTimerId) {
        return;
    }
    // ContentSized 测量回报兜底：超时未收 menu.__ready 即关闭，避免卡在临时尺寸。
    auto& self = MenuOverlayHost::GetInstance();
    if (self.awaitingMeasure_) {
        self.Hide("measure_timeout");
    }
}

void MenuOverlayHost::ArmCloseTimer(int ms) {
    if (!overlayHwnd_ || !IsWindow(overlayHwnd_)) {
        return;
    }
    if (ms < 0) ms = 0;
    if (ms > kCloseAnimationMaxMs) ms = kCloseAnimationMaxMs;   // 防御性 clamp（解析层已 clamp 0..1000）
    SetTimer(overlayHwnd_, kCloseTimerId, static_cast<UINT>(ms), &MenuOverlayHost::CloseTimerProc);
}

void MenuOverlayHost::KillCloseTimer() {
    if (overlayHwnd_ && IsWindow(overlayHwnd_)) {
        KillTimer(overlayHwnd_, kCloseTimerId);
    }
}

void CALLBACK MenuOverlayHost::CloseTimerProc(HWND /*hwnd*/, UINT /*msg*/, UINT_PTR idEvent, DWORD /*elapsed*/) {
    if (idEvent != kCloseTimerId) {
        return;
    }
    // 退场动画收尾：退场时长到期后做与立即路径等价的同步收尾。reason 用进入 Closing 时记录的原始值。
    auto& self = MenuOverlayHost::GetInstance();
    if (self.closing_) {
        self.FinalizeHide(self.pendingCloseReason_);
    }
}