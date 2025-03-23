import axiosLib from "axios";

//axiosLib.defaults.timeout = 10000;

axiosLib.defaults.paramsSerializer = (params) => {
    let result = "";
    Object.keys(params).forEach((key) => {
        if (params[key] === undefined || params[key] === null) return;
        result += `${key}=${encodeURIComponent(params[key])}&`;
    });
    return result.substring(0, result.length - 1);
};

const axios = axiosLib.create();

axios.interceptors.request.use(
    (config) => {
        if (config.url.startsWith("/wapi/")) {
            const appStore = useAppStore();
            config.baseURL = appStore.wapiBaseUrl ?? "/wapi/";
            config.url = config.url.replace("/wapi/", "/");
        }
        return config;
    },
    (error) => {
        return Promise.reject(error);
    }
);

export default axios;
