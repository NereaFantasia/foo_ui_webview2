    var cur = null;
    var layers = [];   // index=depth: {el, rows:[{el,item,navigable,hasSub}], active}
    // Single interaction mode — navigation | editor.
    var interactionMode = "navigation";  // "navigation" | "editor"
    var editorCtx = null;                // { depth, rowIdx, focusEl, exit }
    var pendingRootFocus = false;        // ContentSized: focus after first placed
    var placedGeometry = null;           // ContentSized CSS-pixel slot geometry
    var submenuPanelSequence = 0;        // monotonic root→host panel state reports
    var submenuPanelOpen = false;        // host confirms that the child HWND is visible

    // Clear editor/focus pending state without stealing focus. Used by dismiss/select/hide/render.
    function cleanupMenuInteraction(){
      exitEditor(false);
      interactionMode="navigation";
      editorCtx=null;
      pendingRootFocus=false;
    }
    function dismiss(reason){ try{ cleanupMenuInteraction(); if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__dismiss",{menuId:(cur&&cur.menuId)||"",reason:reason}); }catch(e){} }
    function select(token){ try{ cleanupMenuInteraction(); if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__select",{menuId:(cur&&cur.menuId)||"",token:token}); }catch(e){} }   // 只回 opaque token；C++ 侧 token 索引解析出 publicId + 可信 origin
    function valueChange(token, value){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__valueChanged",{menuId:(cur&&cur.menuId)||"",token:token,value:value}); }catch(e){} }
    function reportSubmenuSurfaceReady(){ try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__submenuPanel",{menuId:(cur&&cur.menuId)||"",ready:true,parentToken:(cur&&cur.parentToken)||""}); }catch(e){} }
    function clampInt(v,lo,hi){ v=Math.round((+v)||0); if(v<lo)v=lo; if(v>hi)v=hi; return v; }
    function numOr(v,d){ return (typeof v==="number"&&!isNaN(v))?v:d; }
    function attrEsc(s){ return (""+s).replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }
    function isNormalKind(it){ return !!it && it.type!=="separator" && it.type!=="nowplaying" && it.type!=="rating" && it.type!=="slider" && it.type!=="segmented"; }   // 仅普通/子菜单项参与图标列（富项含 segmented 不参与，镜像 C++ TrayItemKindRendersIcon）
    function isRichKind(k){ return k==="rating"||k==="slider"||k==="segmented"; }

    // Shared hide/show protocol + stable attrs (from menu-overlay-layout.mjs).
    function applyAttrs(el, attrs){
      if(!el||!attrs) return;
      for(var k in attrs){ if(Object.prototype.hasOwnProperty.call(attrs,k)) el.setAttribute(k, attrs[k]); }
    }
    function runProtocol(el, steps){
      if(!el||!steps) return;
      for(var i=0;i<steps.length;i++){
        var s=steps[i];
        if(s.op==="addClass") el.classList.add(s.className);
        else if(s.op==="removeClass") el.classList.remove(s.className);
        else if(s.op==="setProperty") el.style.setProperty(s.name, s.value, s.priority||"");
        else if(s.op==="removeProperty") el.style.removeProperty(s.name);
      }
    }
    function hideMenuEl(el){ runProtocol(el, hideMenuProtocol()); }
    function showMenuEl(el){ runProtocol(el, showMenuProtocol()); }
    function itemZone(it, fallback){ return (it&&it._zone)||fallback||""; }
    function mountHost(){ return document.getElementById("viewport")||document.body; }

    function applyContentSizedGeometry(el, geometry){
      if(!el||!geometry) return;
      el.style.setProperty("left",geometry.left,"important");
      el.style.setProperty("top",geometry.top,"important");
      el.style.setProperty("max-width",geometry.maxWidth,"important");
      el.style.setProperty("max-height",geometry.maxHeight,"important");
      el.style.setProperty("min-width","0px","important");
      el.style.setProperty("overflow","auto","important");
    }
    function clearContentSizedGeometry(el){
      if(!el) return;
      el.style.removeProperty("left");
      el.style.removeProperty("top");
      el.style.removeProperty("max-width");
      el.style.removeProperty("max-height");
      el.style.removeProperty("min-width");
      el.style.removeProperty("overflow");
    }

    function applyPlacedGeometry(ev){
      placedGeometry=normalizePlacedGeometry(ev, window.devicePixelRatio||1);
      if(!placedGeometry) return;
      var viewport=mountHost(), root=document.getElementById("menu");
      if(viewport){ viewport.style.width=placedGeometry.viewportW+"px"; viewport.style.height=placedGeometry.viewportH+"px"; }
      if(root){
        applyContentSizedGeometry(root,{
          left:"0px",top:placedGeometry.rootTop+"px",
          maxWidth:placedGeometry.rootSlotW+"px",
          maxHeight:(placedGeometry.viewportH-placedGeometry.rootTop)+"px"
        });
      }
    }

    function setSubtreeInert(root, inert){
      if(!root) return;
      if(inert){ root.setAttribute("aria-hidden","true"); }
      else { root.removeAttribute("aria-hidden"); }
      var nodes=root.querySelectorAll("*");
      for(var i=0;i<nodes.length;i++){
        if(inert){ nodes[i].setAttribute("tabindex","-1"); }
        else if(nodes[i].getAttribute("data-fb-focusable")==="1"){ /* restored by editor enter */ }
        else { nodes[i].setAttribute("tabindex","-1"); }
      }
    }

    function syncRovingTabindex(depth){
      var L=layers[depth]; if(!L) return;
      for(var i=0;i<L.rows.length;i++){
        var r=L.rows[i]; if(!r||!r.el) continue;
        if(!r.navigable){ r.el.setAttribute("tabindex","-1"); continue; }
        r.el.setAttribute("tabindex", (interactionMode==="navigation" && i===L.active) ? "0" : "-1");
      }
    }

    function setActive(depth, idx, opts){
      opts=opts||{};
      var L=layers[depth]; if(!L) return;
      if(L.active>=0 && L.rows[L.active]) L.rows[L.active].el.classList.remove("active");
      L.active=idx;
      syncRovingTabindex(depth);
      if(idx>=0 && L.rows[idx]){
        var e=L.rows[idx].el; e.classList.add("active");
        try{ e.scrollIntoView({block:"nearest"}); }catch(x){}
        // Pointer hover must not steal keyboard/editor focus.
        if(interactionMode==="navigation" && shouldFocusOnActivate(opts)){
          try{ e.focus({preventScroll:true}); }catch(x2){ try{ e.focus(); }catch(x3){} }
        }
      }
    }
    // Pointer/mouse hover path: update active/roving only, never real focus().
    function setActiveFromPointer(depth, idx){ setActive(depth, idx, {focus:false}); }
    function firstNav(depth){ var L=layers[depth]; if(!L) return -1; for(var i=0;i<L.rows.length;i++) if(L.rows[i].navigable) return i; return -1; }
    function lastNav(depth){ var L=layers[depth]; if(!L) return -1; for(var i=L.rows.length-1;i>=0;i--) if(L.rows[i].navigable) return i; return -1; }
    function moveActive(depth, dir){
      var L=layers[depth]; if(!L||!L.rows.length) return;
      var n=L.rows.length, i=(L.active<0?(dir>0?-1:0):L.active);
      for(var k=0;k<n;k++){ i=(i+dir+n)%n; if(L.rows[i].navigable){ setActive(depth,i); return; } }
    }
    function closeLayersFrom(depth){
      // 固定窗设计：子菜单收起仅移除其 DOM；窗口不 resize（无闪），无需重测/缩窗。
      if(interactionMode==="editor" && editorCtx && editorCtx.depth>=depth) exitEditor(false);
      for(var i=layers.length-1;i>=depth;i--){ if(i>0 && layers[i] && layers[i].el && layers[i].el.parentNode) layers[i].el.parentNode.removeChild(layers[i].el); }
      if(layers.length>depth) layers.length=depth;
      // Restore parent expanded state.
      if(depth>0){
        var pL=layers[depth-1];
        if(pL){
          for(var ri=0;ri<pL.rows.length;ri++){
            if(pL.rows[ri] && pL.rows[ri].hasSub){
              pL.rows[ri].el.setAttribute("aria-expanded","false");
            }
          }
        }
      }
      if(depth<=1 && cur && cur.windowModel==="contentSized") reportSubmenuPanel(null);
    }

    function enterEditor(depth, rowIdx){
      var L=layers[depth]; if(!L||!L.rows[rowIdx]) return;
      var row=L.rows[rowIdx];
      if(!row.navigable || !isRichKind(row.kind) || !row.enterEditor) return;
      interactionMode="editor";
      editorCtx={ depth:depth, rowIdx:rowIdx, row:row };
      row.el.setAttribute("role","none");
      row.el.setAttribute("tabindex","-1");
      if(row.controlRoot) setSubtreeInert(row.controlRoot, false);
      var focusEl=row.enterEditor();
      editorCtx.focusEl=focusEl||null;
      if(focusEl){ try{ focusEl.focus({preventScroll:true}); }catch(x){ try{ focusEl.focus(); }catch(x2){} } }
    }
    function exitEditor(restoreFocus){
      if(interactionMode!=="editor" || !editorCtx) { interactionMode="navigation"; editorCtx=null; return; }
      var row=editorCtx.row;
      if(row && row.exitEditor) row.exitEditor();
      if(row && row.controlRoot) setSubtreeInert(row.controlRoot, true);
      if(row && row.el){
        // Restore navigation role/label.
        if(row.applyNavAria) row.applyNavAria();
      }
      var d=editorCtx.depth, idx=editorCtx.rowIdx;
      interactionMode="navigation";
      editorCtx=null;
      if(restoreFocus!==false) setActive(d, idx, {focus:true});
      else syncRovingTabindex(d);
    }

    function stampItem(el, it, depth, zone){
      applyAttrs(el, buildItemStableAttrs(it, { depth: depth, zone: zone }));
    }
    function stampSep(el, depth, zone, zoneSeparator){
      applyAttrs(el, buildSeparatorStableAttrs({ depth: depth, zone: zone, zoneSeparator: !!zoneSeparator }));
    }

    // ---- 富菜单项构建器（仅根层出现；均 push 一个 row 供键盘导航/active 高亮）----
    function applyDisabledAria(el, en){ if(!en) el.setAttribute("aria-disabled","true"); else el.removeAttribute("aria-disabled"); }

    function buildNowPlaying(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div");
      d.className="fb-item fb-np"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      d.setAttribute("role","menuitem");
      d.setAttribute("tabindex","-1");
      applyDisabledAria(d, en);
      var cv=document.createElement("div"); cv.className="fb-np-cover"; cv.setAttribute("part","nowplaying-cover"); cv.setAttribute("aria-hidden","true");
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
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"nowplaying", zone:zone};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
      d.addEventListener("click", function(){ if(row.navigable) select(it._token); });   // 点击=普通项：回传 {id} + 关闭
    }

    function buildRating(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div");
      d.className="fb-item fb-rating"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-rating-label"; lb.textContent=it.label; d.appendChild(lb); }
      var box=document.createElement("span"); box.className="fb-stars"; box.setAttribute("part","rating-stars");
      var ctl={v:clampInt(it.value,0,5)};
      var stars=[];
      // Internal focusable slider control (stars remain visual/pointer only).
      var focusEl=document.createElement("span");
      focusEl.className="fb-rating-control";
      focusEl.setAttribute("role","slider");
      focusEl.setAttribute("aria-valuemin","0");
      focusEl.setAttribute("aria-valuemax","5");
      focusEl.setAttribute("aria-valuenow", String(ctl.v));
      focusEl.setAttribute("aria-orientation","horizontal");
      focusEl.setAttribute("tabindex","-1");
      focusEl.setAttribute("data-fb-focusable","1");
      function paint(n){
        for(var k=0;k<5;k++) stars[k].classList.toggle("on", k<n);
        focusEl.setAttribute("aria-valuenow", String(n));
        if(interactionMode==="navigation") applyNavAria();
      }
      for(var k=0;k<5;k++){ (function(k){
        var s=document.createElement("span"); s.className="fb-star"; s.setAttribute("part","rating-star"); s.textContent="\u2605";
        s.setAttribute("aria-hidden","true"); s.setAttribute("tabindex","-1");
        s.addEventListener("mouseenter", function(){ if(en) paint(k+1); });                                  // hover 预览
        s.addEventListener("click", function(ev){ ev.stopPropagation(); if(!en) return; var nv=((k+1)===ctl.v)?0:(k+1); ctl.v=nv; paint(nv); valueChange(it._token, nv); });   // 点同星=清 0
        box.appendChild(s); stars.push(s);
      })(k); }
      box.addEventListener("mouseleave", function(){ paint(ctl.v); });                                       // 移出还原实际值
      box.appendChild(focusEl);
      d.appendChild(box);
      paint(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"rating", label:it.label, value:ctl.v}));
        setSubtreeInert(box, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"rating", zone:zone, controlRoot:box, applyNavAria:applyNavAria,
        adjust:function(delta){ if(!en) return; var nv=clampInt(ctl.v+delta,0,5); if(nv!==ctl.v){ ctl.v=nv; paint(nv); valueChange(it._token, nv); } },
        enterEditor:function(){ d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(box,false); focusEl.setAttribute("tabindex","0"); return focusEl; },
        exitEditor:function(){ focusEl.setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="ArrowRight"||e.key==="ArrowUp"){ row.adjust(1); return true; }
          if(e.key==="ArrowLeft"||e.key==="ArrowDown"){ row.adjust(-1); return true; }
          if(e.key==="Home"){ if(ctl.v!==0){ ctl.v=0; paint(0); valueChange(it._token,0); } return true; }
          if(e.key==="End"){ if(ctl.v!==5){ ctl.v=5; paint(5); valueChange(it._token,5); } return true; }
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    function buildSlider(menuEl, L, it, en, depth, zone){
      var norm=normalizeSliderRange(it);
      var mn=norm.min, mx=norm.max, orient=norm.orientation, constant=norm.constant;
      var ctl={v:norm.value};
      var d=document.createElement("div");
      d.className="fb-item fb-slider"+(!en?" disabled":"");
      d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      d.setAttribute("data-orientation", orient);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-slider-label"; lb.textContent=it.label; d.appendChild(lb); }
      var track=document.createElement("span"); track.className="fb-slider-track"; track.setAttribute("part","slider-track");
      var fill=document.createElement("span"); fill.className="fb-slider-fill"; track.appendChild(fill);
      var thumb=document.createElement("span"); thumb.className="fb-slider-thumb"; track.appendChild(thumb);
      // Internal focusable slider control.
      var focusEl=document.createElement("span");
      focusEl.className="fb-slider-control";
      focusEl.setAttribute("role","slider");
      focusEl.setAttribute("aria-valuemin", String(mn));
      focusEl.setAttribute("aria-valuemax", String(mx));
      focusEl.setAttribute("aria-valuenow", String(ctl.v));
      focusEl.setAttribute("aria-orientation", orient);
      focusEl.setAttribute("tabindex","-1");
      focusEl.setAttribute("data-fb-focusable","1");
      track.appendChild(focusEl);
      d.appendChild(track);
      var out=document.createElement("span"); out.className="fb-slider-val"; out.setAttribute("part","slider-value"); d.appendChild(out);
      function applyPaintStyles(v){
        var st=sliderPaintStyles({orientation:orient, min:mn, max:mx, value:v});
        fill.style.width=st.fill.width; fill.style.height=st.fill.height;
        fill.style.left=st.fill.left; fill.style.top=st.fill.top; fill.style.bottom=st.fill.bottom;
        thumb.style.left=st.thumb.left; thumb.style.top=st.thumb.top; thumb.style.bottom=st.thumb.bottom;
        out.textContent=""+v;
        focusEl.setAttribute("aria-valuenow", String(v));
        if(interactionMode==="navigation") applyNavAria();
      }
      var lastSent=0, dragging=false;
      function emit(v, force){
        if(constant) return;
        if(!shouldEmitSliderValue({min:mn,max:mx,value:v,previous:NaN}) && !force) return;
        // Always gate constant; for force on same value still emit end value once.
        if(!Number.isFinite(v) || v<mn || v>mx) return;
        var now=Date.now();
        if(shouldThrottleEmit(now, lastSent, force, 50)){
          lastSent=now;
          if(!force && v===ctl._lastEmitted) return;
          ctl._lastEmitted=v;
          valueChange(it._token, v);
        }
      }
      function setFromEvent(e, force){
        if(!en || constant) return;
        var r=track.getBoundingClientRect();
        var nv=sliderValueFromPointer({orientation:orient, min:mn, max:mx, clientX:e.clientX, clientY:e.clientY, rect:r});
        if(nv!==ctl.v){ ctl.v=nv; applyPaintStyles(nv); emit(nv, force); }
        else if(force){ emit(nv, true); }
      }
      track.addEventListener("pointerdown", function(e){ if(!en||constant) return; e.stopPropagation(); dragging=true; try{ track.setPointerCapture(e.pointerId); }catch(x){} setFromEvent(e, false); });
      track.addEventListener("pointermove", function(e){ if(dragging) setFromEvent(e, false); });
      track.addEventListener("pointerup", function(e){ if(dragging){ dragging=false; try{ track.releasePointerCapture(e.pointerId); }catch(x){} setFromEvent(e, true); } });
      track.addEventListener("pointercancel", function(){ if(dragging){ dragging=false; emit(ctl.v, true); } });
      track.addEventListener("click", function(ev){ ev.stopPropagation(); });   // 阻止冒泡到 row（不应选中/关闭）
      applyPaintStyles(ctl.v);
      menuEl.appendChild(d);
      var ridx=L.rows.length;
      var step=constant?0:Math.max(1, Math.round((mx-mn)/20));
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"slider", label:it.label, value:ctl.v, min:mn, max:mx}));
        setSubtreeInert(track, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"slider", zone:zone, controlRoot:track, applyNavAria:applyNavAria,
        adjust:function(delta){
          if(!en||constant) return;
          var nv=clampInt(ctl.v+delta*step,mn,mx);
          if(nv!==ctl.v){ ctl.v=nv; applyPaintStyles(nv); valueChange(it._token, nv); }
        },
        enterEditor:function(){ d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(track,false); focusEl.setAttribute("tabindex","0"); return focusEl; },
        exitEditor:function(){ focusEl.setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          if(constant) return (e.key==="ArrowUp"||e.key==="ArrowDown"||e.key==="ArrowLeft"||e.key==="ArrowRight"||e.key==="Home"||e.key==="End");
          var next=applySliderKey({key:e.key, orientation:orient, min:mn, max:mx, value:ctl.v, step:step});
          if(next===ctl.v && e.key!=="Home" && e.key!=="End" && e.key!=="ArrowUp" && e.key!=="ArrowDown" && e.key!=="ArrowLeft" && e.key!=="ArrowRight") return false;
          if(sliderKeyAction(e.key, orient)){
            if(next!==ctl.v){ ctl.v=next; applyPaintStyles(next); valueChange(it._token, next); }
            return true;
          }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    // segmented 分段单选：读 it.segments（互斥选项），it.value=选中段索引。点启用段→paint+valueChange（走现有 value 通道，不关菜单）；
    // Left/Right 经 row.adjust 移到上/下一个【启用】段并 valueChange（clamp 边界）。index→业务语义由前端映射。
    // Editor 态：选中迁移时同步 roving tabindex + 真实 focus（键盘）；pointer pick 至少同步 tabindex。
    function buildSegmented(menuEl, L, it, en, depth, zone){
      var d=document.createElement("div"); d.className="fb-item fb-seg"+(!en?" disabled":""); d.setAttribute("part","item");
      stampItem(d, it, depth, zone);
      applyDisabledAria(d, en);
      if(it.label){ var lb=document.createElement("span"); lb.className="fb-seg-label"; lb.setAttribute("part","segmented-label"); lb.textContent=it.label; d.appendChild(lb); }
      var grp=document.createElement("span"); grp.className="fb-seg-group"; grp.setAttribute("part","segmented-group");
      grp.setAttribute("role","radiogroup");
      var segs=(it.segments||[]), btns=[]; var ctl={v:(typeof it.value==="number")?it.value:-1};
      function applySegmentPlan(plan, doFocus){
        if(!plan) return;
        for(var k=0;k<btns.length;k++){
          btns[k].classList.toggle("on", plan.ariaChecked[k]==="true");
          btns[k].setAttribute("aria-checked", plan.ariaChecked[k]);
          if(interactionMode==="editor") btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
        }
        if(interactionMode==="editor" && doFocus && plan.focusIndex>=0 && btns[plan.focusIndex]){
          try{ btns[plan.focusIndex].focus({preventScroll:true}); }catch(x){ try{ btns[plan.focusIndex].focus(); }catch(x2){} }
        }
        if(interactionMode==="navigation") applyNavAria();
      }
      function paint(sel){
        var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:sel});
        // paint always updates checked/on; tabindex only in editor (nav uses setSubtreeInert).
        for(var k=0;k<btns.length;k++){
          btns[k].classList.toggle("on", plan.ariaChecked[k]==="true");
          btns[k].setAttribute("aria-checked", plan.ariaChecked[k]);
        }
        if(interactionMode==="navigation") applyNavAria();
      }
      // opts: {focus:true|false} — keyboard paths focus; pointer click syncs tabindex only.
      function pick(idx, opts){
        var s=segs[idx]; if(!en||!s||!isSegmentEnabled(s)) return;
        ctl.v=idx;
        var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:idx});
        if(interactionMode==="editor"){
          applySegmentPlan(plan, !!(opts&&opts.focus));
          // pointer: still sync tabindex even when not focusing outside menu.
          if(!(opts&&opts.focus)){
            for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
          }
        } else {
          paint(idx);
        }
        valueChange(it._token, idx);
      }
      for(var i=0;i<segs.length;i++){ (function(c, idx){
        var b=document.createElement("span"); b.className="fb-seg-btn"+((c&&c.enabled===false)?" disabled":""); b.setAttribute("part","segmented-option"); b.title=(c&&c.label)||"";
        b.setAttribute("role","radio");
        b.setAttribute("aria-checked","false");
        b.setAttribute("tabindex","-1");
        b.setAttribute("data-fb-focusable","1");
        if(c&&c.enabled===false) b.setAttribute("aria-disabled","true");
        if(c&&c.iconSvg&&c.iconSvg.content){ mountSanitizedSvgIcon(b, (c.iconSvg.viewBox)||"0 0 24 24", c.iconSvg.content); }
        else { b.textContent=(c&&c.label)||""; }
        b.addEventListener("click", function(ev){ ev.stopPropagation(); pick(idx, {focus:false}); });
        grp.appendChild(b); btns.push(b);
      })(segs[i], i); }
      paint(ctl.v); d.appendChild(grp); menuEl.appendChild(d);
      var ridx=L.rows.length;
      function applyNavAria(){
        d.setAttribute("role","menuitem");
        d.setAttribute("tabindex","-1");
        d.setAttribute("aria-label", buildRichNavAriaLabel({kind:"segmented", label:it.label, value:ctl.v}));
        setSubtreeInert(grp, true);
      }
      applyNavAria();
      var row={el:d, item:it, navigable:en, hasSub:false, kind:"segmented", zone:zone, controlRoot:grp, applyNavAria:applyNavAria,
        adjust:function(delta){
          if(!en) return;
          var plan=planSegmentedEditorAdjust({segments:segs, current:ctl.v, delta:delta});
          if(!plan) return;
          ctl.v=plan.nextIndex;
          if(interactionMode==="editor") applySegmentPlan(plan, true);
          else paint(plan.nextIndex);
          valueChange(it._token, plan.nextIndex);
        },
        enterEditor:function(){
          d.setAttribute("role","none"); d.removeAttribute("aria-label"); setSubtreeInert(grp,false);
          var plan=planSegmentedEditorRoving({segments:segs, selectedIndex:ctl.v});
          for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex", plan.tabIndexes[k]);
          return plan.focusIndex>=0?btns[plan.focusIndex]:null;
        },
        exitEditor:function(){ for(var k=0;k<btns.length;k++) btns[k].setAttribute("tabindex","-1"); applyNavAria(); },
        editorKey:function(e){
          if(e.key==="ArrowRight"){ row.adjust(1); return true; }
          if(e.key==="ArrowLeft"){ row.adjust(-1); return true; }
          if(e.key==="Escape"||e.key==="Enter"){ exitEditor(true); return true; }
          return false;
        }};
      L.rows.push(row);
      d.addEventListener("mouseenter", function(){ if(interactionMode==="editor") return; closeLayersFrom(depth+1); setActiveFromPointer(depth, ridx); });
    }

    function appendItems(menuEl, L, items, depth, zone, layerHasIcon){
      for(var i=0;i<items.length;i++){
        var it=items[i];
        var z=itemZone(it, zone);
        if(it && it.type==="separator"){
          var s=document.createElement("div"); s.className="fb-sep"; s.setAttribute("part","separator");
          stampSep(s, depth, z, false);
          menuEl.appendChild(s); continue;
        }
        if(it && it.type==="nowplaying"){ buildNowPlaying(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        if(it && it.type==="rating"){ buildRating(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        if(it && it.type==="slider"){ buildSlider(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        if(it && it.type==="segmented"){ buildSegmented(menuEl, L, it, !(it.enabled===false), depth, z); continue; }
        var hasSub=!!(it && it.submenu && it.submenu.length);
        var en=!(it && it.enabled===false);
        var checkable=isExplicitCheckable(it);
        var d=document.createElement("div");
        d.className="fb-item nrm"+(!en?" disabled":"")+(it&&it.checked?" checked":"")+(hasSub?" has-sub":"");
        d.setAttribute("part","item");
        stampItem(d, it, depth, z);
        // ARIA: normal / checkable / submenu. Shared menu.show path benefits.
        var role=resolveNavRowRole({kind:hasSub?"submenu":(it&&it.type)||"normal", checkable:checkable, checked:!!(it&&it.checked)});
        d.setAttribute("role", role);
        d.setAttribute("tabindex","-1");
        applyDisabledAria(d, en);
        if(checkable) d.setAttribute("aria-checked", it&&it.checked?"true":"false");
        if(hasSub){ d.setAttribute("aria-haspopup","menu"); d.setAttribute("aria-expanded","false"); }
        if(layerHasIcon){
          var ic=document.createElement("span"); ic.className="fb-item-ico"; ic.setAttribute("part","item-icon"); ic.setAttribute("aria-hidden","true");
          var iv=it&&it.iconSvg;
          if(iv&&iv.content){ mountSanitizedSvgIcon(ic, iv.viewBox||"0 0 24 24", iv.content); }
          d.appendChild(ic);
        }
        var lab=document.createElement("span"); lab.setAttribute("part","item-label"); lab.textContent=(it&&it.label)||""; d.appendChild(lab);
        if(hasSub){ var ar=document.createElement("span"); ar.className="fb-arrow"; ar.setAttribute("part","submenu-arrow"); ar.setAttribute("aria-hidden","true"); ar.textContent="\u25B6"; d.appendChild(ar); }
        menuEl.appendChild(d);
        var row={el:d, item:it, navigable:(en&&(hasSub||(it&&it._token))), hasSub:hasSub, zone:z, checkable:checkable};
        var idx=L.rows.length; L.rows.push(row);
        (function(r, ridx){
          r.el.addEventListener("mouseenter", function(){
            if(interactionMode==="editor") return;
            if(r.navigable && r.hasSub && layers[depth+1] && layers[depth+1].parentRowIdx===ridx){ setActiveFromPointer(depth, ridx); return; }   // 幂等：本项子菜单已开则不重开（bug2 防抖）
            closeLayersFrom(depth+1);
            setActiveFromPointer(depth, ridx);
            if(r.navigable && r.hasSub) openSub(depth, ridx);
          });
          r.el.addEventListener("click", function(){
            if(!r.navigable) return;
            if(r.hasSub){ closeLayersFrom(depth+1); openSub(depth, ridx); setActive(depth+1, firstNav(depth+1)); }
            else select(r.item._token);
          });
        })(row, idx);
      }
    }

    function itemsHaveIcon(items){
      for(var hi=0;hi<(items||[]).length;hi++){ var hit=items[hi]; if(isNormalKind(hit)&&hit.iconSvg&&hit.iconSvg.content) return true; }
      return false;
    }

    function buildLayer(menuEl, items, depth, zone){
      var L={el:menuEl, rows:[], active:-1};
      layers[depth]=L; if(layers.length>depth+1) layers.length=depth+1;
      var layerHasIcon=itemsHaveIcon(items);
      if(layerHasIcon) menuEl.classList.add("has-icons");
      appendItems(menuEl, L, items||[], depth, zone, layerHasIcon);
      return L;
    }

    // Zones model: one navigation layer spanning all zones; icon column unified at root.
    function buildZonesRoot(menuEl, zones, depth){
      var L={el:menuEl, rows:[], active:-1};
      layers[depth]=L; if(layers.length>depth+1) layers.length=depth+1;
      var present=[];
      for(var zi=0;zi<(zones||[]).length;zi++){
        var z=zones[zi];
        if(z && z.items && z.items.length) present.push(z);
      }
      var layerHasIcon=false;
      for(var pi=0;pi<present.length;pi++){ if(itemsHaveIcon(present[pi].items)){ layerHasIcon=true; break; } }
      if(layerHasIcon) menuEl.classList.add("has-icons");
      for(var i=0;i<present.length;i++){
        if(i>0){
          var zs=document.createElement("div");
          zs.className="fb-sep fb-zone-separator";
          zs.setAttribute("part","separator");
          // Inter-zone separator inherits the preceding zone.
          var prevZone=present[i-1].id||"";
          stampSep(zs, depth, prevZone, true);
          menuEl.appendChild(zs);
        }
        var zoneId=present[i].id||"top";
        var zoneEl=document.createElement("div");
        zoneEl.className="fb-zone";
        zoneEl.setAttribute("data-zone", zoneId);
        menuEl.appendChild(zoneEl);
        appendItems(zoneEl, L, present[i].items, depth, zoneId, layerHasIcon);
      }
      return L;
    }

    function openSub(parentDepth, rowIdx){
      var L=layers[parentDepth]; if(!L || !L.rows[rowIdx]) return;
      var content = !!(cur && cur.windowModel==="contentSized");
      if(content && parentDepth>=1) return;   // 内容窗仅 1 层子菜单；2+ 层不展开（深层）
      if(cur && cur.windowModel==="submenu") return;  // independent submenu remains a leaf surface
      var parentEl=L.rows[rowIdx].el; var items=L.rows[rowIdx].item.submenu;
      var parentZone=L.rows[rowIdx].zone||itemZone(L.rows[rowIdx].item, "");
      var parentId=(L.rows[rowIdx].item&&L.rows[rowIdx].item.id)||"";
      var sub=document.createElement("div");
      sub.className="fb-menu fb-submenu fb-hidden";
      sub.setAttribute("part","submenu");
      sub.setAttribute("role","menu");
      sub.setAttribute("data-depth", String(parentDepth+1));
      if(parentId) sub.setAttribute("data-parent-item-id", parentId);
      if(parentZone) sub.setAttribute("data-zone", parentZone);
      // Insert hidden, build content, measure while invisible, position, then show.
      hideMenuEl(sub);
      mountHost().appendChild(sub);
      buildLayer(sub, items, parentDepth+1, parentZone);
      layers[parentDepth+1].parentRowIdx = rowIdx;   // 子菜单归属父项（hover 幂等，bug2）
      parentEl.setAttribute("aria-expanded","true");
      // Temporary measure pass: keep the node invisible to the user, then clear
      // inline display so the final visible state stays CSS-owned.
      // Off-screen measure must not steal focus.
      sub.style.setProperty("visibility","hidden","important");
      sub.style.setProperty("display","block","important");
      var pr=parentEl.getBoundingClientRect(), sw=sub.offsetWidth, sh=sub.offsetHeight;
      if(content){
        // The submenu is rendered by a distinct tight HWND. This temporary DOM
        // node is used only to measure the panel before its native host is
        // positioned; no DWM-backed reservation exists in the root HWND.
        var geom=placedGeometry||{rootSlotW:pr.right,subSlotW:Math.max(0,window.innerWidth-pr.right),viewportH:window.innerHeight,virtualViewportH:window.innerHeight,virtualRootTop:0};
        var subStyle=visibleSubmenuStyle({rootSlotW:geom.rootSlotW,subSlotW:geom.subSlotW,viewportH:geom.viewportH,parentTop:pr.top,submenuHeight:sh});
        applyContentSizedGeometry(sub,subStyle);
      } else {
        var x=pr.right-2; if(x+sw>window.innerWidth) x=Math.max(0, pr.left-sw+2);
        var y=pr.top-4; if(y+sh>window.innerHeight) y=Math.max(0, window.innerHeight-sh);
        sub.style.left=x+"px"; sub.style.top=y+"px";
      }
      sub.style.removeProperty("display");
      sub.style.removeProperty("visibility");
      showMenuEl(sub);
      if(content){
        var virtualPlacement=submenuWindowPlacement({
          rootSlotW:geom.rootSlotW, viewportH:geom.viewportH,
          virtualViewportH:geom.virtualViewportH, virtualRootTop:geom.virtualRootTop,
          parentTop:pr.top, submenuHeight:sh
        });
        var dpr=window.devicePixelRatio||1;
        reportSubmenuPanel({
          x:Math.round(virtualPlacement.x*dpr), y:Math.round(virtualPlacement.y*dpr),
          w:Math.ceil(sw*dpr), h:Math.ceil(virtualPlacement.h*dpr),
          parentToken:(L.rows[rowIdx].item&&L.rows[rowIdx].item._token)||""
        });
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
      cleanupMenuInteraction();
      closeLayersFrom(0); layers.length=0;
      var root=document.getElementById("menu"); root.innerHTML=""; root.classList.remove("in"); root.classList.remove("out");
      root.classList.toggle("fb-submenu", !!(st && st.windowModel==="submenu"));
      clearContentSizedGeometry(root);
      placedGeometry=null;
      root.setAttribute("role","menu");
      root.setAttribute("data-depth","0");
      var content = !!(st && st.windowModel==="contentSized");
      var submenuSurface = !!(st && st.windowModel==="submenu");
      document.body.classList.toggle("content-sized", content);
      var useZones = !!(st && st.overlayModel==="trayZones" && st.layoutMode==="zones" && st.zones && st.zones.length);
      var hasContent = useZones
        ? st.zones.some(function(z){ return z && z.items && z.items.length; })
        : !!(st && st.items && st.items.length);
      if(!st || !st.visible || !hasContent){ hideMenuEl(root); return; }
      if(useZones) buildZonesRoot(root, st.zones, 0);
      else buildLayer(root, st.items, 0, "");
      // 前端样式接管（S-CSS）：每次 render 覆盖 fb-user 防串味；cssReplace=true 时禁用默认样式（仅留 fb-user+fb-protected）。
      // 务必在 measureAndReport / placeRoot 之前应用，保证测量基于最终样式。
      var userStyle=document.getElementById("fb-user");
      if(userStyle) userStyle.textContent=(st.css||"");
      var defStyle=document.getElementById("fb-default");
      if(defStyle) defStyle.disabled=!!st.cssReplace;
      showMenuEl(root);
      if(content){
        // 内容窗：菜单贴客户区原点；窗口尺寸/定位由 C++ OnContentMeasured 据测量回报完成。
        // Off-screen measure must not steal focus — focus after first placed.
        applyContentSizedGeometry(root,{left:"0px",top:"0px",maxWidth:"none",maxHeight:"none"});
        setActive(0, firstNav(0), {focus:false});
        pendingRootFocus=true;
        measureAndReport();
      } else if(!submenuSurface) {
        var dpr=window.devicePixelRatio||1;   // anchor 为物理像素，转 CSS 像素再定位（高 DPI 修正）
        placeRoot(root, (st.anchorX||0)/dpr, (st.anchorY||0)/dpr);
        setActive(0, firstNav(0));   // WAI-ARIA：菜单打开焦点置首项
      }
      if(submenuSurface){
        setActive(0, firstNav(0), {focus:false});
        reportSubmenuSurfaceReady();
      }
    }

    function reportSubmenuPanel(rect){
      try{
        if(!(window.fb2k&&fb2k.invoke)) return;
        if(!rect) submenuPanelOpen=false;
        var payload={menuId:(cur&&cur.menuId)||"",visible:!!rect,sequence:++submenuPanelSequence};
        if(rect){ payload.x=rect.x; payload.y=rect.y; payload.w=rect.w; payload.h=rect.h; payload.parentToken=rect.parentToken||""; }
        var pending=fb2k.invoke("menu.__submenuPanel",payload);
        if(rect&&pending&&pending.then){ pending.then(function(result){
          if((!result||result.success===false)&&!submenuPanelOpen){ requestAnimationFrame(function(){ reportSubmenuPanel(rect); }); }
        }); }
      }catch(e){}
    }

    function measureElement(el){
      var r=el.getBoundingClientRect();
      return {w:Math.max(el.scrollWidth,Math.ceil(r.width)),h:Math.max(el.scrollHeight,Math.ceil(r.height))};
    }

    function measureFirstLevelSubmenus(){
      var out=[];
      var rootLayer=layers[0]; if(!rootLayer) return out;
      for(var i=0;i<rootLayer.rows.length;i++){
        var row=rootLayer.rows[i];
        if(!row||!row.hasSub||!row.item||!row.item.submenu) continue;
        var sub=document.createElement("div");
        sub.className="fb-menu fb-submenu fb-measure-submenu";
        sub.setAttribute("role","menu");
        sub.style.setProperty("position","fixed","important");
        sub.style.setProperty("left","-32000px","important");
        sub.style.setProperty("top","-32000px","important");
        sub.style.setProperty("display","block","important");
        sub.style.setProperty("visibility","hidden","important");
        sub.style.setProperty("max-width","none","important");
        sub.style.setProperty("max-height","none","important");
        mountHost().appendChild(sub);
        var oldLength=layers.length;
        buildLayer(sub,row.item.submenu,1,row.zone||"");
        out.push(measureElement(sub));
        if(sub.parentNode) sub.parentNode.removeChild(sub);
        layers.length=oldLength;
        layers[0]=rootLayer;
      }
      return out;
    }

    // Wait for fonts and two equal animation-frame measurements before reporting
    // actual root + first-level submenu maxima. The host validates all integers
    // before atomically consuming the measure state.
    var _measureRaf=0;
    var _measureGeneration=0;
    function measureAndReport(){
      if(!cur || cur.windowModel!=="contentSized") return;
      if(_measureRaf){ try{ cancelAnimationFrame(_measureRaf); }catch(e){} }
      var generation=++_measureGeneration;
      var begin=function(){
        var previous=null, equalFrames=0;
        function sample(){
          _measureRaf=0;
          if(generation!==_measureGeneration||!cur||cur.windowModel!=="contentSized") return;
          var root=document.getElementById("menu"); if(!root) return;
          var payload=buildMeasurePayload({menuId:(cur&&cur.menuId)||"",root:measureElement(root),submenus:measureFirstLevelSubmenus(),dpr:window.devicePixelRatio||1});
          if(payload.root.w<=0||payload.root.h<=0) return;
          if(sameMeasurePayload(previous,payload)) equalFrames++; else equalFrames=0;
          previous=payload;
          if(equalFrames>=1){
            try{ if(window.fb2k&&fb2k.invoke) fb2k.invoke("menu.__ready",payload); }catch(e){}
            return;
          }
          _measureRaf=requestAnimationFrame(sample);
        }
        _measureRaf=requestAnimationFrame(sample);
      };
      try{
        if(document.fonts&&document.fonts.ready&&document.fonts.ready.then){ document.fonts.ready.then(begin,begin); }
        else begin();
      }catch(e){ begin(); }
    }

    function onPlaced(ev){
      // 固定窗设计：窗口不 resize，子菜单无需隐藏淡入；仅首次定位触发入场动画。
      if(ev && ev.first){
        applyPlacedGeometry(ev);
        var root=document.getElementById("menu");
        requestAnimationFrame(function(){
          root.classList.add("in");
          // ContentSized: real focus only after first visible place.
          if(pendingRootFocus){
            pendingRootFocus=false;
            setActive(0, layers[0]?layers[0].active:firstNav(0), {focus:true});
          }
        });
      }
    }

    // Single window-capture keydown: mode split first, handle once.
    function onKey(e){
      if(!layers.length) return;
      // Editor mode: only editor handler; navigation never sees the same key.
      if(interactionMode==="editor"){
        if(editorCtx && editorCtx.row && editorCtx.row.editorKey){
          if(editorCtx.row.editorKey(e)) e.preventDefault();
        }
        return;
      }
      var depth=layers.length-1, L=layers[depth]; if(!L) return;
      var r=L.rows[L.active];
      switch(e.key){
        case "ArrowDown": moveActive(depth,1); e.preventDefault(); break;
        case "ArrowUp":   moveActive(depth,-1); e.preventDefault(); break;
        case "Home": { var fi=firstNav(depth); if(fi>=0) setActive(depth,fi); e.preventDefault(); break; }
        case "End":  { var li=lastNav(depth); if(li>=0) setActive(depth,li); e.preventDefault(); break; }
        case "ArrowRight": {
          if(r&&r.navigable&&r.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); e.preventDefault(); break; }
          if(r&&isRichKind(r.kind)){ enterEditor(depth, L.active); e.preventDefault(); break; }
          e.preventDefault(); break;
        }
        case "ArrowLeft": {
          var leftAction=resolveMenuArrowLeftAction({windowModel:cur&&cur.windowModel,depth:depth});
          if(leftAction.kind==="dismissSubmenu"){
            // The independent submenu HWND has depth zero. Ask its host to
            // close only this child surface and restore the root parent row.
            dismiss(leftAction.reason);
          } else if(leftAction.kind==="closeLayer"){
            var parentIdx=(layers[depth]&&layers[depth].parentRowIdx!=null)?layers[depth].parentRowIdx:-1;
            closeLayersFrom(depth);
            if(parentIdx>=0) setActive(depth-1, parentIdx, {focus:true});
          }
          e.preventDefault(); break;
        }
        case "Enter": case " ": {
          if(r&&r.navigable){
            if(r.hasSub){ openSub(depth,L.active); setActive(depth+1, firstNav(depth+1)); }
            else if(isRichKind(r.kind)){ enterEditor(depth, L.active); }
            else select(r.item._token);
          }
          e.preventDefault(); break;
        }
        case "Escape":
          if(depth>0){
            var pIdx=(layers[depth]&&layers[depth].parentRowIdx!=null)?layers[depth].parentRowIdx:-1;
            closeLayersFrom(depth);
            if(pIdx>=0) setActive(depth-1, pIdx, {focus:true});
          } else { dismiss("escape"); }
          e.preventDefault(); break;
        default: break;
      }
    }

    function pull(){ try{ if(window.fb2k&&fb2k.invoke){ var p=fb2k.invoke("menu.__getMenuState"); if(p&&p.then) p.then(render); } }catch(e){} }
    function inMenu(t){ return t && t.closest && t.closest(".fb-menu"); }
    window.addEventListener("mousedown", function(e){ if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("contextmenu", function(e){ e.preventDefault(); if(!inMenu(e.target)) dismiss("outside"); }, true);
    window.addEventListener("keydown", onKey, true);
    if(window.fb2k && fb2k.on){ fb2k.on("menu:show", pull); fb2k.on("menu:__placed", onPlaced); fb2k.on("menu:__submenuOpened", function(){ submenuPanelOpen=true; }); fb2k.on("menu:__submenuVisible", function(){ setActive(0,firstNav(0),{focus:true}); }); fb2k.on("menu:__submenuClosed", function(){ submenuPanelOpen=false; var parentIdx=(layers[1]&&layers[1].parentRowIdx!=null)?layers[1].parentRowIdx:-1; closeLayersFrom(1); if(parentIdx>=0) setActive(0,parentIdx,{focus:true}); }); fb2k.on("menu:__hide", function(){ cleanupMenuInteraction(); var root=document.getElementById("menu"); if(root){ root.classList.remove("in"); root.classList.add("out"); } }); }
    pull();
