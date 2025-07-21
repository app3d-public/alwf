const Framework = (function () {
    function post(message) {
        const json = typeof message === 'string' ? message : JSON.stringify(message);

        if (window.chrome?.webview?.postMessage) {
            window.chrome.webview.postMessage(json);
            return true;
        }

        if (window.webkit?.messageHandlers?.handler?.postMessage) {
            window.webkit.messageHandlers.handler.postMessage(json);
            return true;
        }

        console.warn("No native bridge found!");
        return false;
    }

    return {
        send: post,
        click: function () {
            this.send({ handler: 'btn_click', message: 'Button clicked' });
        }
    };
})();
