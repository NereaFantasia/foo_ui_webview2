// .vitepress/theme/index.ts
import DefaultTheme from 'vitepress/theme'
import type { Theme, Router } from 'vitepress'
import './custom.css'
import {
  isCorrespondingLocalePath,
  localeSeoUrls,
  rewriteHrefWithPendingHash,
} from './locale-hash.mjs'

function syncLocaleSeoHead(pathname: string) {
  const urls = localeSeoUrls(pathname)
  const canonical = document.querySelector<HTMLLinkElement>('link[rel="canonical"]')
  if (canonical && canonical.href !== urls.canonical) canonical.href = urls.canonical

  const alternateUrls: Record<string, string> = {
    en: urls.en,
    'zh-CN': urls.zh,
    'x-default': urls.xDefault,
  }
  for (const alternate of document.querySelectorAll<HTMLLinkElement>('link[rel="alternate"][hreflang]')) {
    const expected = alternateUrls[alternate.hreflang]
    if (expected && alternate.href !== expected) alternate.href = expected
  }

  const ogUrl = document.querySelector<HTMLMetaElement>('meta[property="og:url"]')
  if (ogUrl && ogUrl.content !== urls.canonical) ogUrl.content = urls.canonical
}

function scrollToCurrentFragment() {
  if (!window.location.hash) return
  const id = decodeURIComponent(window.location.hash.slice(1))
  document.getElementById(id)?.scrollIntoView()
}

function scheduleRouteDomSync() {
  for (const delay of [0, 100, 500]) {
    window.setTimeout(() => {
      syncLocaleSeoHead(window.location.pathname)
      scrollToCurrentFragment()
    }, delay)
  }
}

function installLocaleSeoObserver() {
  const observer = new MutationObserver(() => {
    syncLocaleSeoHead(window.location.pathname)
  })
  observer.observe(document.head, {
    attributes: true,
    childList: true,
    subtree: true,
    attributeFilter: ['content', 'href'],
  })
  syncLocaleSeoHead(window.location.pathname)
}

/**
 * Preserve pathname + hash across locale switches.
 *
 * Root cause (auditor real-browser regression):
 * VitePress createRouter() registers a window capture click handler before
 * theme enhanceApp. A document-level preventDefault + location.assign therefore
 * cannot stop router.go(); when the language-menu href is hash-less, SPA
 * navigation lands without the fragment. stopImmediatePropagation on a later
 * listener also cannot cancel listeners that already ran.
 *
 * Fix: hook router.onBeforeRouteChange (runs before history.pushState). If the
 * live location.hash is set, the target href lacks a fragment, and the target
 * is the corresponding page in the other locale, cancel and re-enter
 * router.go(pathname + search + hash).
 *
 * Pure helpers live in ./locale-hash.mjs (Node 20-native ESM) for unit tests.
 */
function installLocaleHashPreservation(router: Router) {
  if (typeof window === 'undefined') return
  installLocaleSeoObserver()

  let reentering = false
  let lastSourceHash = ''

  const prevBefore = router.onBeforeRouteChange
  router.onBeforeRouteChange = async (href: string) => {
    if (reentering) return prevBefore ? prevBefore(href) : undefined

    const liveHash = window.location.hash
    if (liveHash) {
      const rewritten = rewriteHrefWithPendingHash(href, liveHash)
      if (
        rewritten &&
        isCorrespondingLocalePath(window.location.pathname, rewritten)
      ) {
        lastSourceHash = liveHash
        reentering = true
        try {
          await router.go(rewritten)
        } finally {
          reentering = false
        }
        return false
      }
    }

    return prevBefore ? prevBefore(href) : undefined
  }

  const prevAfter = router.onAfterRouteChange
  router.onAfterRouteChange = async (toHref: string) => {
    // Safety net if a rewritten locale navigation still settled without hash.
    if (lastSourceHash && !window.location.hash) {
      history.replaceState(
        history.state,
        '',
        window.location.pathname + window.location.search + lastSourceHash,
      )
    }
    lastSourceHash = ''
    if (prevAfter) await prevAfter(toHref)
    scheduleRouteDomSync()
  }
}

export default {
  extends: DefaultTheme,
  enhanceApp({ router }) {
    installLocaleHashPreservation(router)
  },
} satisfies Theme

export {
  applyLocaleHashClick,
  isCorrespondingLocalePath,
  localeRelativePath,
  localeSeoUrls,
  resolveLocaleHashNavigation,
  rewriteHrefWithPendingHash,
  LOCALE_SWITCH_LABELS,
  LOCALE_MENU_SELECTORS,
  DOCS_BASE,
  PUBLIC_ORIGIN,
} from './locale-hash.mjs'
