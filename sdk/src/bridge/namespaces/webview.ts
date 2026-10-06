import { call } from '../call.js';

/**
 * `webview` — facts about the calling WebView itself.
 */
export const webview = {
    /**
     * Report where the host loaded this page from: the development server, a URL, a
     * template folder, or the built-in "Frontend not found" page. The answer is the record
     * the host made when it submitted the navigation; only `activeTemplateName` is read at
     * the time of the call.
     */
    getSource: () => call('webview.getSource'),
};
