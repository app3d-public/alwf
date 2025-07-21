const Framework = (function () {
    function post(message) {
        if (window.chrome?.webview?.postMessage) {
            window.chrome.webview.postMessage(message);
            return true;
        }

        if (window.webkit?.messageHandlers?.handler?.postMessage) {
            const json = typeof message === 'string' ? message : JSON.stringify(message);
            window.webkit.messageHandlers.handler.postMessage(json);
            return true;
        }

        console.warn("No native bridge found!");
        return false;
    }

    return {
        send: post
    };
})();
