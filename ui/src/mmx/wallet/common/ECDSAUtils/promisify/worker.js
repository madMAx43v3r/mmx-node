import { syncFunctionList as functionList } from "@/mmx/wallet/common/ECDSAUtils/ECDSAUtils";

self.onmessage = function (e) {
    const { fnName, args } = e.data;
    //console.log(e.data);
    const fn = functionList[fnName];
    if (fn) {
        try {
            const result = fn(...args);
            self.postMessage({ fnName, result });
        } catch (error) {
            self.postMessage({ type: "error", message: error.message, error });
        }
    } else {
        self.postMessage({ type: "error", message: `Unknown fnName: ${fnName}` });
        return;
    }
};
