// window.fb2k 桥接脚本：invoke、事件订阅与拖放快照等页面侧入口，在每个文档创建时注入。
// 只由 WebViewHostScripts.cpp include（WebViewHost::InjectBridgeScript）。
// MSVC 限制单段字符串字面量的长度（C2026），)" LR"( 的分段位置不要改。
#pragma once

constexpr const wchar_t* kBridgeBootstrapScript = LR"(
        // foobar2000 WebView2 UI Bridge

        // Drag-drop snapshot slot, declared before any page code runs so a
        // synchronous read never hits an undefined global. Runs once per
        // document, so a navigation cannot leave the previous page's paths
        // readable. The host fills it from dnd:* events; null means no drag
        // session has been published to this document.
        //
        // Declared in every frame but only ever filled in the main one, see
        // _isMainFrame below. A subframe therefore reads null here for the
        // lifetime of its document, which is also what makes the accessors
        // answer with an empty array instead of throwing.
        window.__fbDndSession = null;

        window.fb2k = window.fb2k || {
            _callbacks: new Map(),
            _callId: 0,
            
            // 发送请求到 C++
            invoke: function(method, params) {
                return new Promise((resolve, reject) => {
                    // A subframe's chrome.webview.postMessage does not reach
                    // the host, so the request would sit in _callbacks until
                    // the 30s timeout below rejected it with a message that
                    // says nothing about the real cause. Reject now, on the
                    // same _isMainFrame test the drag-drop snapshot uses, so a
                    // framed caller cannot observe the two boundaries
                    // disagreeing.
                    //
                    // Inside the executor rather than above it: a detached call
                    // (const {invoke} = fb2k) leaves this === window, and the
                    // resulting TypeError has to surface as a rejection like it
                    // did before this guard existed, not as a synchronous throw.
                    if (!this._isMainFrame()) {
                        const err = new Error('fb2k.invoke is unavailable in subframes');
                        err.code = 'NOT_SUPPORTED';
                        reject(err);
                        return;
                    }

                    const id = ++this._callId;
                    this._callbacks.set(id, { resolve, reject });
                    
                    window.chrome.webview.postMessage({
                        type: 'invoke',
                        id: id,
                        method: method,
                        params: params || {}
                    });
                    
                    // 超时只说明页面不再等了：宿主可能仍在执行，也可能已执行完，之后到达的应答被丢弃。
                    setTimeout(() => {
                        if (this._callbacks.has(id)) {
                            this._callbacks.delete(id);
                            const err = new Error('Request timeout: ' + method);
                            err.code = 'TIMEOUT';
                            reject(err);
                        }
                    }, 30000);
                });
            },
            
            // 接收 C++ 响应
            _handleResponse: function(id, error, result, code) {
                const callback = this._callbacks.get(id);
                if (callback) {
                    this._callbacks.delete(id);
                    if (error) {
                        const err = new Error(error);
                        if (code) err.code = code;
                        callback.reject(err);
                    } else {
                        callback.resolve(result);
                    }
                }
            },
            
            // 事件监听
            _eventListeners: new Map(),
            
            on: function(event, callback) {
                if (!this._eventListeners.has(event)) {
                    this._eventListeners.set(event, new Set());
                }
                this._eventListeners.get(event).add(callback);
                return () => this.off(event, callback);
            },
            
            off: function(event, callback) {
                const listeners = this._eventListeners.get(event);
                if (listeners) {
                    listeners.delete(callback);
                }
            },
            
            _emit: function(event, data) {
                // 0. Publish the drag-drop snapshot before any listener runs.
                //
                // Must happen here rather than in an SDK wrapper: listeners are
                // stored in _eventListeners and invoked below, so a wrapper
                // registered through on() would be just another entry in that
                // set with no ordering guarantee against page code. A drop
                // handler calling fb.dnd.getPaths() has to observe the current
                // session, not the previous one.
                if (event === 'dnd:enter' || event === 'dnd:drop' || event === 'dnd:leave') {
                    try { this._updateDndSnapshot(event, data); } catch(e) { console.error(e); }
                }

                // 1. Call registered callbacks
                const listeners = this._eventListeners.get(event);
                if (listeners) {
                    listeners.forEach(cb => {
                        try { cb(data); } catch(e) { console.error(e); }
                    });
                }
                
                // 2. Dispatch CustomEvent for window.addEventListener
                try {
                    window.dispatchEvent(new CustomEvent('fb2k:' + event, {
                        detail: data,
                        bubbles: true
                    }));
                } catch(e) { console.error('CustomEvent dispatch error:', e); }
            },

            // Retains the snapshot briefly after a session ends so a drop
            // handler that awaits something before reading paths still sees
            // them. Kept short because the paths are stale by then.
            _dndSnapshotGraceMs: 1500,
            _dndClearTimer: null,

            // Whether this document is the top-level one.
            //
            // AddScriptToExecuteOnDocumentCreated runs in every frame, so
            // without this test a subframe of any origin would get the same
            // snapshot slot filled with real filesystem paths. That drag events
            // do not reach a frame today is a side effect of how the host posts
            // web messages, not a boundary this code states; this states it,
            // on two attributes a framed document cannot redefine.
            //
            // Compared against window, not window.self: the HTML standard marks
            // window and top [LegacyUnforgeable] but self [Replaceable], so one
            // assignment of self = top inside a subframe would make a self-based
            // test answer true. window.top is null in a detached document, which
            // compares unequal and so counts as framed.
            //
            // Reading window.top across origins can throw, and a document that
            // cannot show it is the top one is treated as framed.
            _isMainFrame: function() {
                try {
                    return window.top === window;
                } catch (e) {
                    return false;
                }
            },
    )" LR"(
            // Shortcut targets parallel to paths, padded to the same length so
            // an index that is valid for one is valid for the other. A host that
            // sends no resolvedPaths yields all nulls rather than a short array.
            _dndResolvedPaths: function(paths, resolved) {
                const out = [];
                for (let i = 0; i < paths.length; i++) {
                    const target = Array.isArray(resolved) ? resolved[i] : null;
                    out.push(typeof target === 'string' ? target : null);
                }
                return out;
            },

            _updateDndSnapshot: function(event, data) {
                // Only the main frame ever holds a snapshot. Returning rather
                // than throwing keeps getPaths() / getResolvedPaths() answering
                // an empty array in a frame, so page code that does not know it
                // is framed still runs.
                if (!this._isMainFrame()) {
                    return;
                }

                const d = data || {};
                const sessionId = d.sessionId || '';

                if (this._dndClearTimer !== null) {
                    clearTimeout(this._dndClearTimer);
                    this._dndClearTimer = null;
                }

                if (event === 'dnd:enter') {
                    const paths = Array.isArray(d.paths) ? d.paths.slice() : [];
                    window.__fbDndSession = {
                        sessionId: sessionId,
                        paths: paths,
                        resolvedPaths: this._dndResolvedPaths(paths, d.resolvedPaths),
                        hasFiles: !!d.hasFiles,
                        phase: 'active',
                        publishedAt: performance.now()
                    };
                    return;
                }

                const current = window.__fbDndSession;
                // A leave or drop for some other session is stale; ignoring it
                // keeps a superseded gesture from clearing the live one.
                if (!current || (sessionId && current.sessionId !== sessionId)) {
                    return;
                }

                if (event === 'dnd:drop') {
                    // Drop carries the authoritative list: the source may have
                    // changed it since enter.
                    if (Array.isArray(d.paths)) {
                        current.paths = d.paths.slice();
                        current.resolvedPaths =
                            this._dndResolvedPaths(current.paths, d.resolvedPaths);
                        current.hasFiles = d.paths.length > 0;
                    }
                }

                current.phase = 'ended';
                current.publishedAt = performance.now();

                const endedId = current.sessionId;
                this._dndClearTimer = setTimeout(() => {
                    this._dndClearTimer = null;
                    const snap = window.__fbDndSession;
                    if (snap && snap.sessionId === endedId && snap.phase === 'ended') {
                        window.__fbDndSession = null;
                    }
                }, this._dndSnapshotGraceMs);
            }
        };
        
        // 监听来自 C++ 的消息
        window.chrome.webview.addEventListener('message', (e) => {
            const msg = e.data;
            if (msg.type === 'response') {
                window.fb2k._handleResponse(msg.id, msg.error, msg.result, msg.code);
            } else if (msg.type === 'event') {
                window.fb2k._emit(msg.event, msg.data);
            }
        });
        
        // 全局右键菜单 - 仅在无组件处理时调用原生菜单
        document.addEventListener('contextmenu', (e) => {
            if (e.defaultPrevented) return;  // 组件已处理(playlist-view, library-tree等)
            e.preventDefault();
            window.fb2k.invoke('ui.showContextMenu', { x: e.screenX, y: e.screenY });
        });

        // 页面没接住的拖放一律拒收。Chromium 对没人处理的文件拖动默认答「复制」，
        // 松手后自己去打开那个文件；这里替页面答「不接受」，宿主报给拖动源的光标
        // 就跟页面的意思一致，也不会打开文件。挂在 window 的冒泡阶段，页面自己的
        // 处理先跑。保留浏览器默认的只有两种：文件拖到文件输入框，非文件拖到可编辑
        // 元素（文本放下）。文件拖到文本框时 Chromium 不插入文本，而是去打开文件。
        const fbDndKeepsDefault = (e) => {
            const target = e.target;
            const el = target && target.nodeType === 1 ? target : target && target.parentElement;
            if (!el) return false;
            const types = e.dataTransfer ? e.dataTransfer.types : [];
            if (Array.prototype.includes.call(types, 'Files')) return !!el.closest('input[type=file]');
            return el.isContentEditable || !!el.closest('input, textarea');
        };
        const fbDndRefuse = (e) => {
            if (e.defaultPrevented || fbDndKeepsDefault(e)) return;
            e.preventDefault();
            if (e.dataTransfer) e.dataTransfer.dropEffect = 'none';
        };
        window.addEventListener('dragenter', fbDndRefuse);
        window.addEventListener('dragover', fbDndRefuse);
        window.addEventListener('drop', (e) => {
            if (e.defaultPrevented || fbDndKeepsDefault(e)) return;
            e.preventDefault();
        });
    )" LR"(
        // Keys the host cannot route on its own. Letters, digits, Space and
        // punctuation never reach its accelerator handler; the arrows, Home,
        // End, PageUp and PageDown do, but it leaves them to the page because
        // they edit text. Outside an editable element they go to the
        // foobar2000 keyboard shortcuts. The host answers synchronously whether
        // a shortcut ran, and only then is the default action (scrolling,
        // activating the focused button) cancelled. Runs after the page's own
        // keydown listeners, see fbAfterPageKeydown below.
        const fbIsTextKeyCode = (k) => k === 32 || (k >= 48 && k <= 57) || (k >= 65 && k <= 90) ||
            (k >= 96 && k <= 111) || (k >= 186 && k <= 192) || (k >= 219 && k <= 223);
        // composedPath()[0] rather than target: on window, target is retargeted
        // to the shadow host, which hides an input inside a component.
        const fbIsEditableTarget = (e) => {
            const path = typeof e.composedPath === 'function' ? e.composedPath() : [];
            const first = path.length ? path[0] : e.target;
            const el = first && first.nodeType === 1 ? first : first && first.parentElement;
            if (!el) return false;
            return el.isContentEditable || !!el.closest('input, textarea, select');
        };
        // While a key whose press ran a shortcut is held down, its repeats only
        // cancel the default action; the shortcut runs once per press.
        let fbRelayedKey = '';
        const fbRelayUnhandledKey = (e) => {
            const signature = e.keyCode + ':' + e.ctrlKey + e.shiftKey + e.altKey + e.metaKey;
            if (e.repeat) {
                if (signature === fbRelayedKey && !e.defaultPrevented) e.preventDefault();
                return;
            }
            fbRelayedKey = '';
            if (e.defaultPrevented || e.isComposing || e.keyCode === 229) return;
            if (e.altKey || e.metaKey) return;
            const k = e.keyCode;
            const caretKey = k >= 35 && k <= 40;   // End, Home and the arrows
            const pageKey = k === 33 || k === 34;  // PageUp, PageDown
            if (e.ctrlKey ? !caretKey : !(caretKey || pageKey || fbIsTextKeyCode(k))) return;
            let mainFrame = false;
            try { mainFrame = window.top === window; } catch (err) { mainFrame = false; }
            if (!mainFrame || fbIsEditableTarget(e)) return;
            const objects = window.chrome && window.chrome.webview && window.chrome.webview.hostObjects;
            if (!objects) return;
            let ran = false;
            try {
                // Modifiers as MOD_CONTROL (2) and MOD_SHIFT (4).
                ran = objects.sync.fb2kShortcutRelay.relay(k, (e.ctrlKey ? 2 : 0) | (e.shiftKey ? 4 : 0)) === true;
            } catch (err) {
                return;
            }
            if (ran) {
                fbRelayedKey = signature;
                e.preventDefault();
            }
        };
    )" LR"(
        // F5 reaches the page only after a trusted page has finished loading;
        // otherwise the host reloads without asking the page. A listener that
        // cancels the key press keeps F5 for the page. Inside a frame the top
        // document is reloaded, which fails when it has another origin.
        const fbReloadUnlessCancelled = (e) => {
            if (e.defaultPrevented || e.repeat) return;
            try { window.top.location.reload(); } catch (err) { /* cross-origin top document */ }
        };
        // Both keydown decisions need e.defaultPrevented as the page leaves it.
        // During dispatch, a capture listener on window appends a one-shot
        // bubble listener to window; it runs after every keydown listener the
        // page added before the key press, those on window included. The
        // original add/removeEventListener are used so that a page patching
        // them neither sees nor wraps these listeners.
        const fbAddListener = EventTarget.prototype.addEventListener;
        const fbRemoveListener = EventTarget.prototype.removeEventListener;
        let fbAfterPageKeydown = null;
        fbAddListener.call(window, 'keydown', (e) => {
            // Left over when the page stopped the propagation of an earlier key press.
            if (fbAfterPageKeydown) fbRemoveListener.call(window, 'keydown', fbAfterPageKeydown);
            const listener = (ev) => {
                fbRemoveListener.call(window, 'keydown', listener);
                if (fbAfterPageKeydown === listener) fbAfterPageKeydown = null;
                if (ev !== e) return;
                if (ev.keyCode === 116) fbReloadUnlessCancelled(ev);  // F5
                else fbRelayUnhandledKey(ev);
            };
            fbAfterPageKeydown = listener;
            fbAddListener.call(window, 'keydown', listener);
        }, true);

        console.log('[fb2k] Bridge initialized');
    )";
