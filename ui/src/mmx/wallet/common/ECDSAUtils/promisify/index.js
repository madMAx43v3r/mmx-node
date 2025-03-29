import { syncFunctionList as functionList } from "@/mmx/wallet/common/ECDSAUtils/ECDSAUtils";
import PromisifyWorker from "./worker?worker&inline";

export const promisify = (fnName, ...args) => {
    return new Promise((resolve, reject) => {
        if (typeof window !== "undefined" && window.Worker) {
            const worker = new PromisifyWorker();
            worker.postMessage({ fnName, args });
            worker.onmessage = function (e) {
                if (e.data.fnName === fnName) resolve(e.data.result);
                if (e.data.type === "error") throw e.data.error;
            };
            worker.onerror = reject;
        } else {
            const fn = functionList[fnName];
            const argsLocal = [...args];
            if (fn) {
                if (typeof queueMicrotask !== "undefined") {
                    queueMicrotask(() => {
                        try {
                            resolve(fn(...argsLocal));
                        } catch (error) {
                            reject(error);
                        }
                    });
                } else {
                    setTimeout(() => {
                        try {
                            resolve(fn(...argsLocal));
                        } catch (error) {
                            reject(error);
                        }
                    });
                }
            }
        }
    });
};
