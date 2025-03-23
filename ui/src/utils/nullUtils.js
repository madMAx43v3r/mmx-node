export const NullToStr = (str) => {
    if (!str) {
        return "null";
    }
    return str;
};

export const StrToNull = (str) => {
    if (str === "null") {
        return null;
    }
    return str;
};

export const isEmpty = (value) => {
    return value == null || (typeof value === "string" && value.trim().length === 0);
};
