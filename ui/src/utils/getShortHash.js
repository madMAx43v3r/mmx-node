export const getShortHash = (hash, length) => {
    if (!length) {
        length = 10;
    }
    return hash.substring(0, length) + "..." + hash.substring(64 - length);
};
