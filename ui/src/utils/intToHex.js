export const intToHex = (int) => {
    let hex = BigInt(int).toString(16);
    if (hex.length % 2) {
        hex = "0" + hex; // Ensure even length
    }
    return "0x" + hex;
};
