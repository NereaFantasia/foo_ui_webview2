// OriginPolicy.h - which page addresses the host trusts.
//
// Free of foobar2000 SDK dependencies so the rules can be unit-tested. Every
// comparison is between parsed origins; a raw URL is never prefix-matched.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace origin_policy {

// Origin of the component's own virtual host, where local templates are served.
inline constexpr std::wstring_view kBundledOrigin = L"https://foo-ui-webview2.local";

// Source a WebView reports for a page loaded with NavigateToString, such as the
// built-in page and the menu overlay.
inline constexpr std::wstring_view kInlinePageSource = L"about:blank";

// Origin of an http or https URL as "scheme://host[:port]", with scheme and host
// lowercased and the scheme's default port dropped. Empty for any other scheme, an
// empty host, a port that is not a number in 1..65535, and any URL carrying user
// info: "https://foo-ui-webview2.local:1@example.com/" is a page on example.com.
std::wstring OriginOf(std::wstring_view url);

// Origin of the configured development server; empty when the server is off or its
// URL has no origin. urlUtf8 is the stored setting.
std::wstring DevServerOrigin(bool useDevServer, const std::string& urlUtf8);

// Whether a page whose top-level URL is `url` may use the bridge: an inline page, a
// page on the bundled origin, or a page on an origin the host itself loaded into
// this WebView (`registered`, already normalised by OriginOf).
bool IsTrustedPage(std::wstring_view url, const std::vector<std::wstring>& registered);

}  // namespace origin_policy
