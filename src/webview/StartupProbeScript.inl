// 启动辅助脚本：自动发 windowReady，按启动遮罩消失判定 visualReady，并上报 startupProbe。
// 在每个文档创建时注入，只由 WebViewHostScripts.cpp include（WebViewHost::InjectBridgeScript）。
// MSVC 限制单段字符串字面量的长度（C2026），)" LR"( 的分段位置不要改。
#pragma once

constexpr const wchar_t* kStartupProbeScript = LR"(
        (function() {
            var _notified = false;
            var _scheduled = false;

            function postWindowReady(detail) {
                if (_notified) return;
                _notified = true;
                try {
                    window.chrome.webview.postMessage({
                        type: 'windowReady',
                        source: (detail && detail.source) || 'explicit',
                        href: location.href,
                        readyState: document.readyState
                    });
                } catch(e) {}
                // After windowReady, start overlay-gone detection → send visualReady
                startOverlayDetection();
            }

            function scheduleWindowReady(detail) {
                if (_notified || _scheduled) return;
                _scheduled = true;
                setTimeout(function() {
                    _scheduled = false;
                    postWindowReady(detail);
                }, 0);
            }

            // === overlay-gone detection -> send visualReady ===
            var _visualReadyNotified = false;

            function postVisualReady(source) {
                if (_visualReadyNotified) return;
                _visualReadyNotified = true;
                try {
                    window.chrome.webview.postMessage({
                        type: 'visualReady',
                        source: source,
                        href: location.href,
                        readyState: document.readyState
                    });
                } catch(e) {}
            }

            function isOverlayGone() {
                var overlay = document.getElementById('server-loading');
                if (!overlay) return true;
                if (!overlay.offsetWidth && !overlay.offsetHeight && overlay.offsetParent === null) return true;
                try {
                    var style = window.getComputedStyle(overlay);
                    if (style.display === 'none' || style.visibility === 'hidden') return true;
                } catch(e) {}
                return false;
            }

            function startOverlayDetection() {
                if (_visualReadyNotified) return;
                // Immediate check: themes without overlay pass immediately
                if (isOverlayGone()) {
                    postVisualReady('overlay-none');
                    return;
                }
                // MutationObserver for DOM removal / attribute changes
                var observer = new MutationObserver(function() {
                    if (isOverlayGone()) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-removed');
                    }
                });
                observer.observe(document.documentElement, {
                    childList: true, subtree: true, attributes: true
                });
                // Polling fallback (100ms interval, in case MutationObserver misses style changes)
                var pollId = setInterval(function() {
                    if (isOverlayGone()) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-poll');
                    }
                }, 100);
                // Safety timeout: 10s (coordinator fallback will fire earlier)
                setTimeout(function() {
                    if (!_visualReadyNotified) {
                        observer.disconnect();
                        clearInterval(pollId);
                        postVisualReady('overlay-timeout');
                    }
                }, 10000);
            }
    )" LR"(
            function roundNumber(value) {
                if (typeof value !== 'number' || !isFinite(value)) return -1;
                return Math.round(value * 1000) / 1000;
            }

            function cropText(value) {
                if (value == null) return '';
                value = String(value);
                return value.length > 120 ? value.slice(0, 117) + '...' : value;
            }

            function getPaintMetric(name) {
                try {
                    var entries = performance.getEntriesByType ? performance.getEntriesByType('paint') : [];
                    for (var i = 0; i < entries.length; i++) {
                        if (entries[i] && entries[i].name === name) {
                            return roundNumber(entries[i].startTime);
                        }
                    }
                } catch (e) {}
                return -1;
            }

            function getStyleSnapshot(element) {
                if (!element || !window.getComputedStyle) {
                    return { backgroundColor: '', backgroundImage: '', opacity: '', visibility: '', display: '' };
                }

                try {
                    var style = window.getComputedStyle(element);
                    return {
                        backgroundColor: cropText(style.backgroundColor || ''),
                        backgroundImage: cropText(style.backgroundImage || ''),
                        opacity: cropText(style.opacity || ''),
                        visibility: cropText(style.visibility || ''),
                        display: cropText(style.display || '')
                    };
                } catch (e) {
                    return { backgroundColor: '', backgroundImage: '', opacity: '', visibility: '', display: '' };
                }
            }

            var _probeSent = Object.create(null);

            function postStartupProbe(phase) {
                if (_probeSent[phase]) return;
                _probeSent[phase] = true;

                try {
                    var html = document.documentElement;
                    var body = document.body;
                    var centerElement = null;
                    if (document.elementFromPoint && window.innerWidth > 0 && window.innerHeight > 0) {
                        centerElement = document.elementFromPoint(
                            Math.floor(window.innerWidth / 2),
                            Math.floor(window.innerHeight / 2)
                        );
                    }

                    var htmlStyle = getStyleSnapshot(html);
                    var bodyStyle = getStyleSnapshot(body);
                    var centerStyle = getStyleSnapshot(centerElement);

                    window.chrome.webview.postMessage({
                        type: 'startupProbe',
                        phase: phase,
                        href: location.href,
                        readyState: document.readyState,
                        visibilityState: document.visibilityState,
                        perfNow: roundNumber(performance.now ? performance.now() : -1),
                        firstPaint: getPaintMetric('first-paint'),
                        firstContentfulPaint: getPaintMetric('first-contentful-paint'),
                        htmlBackground: htmlStyle.backgroundColor,
                        htmlBackgroundImage: htmlStyle.backgroundImage,
                        htmlOpacity: htmlStyle.opacity,
                        bodyBackground: bodyStyle.backgroundColor,
                        bodyBackgroundImage: bodyStyle.backgroundImage,
                        bodyOpacity: bodyStyle.opacity,
                        centerTag: centerElement ? cropText(centerElement.tagName || '') : '',
                        centerId: centerElement ? cropText(centerElement.id || '') : '',
                        centerClass: centerElement ? cropText(centerElement.className || '') : '',
                        centerBackground: centerStyle.backgroundColor,
                        centerBackgroundImage: centerStyle.backgroundImage,
                        centerOpacity: centerStyle.opacity,
                        innerWidth: window.innerWidth || 0,
                        innerHeight: window.innerHeight || 0
                    });
                } catch (e) {}
            }

            function scheduleVisualProbeSeries(phaseBase) {
                if (window.requestAnimationFrame) {
                    window.requestAnimationFrame(function() {
                        window.requestAnimationFrame(function() {
                            postStartupProbe(phaseBase + '-raf2');
                        });
                    });
                } else {
                    setTimeout(function() {
                        postStartupProbe(phaseBase + '-timeout');
                    }, 0);
                }

                setTimeout(function() {
                    postStartupProbe(phaseBase + '-t250');
                }, 250);
            }

            window.__fb2kNotifyWindowReady = function(detail) {
                postStartupProbe('explicit-window-ready');
                postWindowReady(detail);
            };

            postStartupProbe('helper-injected');

            if (document.readyState === 'complete') {
                postStartupProbe('auto-ready-state-complete');
                scheduleVisualProbeSeries('auto-ready-state-complete');
                scheduleWindowReady({ source: 'auto-ready-state-complete' });
            } else {
                window.addEventListener('load', function() {
                    postStartupProbe('auto-load');
                    scheduleVisualProbeSeries('auto-load');
                    scheduleWindowReady({ source: 'auto-load' });
                }, { once: true });
            }
        })();
    )";
