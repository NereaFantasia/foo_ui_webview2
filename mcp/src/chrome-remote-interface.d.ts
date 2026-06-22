declare module "chrome-remote-interface" {
    interface Target {
        type: string;
        id: string;
        title: string;
        url: string;
        webSocketDebuggerUrl: string;
    }

    interface ListOptions {
        host?: string;
        port?: number;
    }

    interface ConnectOptions {
        host?: string;
        port?: number;
        target?: Target | string;
    }

    interface EvaluateResult {
        result: {
            type: string;
            value: unknown;
            description?: string;
        };
        exceptionDetails?: {
            text: string;
            exception?: {
                description?: string;
            };
        };
    }

    interface LayoutMetrics {
        contentSize: {
            width: number;
            height: number;
        };
    }

    interface Client {
        Runtime: {
            enable(): Promise<void>;
            evaluate(params: {
                expression: string;
                awaitPromise?: boolean;
                returnByValue?: boolean;
            }): Promise<EvaluateResult>;
        };
        Page: {
            captureScreenshot(params: { format: string }): Promise<{ data: string }>;
            getLayoutMetrics(): Promise<LayoutMetrics>;
        };
        Emulation: {
            setDeviceMetricsOverride(params: {
                width: number;
                height: number;
                deviceScaleFactor: number;
                mobile: boolean;
            }): Promise<void>;
            clearDeviceMetricsOverride(): Promise<void>;
        };
        on(event: string, callback: () => void): void;
        close(): Promise<void>;
    }

    function CDP(options?: ConnectOptions): Promise<Client>;

    namespace CDP {
        function List(options?: ListOptions): Promise<Target[]>;
        type Client = import("chrome-remote-interface").Client;
    }

    export = CDP;
}
